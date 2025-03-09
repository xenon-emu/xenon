// Copyright 2025 Xenon Emulator Project

#include "PPU.h"

#include <assert.h>
#include <chrono>
#include <thread>

#include "Base/Config.h"
#include "Base/Thread.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

// Clocks per instruction / Ticks per instruction
static constexpr u64 cpi_base_freq = 50000000ull; // 50Mhz
static constexpr u64 cpi_scale = 100u; // Scale of how many clocks to speed up by divided by 100 | Default: 100
static constexpr u64 cpi_base = (cpi_base_freq / 1000000ull) * (cpi_scale / 100u);
#define EPOCH_TIME std::chrono::steady_clock::now().time_since_epoch()
#define DURATION_CAST(x, t) std::chrono::duration_cast<x>(t).count()
static constexpr u64 get_cpi_value(u64 instrPerSecond, u64 epochNs) {
  // Compute CPU cycles elapsed based on epoch time
  u64 cycles = (epochNs / 1000000000ull * cpi_base_freq) +
               ((epochNs % 1000000000ull) * cpi_base_freq / 1000000000ull) *
               (cpi_scale / 100u);
  u64 instrExecuted = (instrPerSecond * epochNs) / 1000000000ull;
  instrExecuted = instrExecuted ? instrExecuted : 1;
  return cycles / instrExecuted;
}

PPU::PPU(XENON_CONTEXT *inXenonContext, RootBus *mainBus, u32 PVR,
                  u32 PIR, const char *ppuName) {
  //
  // Set evrything as in POR. See CELL-BE Programming Handbook.
  //

  // Allocate memory for our PPU state
  ppuState = std::make_shared<STRIP_UNIQUE(ppuState)>();

  // Set PPU Name
  ppuState->ppuName = ppuName;

  // Set thread name
  Base::SetCurrentThreadName(ppuName);

  // Initialize Both threads as in a Reset
  for (u8 thrdNum = 0; thrdNum < 2; thrdNum++) {
    // Set Reset vector for both threads
    ppuState->ppuThread[thrdNum].NIA = XE_RESET_VECTOR;
    // Set MSR for both Threads
    ppuState->ppuThread[thrdNum].SPR.MSR.MSR_Hex = 0x9000000000000000;
  }

  // Set Thread Timeout Register
  ppuState->SPR.TTR = 0x1000; // Execute 4096 instructions

  // Asign global Xenon context
  xenonContext = inXenonContext;

  // Asign Interpreter global variables
  PPCInterpreter::intXCPUContext = xenonContext;
  PPCInterpreter::sysBus = mainBus;

  CalculateCPI();

  for (u8 thrdID = 0; thrdID < 2; thrdID++) {
    ppuState->ppuThread[thrdID].ppuRes = std::make_unique<STRIP_UNIQUE(PPU_THREAD_REGISTERS::ppuRes)>();
    memset(ppuState->ppuThread[thrdID].ppuRes.get(), 0, sizeof(PPU_RES));
    xenonContext->xenonRes.Register(ppuState->ppuThread[thrdID].ppuRes.get());

    // Set the decrementer as per docs. See CBE Public Registers pdf in Docs
    ppuState->ppuThread[ppuState->currentThread].SPR.DEC = 0x7FFFFFFF;
  }

  // Set PVR and PIR
  ppuState->SPR.PVR.PVR_Hex = PVR;
  ppuState->ppuThread[PPU_THREAD_0].SPR.PIR = PIR;
  ppuState->ppuThread[PPU_THREAD_1].SPR.PIR = PIR + 1;

  // PPU is running!
  ppuRunning = true;

  // TLB Software reload Mode?
  ppuState->SPR.LPCR = 0x0000000000000402;

  // HID6?
  ppuState->SPR.HID6 = 0x0001803800000000;

  // TSCR[WEXT] = 1??
  ppuState->SPR.TSCR = 0x100000;

  // If we're PPU0,thread0 then enable THRD 0 and set Reset Vector.
  if (strcmp(ppuState->ppuName, "PPU0") == false) {
    ppuState->SPR.CTRL = 0x800000; // CTRL[TE0] = 1;
    ppuState->SPR.HRMOR = 0x0000020000000000;
    ppuState->ppuThread[PPU_THREAD_0].NIA = 0x20000000100;
  }

  ppuThread = std::thread(&PPU::StartExecution, this);
  ppuThread.detach();
}
PPU::~PPU() {
  ppuRunning = false;
  // Ensure our threads are finished running
  while (!ppuExecutionDone) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ppuState.reset();
}

void PPU::CalculateCPI() {
  // Get the instructions per second that we're able to execute.
  u32 instrPerSecond = getIPS();

  // Get our CPI
  u64 cpi = get_cpi_value(instrPerSecond, DURATION_CAST(std::chrono::nanoseconds, EPOCH_TIME));

  LOG_INFO(Xenon, "{} Speed: {:#d} instructions per second.", ppuState->ppuName, instrPerSecond);

  // Find a way to calculate the right ticks/IPS ratio.
  int configCpi = Config::cpi();
  clocksPerInstruction = configCpi ? configCpi : cpi;
  if (!configCpi)
    LOG_INFO(Xenon, "{} CPI: {} clocks per instruction", ppuState->ppuName, clocksPerInstruction);
  else
    LOG_INFO(Xenon, "{} CPI: {} clocks per instruction (overrwriten! actual CPI: {})", ppuState->ppuName, clocksPerInstruction, cpi);
}

void PPU::Reset() {
  // Zero out the memory
  PPCInterpreter::MMUWrite(xenonContext, ppuState.get(), 4, XE_SRAM_ADDR, 32);

  // Set the NIP back to default
  ppuState->ppuThread[ppuState->currentThread].NIA = 0x100;

  // Reset the registers
  memset(ppuState->ppuThread[ppuState->currentThread].GPR, 0, sizeof(ppuState->ppuThread[ppuState->currentThread].GPR));
}

void PPU::Halt() {
  ppcHalt = true;
}
void PPU::Continue() {
  ppcHalt = false;
}
void PPU::Step(int amount) {
  ppcStepAmount = amount;
  ppcStep = true;
}

// PPU Entry Point.
void PPU::StartExecution() {
  // While the CPU is running
  while (ppuRunning) {
    // See if we have any threads active.
    while (ppuRunning && getCurrentRunningThreads() != PPU_THREAD_NONE) {
      ppuExecutionDone = false;
      // We have some threads active!

      // Check if the 1st thread is active and process instructions on it.
      if (getCurrentRunningThreads() == PPU_THREAD_0 ||
          getCurrentRunningThreads() == PPU_THREAD_BOTH) {
        // Thread 0 is running, process instructions until we reach TTR timeout.
        ppuState->currentThread = PPU_THREAD_0;

        // Loop on this thread for the amount of Instructions that TTR tells us.
        for (size_t instrCount = 0; instrCount < ppuState->SPR.TTR;
             instrCount++) {
          // Main processing loop.

          // Debug tools
          if (ppcHalt) {
            if (ppcStep) {
              if (ppcStepCounter != ppcStepAmount) {
                ppcStepCounter++;
              }
              else {
                ppcStep = false;
              }
            }
            while (ppcHalt && !ppcStep) {
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
          }

          // Read next intruction from Memory.
          if (ppuReadNextInstruction()) {
            // Execute next intrucrtion.
            PPCInterpreter::ppcExecuteSingleInstruction(ppuState.get());
          }

          // Increase Time Base Counter
          if (xenonContext->timeBaseActive) {
            // HID6[15]: Time-base and decrementer facility enable.
            // 0 -> TBU, TBL, DEC, HDEC, and the hang-detection logic do not
            // update. 1 -> TBU, TBL, DEC, HDEC, and the hang-detection logic
            // are enabled to update.
            if (ppuState->SPR.HID6 & 0x1000000000000) {
              updateTimeBase();
            }
          }

          // Check if External interrupts are enabled and the IIC has a pending
          // interrupt.
          if (ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE) {
            if (xenonContext->xenonIIC.checkExtInterrupt(
                    ppuState->ppuThread[ppuState->currentThread].SPR.PIR)) {
              ppuState->ppuThread[ppuState->currentThread].exceptReg |=
                  PPU_EX_EXT;
            }
          }

          // Check Exceptions pending.
          ppuCheckExceptions();
        }
      }
      // Check again for the 2nd thread.
      if (getCurrentRunningThreads() == PPU_THREAD_1 ||
          getCurrentRunningThreads() == PPU_THREAD_BOTH) {
        // Thread 0 is running, process instructions until we reach TTR timeout.
        ppuState->currentThread = PPU_THREAD_1;

        // Loop on this thread for the amount of Instructions that TTR tells us.
        for (size_t instrCount = 0; instrCount < ppuState->SPR.TTR;
             instrCount++) {
          // Main processing loop.
          
          // Debug tools
          if (ppcHalt) {
            if (ppcStep) {
              if (ppcStepCounter != ppcStepAmount) {
                ppcStepCounter++;
              }
              else {
                ppcStep = false;
              }
            }
            while (ppcHalt && !ppcStep) {
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
          }

          // Read next intruction from Memory.
          if (ppuReadNextInstruction()) {
            // Execute next intrucrtion.
            PPCInterpreter::ppcExecuteSingleInstruction(ppuState.get());
          }

          // Check if time base is active.
          if (xenonContext->timeBaseActive) {
            // HID6[15]: Time-base and decrementer facility enable.
            // 0 -> TBU, TBL, DEC, HDEC, and the hang-detection logic do not
            // update. 1 -> TBU, TBL, DEC, HDEC, and the hang-detection logic
            // are enabled to update.
            if (ppuState->SPR.HID6 & 0x1000000000000) {
              updateTimeBase();
            }
          }

          // Check if External interrupts are enabled and the IIC has a pending
          // interrupt.
          if (ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE) {
            if (xenonContext->xenonIIC.checkExtInterrupt(
                    ppuState->ppuThread[ppuState->currentThread].SPR.PIR)) {
              ppuState->ppuThread[ppuState->currentThread].exceptReg |=
                  PPU_EX_EXT;
            }
          }

          // Check Exceptions pending.
          ppuCheckExceptions();
        }
      }
      ppuExecutionDone = true;
    }
    if (!ppuState.get())
      break;

    //
    // Check for external interrupts that enable us if we're allowed to.
    //

    // If TSCR[WEXT] = '1', wake up at System Reset and set SRR1[42:44] = '100'.
    bool WEXT = (ppuState->SPR.TSCR & 0x100000) >> 20;
    if (xenonContext->xenonIIC.checkExtInterrupt(
            ppuState->ppuThread[ppuState->currentThread].SPR.PIR) &&
        WEXT) {
      // Great, someone started us! Let's enable THRD0.
      ppuState->SPR.CTRL = 0x800000;
      // Issue reset!
      ppuState->ppuThread[PPU_THREAD_0].exceptReg |= PPU_EX_RESET;
      ppuState->ppuThread[PPU_THREAD_1].exceptReg |=
          PPU_EX_RESET; // Set CIA to 0x100 as per docs.
      ppuState->ppuThread[ppuState->currentThread].SPR.SRR1 =
          0x200000; // Set SRR1 42-44 = 100
      // ACK and EOI the interrupt:
      u64 intData = 0;
      xenonContext->xenonIIC.readInterrupt(
        ppuState->ppuThread[ppuState->currentThread].SPR.PIR * 0x1000 + 0x50050, &intData);
      xenonContext->xenonIIC.writeInterrupt(
        ppuState->ppuThread[ppuState->currentThread].SPR.PIR * 0x1000 + 0x50060, 0);
    }
  }
}

// Returns a pointer to the specified thread.
PPU_THREAD_REGISTERS *PPU::GetPPUThread(u8 thrdID) {
  return &this->ppuState->ppuThread[thrdID];
}

// This is the calibration code for the getIPS() function. It branches to the
// 0x4 location in memory.
static u32 ipsCalibrationCode[] = {
    0x55726220, //  rlwinm   r18,r11,12,8,16
    0x723D7825, //  andi.    r29,r17,0x7825
    0x65723D78, //  oris     r18,r11,0x3D78
    0x4BFFFFF4  //  b        ipsCalibrationCode
};

// Performs a test using a loop to check the amount of IPS we're able to
// execute
u32 PPU::getIPS() {
  // Instr Count: The amount of instructions to execute in order to test

  // Write the calibration code to main memory
  for (int i = 0; i < 4; i++) {
    PPCInterpreter::MMUWrite32(ppuState.get(), 4 + (i * 4), ipsCalibrationCode[i]);
  }

  // Set our NIP to our calibration code address
  ppuState->ppuThread[ppuState->currentThread].NIA = 4;

  // Start a timer
  auto timerStart = std::chrono::steady_clock::now();

  // Instruction count
  u64 instrCount = 0;

  // Execute the amount of cycles we're requested
  while (auto timerEnd = std::chrono::steady_clock::now() <=
                         timerStart + std::chrono::seconds(1)) {
    ppuReadNextInstruction();
    PPCInterpreter::ppcExecuteSingleInstruction(ppuState.get());
    instrCount++;
  }

  // Reset our state
  
  // Zero out the memory after execution
  for (int i = 0; i < 4; i++) {
    PPCInterpreter::MMUWrite32(ppuState.get(), 4 + (i * 4), 0x00000000);
  }

  // Set the NIP back to default
  ppuState->ppuThread[ppuState->currentThread].NIA = 0x100;

  // Reset the registers
  for (int i = 0; i < 32; i++) {
    ppuState->ppuThread[ppuState->currentThread].GPR[i] = 0;
  }

  return instrCount;
}

// Reads the next instruction from memory and advances the NIP accordingly.
bool PPU::ppuReadNextInstruction() {
  // Update CIA.
  ppuState->ppuThread[ppuState->currentThread].CIA =
      ppuState->ppuThread[ppuState->currentThread].NIA;
  // Increase Next Instruction Address.
  ppuState->ppuThread[ppuState->currentThread].NIA += 4;
  ppuState->ppuThread[ppuState->currentThread].iFetch = true;
  // Fetch the instruction from memory.
  ppuState->ppuThread[ppuState->currentThread].CI.opcode = PPCInterpreter::MMURead32(
      ppuState.get(), ppuState->ppuThread[ppuState->currentThread].CIA);
  if (ppuState->ppuThread[ppuState->currentThread].exceptReg & PPU_EX_INSSTOR ||
      ppuState->ppuThread[ppuState->currentThread].exceptReg &
          PPU_EX_INSTSEGM) {
    return false;
  }
  ppuState->ppuThread[ppuState->currentThread].iFetch = false;
  return true;
}

// Checks for exceptions and process them in the correct order.
void PPU::ppuCheckExceptions() {
  // Check Exceptions pending and process them in order.
  u16 exceptions = ppuState->ppuThread[ppuState->currentThread].exceptReg;
  if (exceptions != PPU_EX_NONE) {
    // Non Maskable:

    //
    // 1. System Reset
    //
    if (exceptions & PPU_EX_RESET) {
      PPCInterpreter::ppcResetException(ppuState.get());
      exceptions &= ~PPU_EX_RESET;
      goto end;
    }

    //
    // 2. Machine Check
    //
    if (exceptions & PPU_EX_MC) {
      if (ppuState->ppuThread[ppuState->currentThread].SPR.MSR.ME) {
        PPCInterpreter::ppcResetException(ppuState.get());
        exceptions &= ~PPU_EX_MC;
        goto end;
      } else {
        // Checkstop Mode. Hard Fault.
        LOG_CRITICAL(Xenon, "{}: CHECKSTOP!", ppuState->ppuName);
        // TODO: Properly end execution.
        // A checkstop is a full - stop of the processor that requires a System
        // Reset to recover.
        SYSTEM_PAUSE();
      }
    }

    // Maskable

    //
    // 3. Instruction-Dependent
    //

    // A. Program - Illegal Instruction
    if (exceptions & PPU_EX_PROG &&
        ppuState->ppuThread[ppuState->currentThread].exceptTrapType == 44) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Illegal Instruction.",
          ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_PROG;
      goto end;
    }
    // B. Floating-Point Unavailable
    if (exceptions & PPU_EX_FPU) {
        LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Floating-Point Unavailable.",
            ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_FPU;
      goto end;
    }
    // C. Data Storage, Data Segment, or Alignment
    // Data Storage
    if (exceptions & PPU_EX_DATASTOR) {
      PPCInterpreter::ppcDataStorageException(ppuState.get());
      exceptions &= ~PPU_EX_DATASTOR;
      goto end;
    }
    // Data Segment
    if (exceptions & PPU_EX_DATASEGM) {
      PPCInterpreter::ppcDataSegmentException(ppuState.get());
      exceptions &= ~PPU_EX_DATASEGM;
      goto end;
    }
    // Alignment
    if (exceptions & PPU_EX_ALIGNM) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Alignment.",
          ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_ALIGNM;
      goto end;
    }
    // D. Trace
    if (exceptions & PPU_EX_TRACE) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Trace.",
          ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_TRACE;
      goto end;
    }
    // E. Program Trap, System Call, Program Priv Inst, Program Illegal Inst
    // Program Trap
    if (exceptions & PPU_EX_PROG &&
        ppuState->ppuThread[ppuState->currentThread].exceptTrapType == 46) {
      PPCInterpreter::ppcProgramException(ppuState.get());
      exceptions &= ~PPU_EX_PROG;
      goto end;
    }
    // System Call
    if (exceptions & PPU_EX_SC) {
      PPCInterpreter::ppcSystemCallException(ppuState.get());
      exceptions &= ~PPU_EX_SC;
      goto end;
    }
    // Program - Privileged Instruction
    if (exceptions & PPU_EX_PROG &&
        ppuState->ppuThread[ppuState->currentThread].exceptTrapType == 45) {
        LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Privileged Instruction.",
            ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_PROG;
      goto end;
    }
    // F. Instruction Storage and Instruction Segment
    // Instruction Storage
    if (exceptions & PPU_EX_INSSTOR) {
      PPCInterpreter::ppcInstStorageException(ppuState.get());
      exceptions &= ~PPU_EX_INSSTOR;
      goto end;
    }
    // Instruction Segment
    if (exceptions & PPU_EX_INSTSEGM) {
      PPCInterpreter::ppcInstSegmentException(ppuState.get());
      exceptions &= ~PPU_EX_INSTSEGM;
      goto end;
    }

    //
    // 4. Program - Imprecise Mode Floating-Point Enabled Exception
    //

    if (exceptions & PPU_EX_PROG) {
        LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Imprecise Mode Floating-Point Enabled Exception.",
            ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_PROG;
      goto end;
    }

    //
    // 5. External, Decrementer, and Hypervisor Decrementer
    //

    // External
    if (exceptions & PPU_EX_EXT &&
        ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE) {
      PPCInterpreter::ppcExternalException(ppuState.get());
      exceptions &= ~PPU_EX_EXT;
      goto end;
    }
    // Decrementer. A dec exception may be present but will only be taken when
    // the EE bit of MSR is set.
    if (exceptions & PPU_EX_DEC &&
        ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE) {
      PPCInterpreter::ppcDecrementerException(ppuState.get());
      exceptions &= ~PPU_EX_DEC;
      goto end;
    }
    // Hypervisor Decrementer
    if (exceptions & PPU_EX_HDEC) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Hypervisor Decrementer.",
          ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_HDEC;
      goto end;
    }

    // Set the new value for our exception register.
  end:
    ppuState->ppuThread[ppuState->currentThread].exceptReg = exceptions;
  }
}

// Updates the time base based on the amount of ticks and checks for decrementer
// interrupts if enabled.
void PPU::updateTimeBase() {
  // The Decrementer and the Time Base are driven by the same time frequency.
  u32 newDec = 0;
  u32 dec = 0;
  // Update the Time Base.
  ppuState->SPR.TB += clocksPerInstruction;
  // Get the decrementer value.
  dec = ppuState->ppuThread[ppuState->currentThread].SPR.DEC;
  newDec = dec - clocksPerInstruction;
  // Update the new decrementer value.
  ppuState->ppuThread[ppuState->currentThread].SPR.DEC = newDec;
  // Check if Previous decrementer measurement is smaller than current and a
  // decrementer exception is not pending.
  if (newDec > dec && ((ppuState->ppuThread[ppuState->currentThread].exceptReg &
                        PPU_EX_DEC) == 0)) {
    // The decrementer must issue an interrupt.
    ppuState->ppuThread[ppuState->currentThread].exceptReg |= PPU_EX_DEC;
  }
}

// Returns current executing thread by reading CTRL register.
PPU_THREAD PPU::getCurrentRunningThreads() {
  // If we have shutdown before current running threads is finished, force a shutdown
  if (!ppuState.get()) {
    return PPU_THREAD_NONE;
  }
  // Check CTRL Register CTRL>TE[0,1];
  u8 ctrlTE = (ppuState->SPR.CTRL & 0xC00000) >> 22;
  switch (ctrlTE) {
  case 0b10:
    return PPU_THREAD_0;
  case 0b01:
    return PPU_THREAD_1;
  case 0b11:
    return PPU_THREAD_BOTH;
  default:
    return PPU_THREAD_NONE;
  }
}
