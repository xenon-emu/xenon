// Copyright 2025 Xenon Emulator Project

#include "PPU.h"

#include <assert.h>
#include <chrono>
#include <iostream>
#include <thread>

#include "Core/Xe_Main.h"
#include "Base/Config.h"
#include "Base/Thread.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/elf_abi.h"

// Clocks per instruction / Ticks per instruction
static constexpr f64 cpi_base_freq = 50000000ull; // 50Mhz
static constexpr f64 cpi_scale = 1.00; // Scale of how many clocks to speed up in percentage of speed
static constexpr u64 get_cpi_value(u64 instrPerSecond) {
  u64 tpi = 0;
  // Use floating point for a more percise CPI
  f64 cpi_value = (instrPerSecond / 100000ull) / ((cpi_base_freq / 1000000ull) * cpi_scale);
  tpi = static_cast<u64>(cpi_value);
  // Round up
  if ((cpi_value - static_cast<f64>(tpi)) >= 0.5 || tpi == 0)
    tpi++;
  return tpi;
}

PPU::PPU(XENON_CONTEXT *inXenonContext, RootBus *mainBus, u64 resetVector, u32 PVR,
                  u32 PIR) :
  resetVector(resetVector) {
  //
  // Set evrything as in POR. See CELL-BE Programming Handbook.
  //

  // Allocate memory for our PPU state
  ppuState = std::make_shared<STRIP_UNIQUE(ppuState)>();

  // Set PPU ID (PIR, as 0 indexed. So 0-4)
  ppuState->ppuID = PIR / 2;

  // Set PPU Name
  ppuState->ppuName = fmt::format("PPU{}", ppuState->ppuID);

  // Set thread name
  Base::SetCurrentThreadName(ppuState->ppuName.data());

  // Initialize Both threads as in a Reset
  for (u8 thrdNum = 0; thrdNum < 2; thrdNum++) {
    // Set Reset vector for both threads
    ppuState->ppuThread[thrdNum].NIA = XE_RESET_VECTOR;
    // Set MSR for both Threads
    ppuState->ppuThread[thrdNum].SPR.MSR.MSR_Hex = 0x9000000000000000;
  }
  #define LogMSR(x) LOG_INFO(Xenon, "MSR.{}: 0x{:02x}", #x, ppuState->ppuThread[0].SPR.MSR.x);
  LogMSR(MSR_Hex);
  LogMSR(LE);
  LogMSR(RI);
  LogMSR(PMM);
  LogMSR(DR);
  LogMSR(IR);
  LogMSR(FE1);
  LogMSR(BE);
  LogMSR(SE);
  LogMSR(FE0);
  LogMSR(ME);
  LogMSR(FP);
  LogMSR(PR);
  LogMSR(EE);
  LogMSR(ILE);
  LogMSR(VXU);
  LogMSR(HV);
  LogMSR(TA);
  LogMSR(SF);
  SYSTEM_PAUSE();

  // Set Thread Timeout Register
  ppuState->SPR.TTR = 0x1000; // Execute 4096 instructions

  // Asign global Xenon context
  xenonContext = inXenonContext;

  // Asign Interpreter global variables
  PPCInterpreter::intXCPUContext = xenonContext;
  PPCInterpreter::sysBus = mainBus;

  CalculateCPI();

  // If we want to start halted, halt after CPI is done.
  ppuStartHalted = Config::startCPUHalted();
  ppuHalt = Config::startCPUHalted();

  // If we have a specific halt address, set it here
  ppuHaltOn = Config::haltOn();

  for (u8 thrdID = 0; thrdID < 2; thrdID++) {
    ppuState->ppuThread[thrdID].ppuRes = std::make_unique<STRIP_UNIQUE(PPU_THREAD_REGISTERS::ppuRes)>();
    memset(ppuState->ppuThread[thrdID].ppuRes.get(), 0, sizeof(PPU_RES));
    xenonContext->xenonRes.Register(ppuState->ppuThread[thrdID].ppuRes.get());

    // Set the decrementer as per docs. See CBE Public Registers pdf in Docs
    curThread.SPR.DEC = 0x7FFFFFFF;

    // Resize ERAT's
    curThread.iERAT.resizeCache(64);
    curThread.dERAT.resizeCache(64);
  }

  // Set PVR and PIR
  ppuState->SPR.PVR.PVR_Hex = PVR;
  ppuState->ppuThread[PPU_THREAD_0].SPR.PIR = PIR;
  ppuState->ppuThread[PPU_THREAD_1].SPR.PIR = PIR + 1;
}
PPU::~PPU() {
  ppuRunning = false;
  // Ensure our threads are finished running
  while (!ppuExecutionDone) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ppuState.reset();
}

void PPU::StartExecution(bool setHRMOR) {
  // PPU is running!
  ppuRunning = true;

  // TLB Software reload Mode?
  ppuState->SPR.LPCR = 0x0000000000000402;

  // HID6?
  ppuState->SPR.HID6 = 0x0001803800000000;

  // TSCR[WEXT] = 1??
  ppuState->SPR.TSCR = 0x100000;

  // If we're PPU0,thread0 then enable THRD 0 and set Reset Vector.
  if (ppuState->ppuID == 0 && setHRMOR) {
    ppuState->SPR.CTRL = 0x800000; // CTRL[TE0] = 1;
    ppuState->SPR.HRMOR = 0x0000020000000000;
    ppuState->ppuThread[PPU_THREAD_0].NIA = resetVector;
  }

  ppuThread = std::thread(&PPU::Thread, this);
  ppuThread.detach();
}

void PPU::CalculateCPI() {
  // Get the instructions per second that we're able to execute.
  u32 instrPerSecond = getIPS();

  // Get our CPI
  u64 cpi = get_cpi_value(instrPerSecond);

  LOG_INFO(Xenon, "{} Speed: {:#d} instructions per second.", ppuState->ppuName, instrPerSecond);

  // Find a way to calculate the right ticks/IPS ratio.
  int configCpi = Config::cpi();
  clocksPerInstruction = configCpi ? configCpi : cpi;
  if (!configCpi)
    LOG_INFO(Xenon, "{} CPI: {} clocks per instruction", ppuState->ppuName, clocksPerInstruction);
  else
    LOG_INFO(Xenon, "{} CPI: {} clocks per instruction (Overwritten! Actual CPI: {})", ppuState->ppuName, clocksPerInstruction, cpi);
}

void PPU::Reset() {
  // Zero out the memory
  PPCInterpreter::MMUWrite(xenonContext, ppuState.get(), 0, 4, 32);

  // Set the NIP back to default
  curThread.NIA = 0x100;

  // Reset the registers
  memset(curThread.GPR, 0, sizeof(curThread.GPR));
}

void PPU::Halt(u64 haltOn) {
  if (haltOn != 0)
    LOG_DEBUG(Xenon, "Halting PPU{} on address {:#x}", ppuState->ppuID, haltOn);
  ppuHaltOn = haltOn;
  ppuHalt = true;
}
void PPU::Continue() {
  LOG_DEBUG(Xenon, "Continuing execution on PPU{}", ppuState->ppuID);
  ppuHaltOn = 0;
  ppuHalt = false;
}
void PPU::Step(int amount) {
  LOG_DEBUG(Xenon, "Continuing PPU{} for {} Instructions", ppuState->ppuID, amount);
  ppuHaltOn = 0;
  ppuStepAmount = amount;
  ppuStep = true;
}

// PPU Entry Point.
void PPU::Thread() {
  // While the CPU is running
  while (ThreadRunning()) {
    // See if we have any threads active.
    while (ThreadRunning() && getCurrentRunningThreads() != PPU_THREAD_NONE) {
      ppuExecutionDone = false;
      // We have some threads active!

      // Check if the 1st thread is active and process instructions on it.
      if (getCurrentRunningThreads() == PPU_THREAD_0 ||
          getCurrentRunningThreads() == PPU_THREAD_BOTH) {
        // Thread 0 is running, process instructions until we reach TTR timeout.
        curThreadId = PPU_THREAD_0;

        // Loop on this thread for the amount of Instructions that TTR tells us.
        for (size_t instrCount = 0; instrCount < ppuState->SPR.TTR;
             instrCount++) {
          // Main processing loop.

          // Read next intruction from Memory.
          if (ThreadRunning() && ppuReadNextInstruction()) {
            // Execute next intrucrtion.
            PPCInterpreter::ppcExecuteSingleInstruction(ppuState.get());
          }
          
          // Debug tools
          if (ppuHalt) {
            bool shouldHalt = ppuHalt && (ppuStartHalted ? ppuHaltOn != 0 && ppuHaltOn == curThread.CIA : true);
            if (ppuStep) {
              if (ppuStepCounter != ppuStepAmount) {
                ppuStepCounter++;
              }
              else {
                ppuStep = false;
              }
            }
            while (ppuHalt && !ppuStep) {
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
          }

          // We early returned, likely a elf binary
          if (curThread.CIA == 0x00 && curThread.NIA == 0x04 && ppuElfExecution) {
            ppuRunning = false;
            ppuExecutionDone = true;
            break;
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
          if (curThread.SPR.MSR.EE) {
            if (xenonContext->xenonIIC.checkExtInterrupt(curThread.SPR.PIR)) {
              _ex |= PPU_EX_EXT;
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
        curThreadId = PPU_THREAD_1;

        // Loop on this thread for the amount of Instructions that TTR tells us.
        for (size_t instrCount = 0; instrCount < ppuState->SPR.TTR;
             instrCount++) {
          // Main processing loop.

          // Read next intruction from Memory.
          if (ThreadRunning() && ppuReadNextInstruction()) {
            // Execute next intrucrtion.
            PPCInterpreter::ppcExecuteSingleInstruction(ppuState.get());
          }
          
          // Debug tools
          if (ppuHalt) {
            bool shouldHalt = ppuHalt && (ppuStartHalted ? ppuHaltOn != 0 && ppuHaltOn == curThread.CIA : true);
            if (ppuStep) {
              if (ppuStepCounter != ppuStepAmount) {
                ppuStepCounter++;
              }
              else {
                ppuStep = false;
              }
            }
            while (ppuHalt && !ppuStep) {
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
          }

          // We early returned, likely a elf binary
          if (curThread.CIA == 0x00 && curThread.NIA == 0x04 && ppuElfExecution) {
            ppuRunning = false;
            ppuExecutionDone = true;
            break;
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
          if (curThread.SPR.MSR.EE) {
            if (xenonContext->xenonIIC.checkExtInterrupt(curThread.SPR.PIR)) {
              _ex |= PPU_EX_EXT;
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

    // We early returned, likely a elf binary
    if (curThread.CIA == 0x00 && curThread.NIA == 0x04 && ppuElfExecution) {
      ppuRunning = false;
      ppuExecutionDone = true;
      break;
    }

    //
    // Check for external interrupts that enable us if we're allowed to.
    //

    // If TSCR[WEXT] = '1', wake up at System Reset and set SRR1[42:44] = '100'.
    bool WEXT = (ppuState->SPR.TSCR & 0x100000) >> 20;
    if (xenonContext->xenonIIC.checkExtInterrupt(curThread.SPR.PIR) && WEXT) {
      // Great, someone started us! Let's enable THRD0.
      ppuState->SPR.CTRL = 0x800000;
      // Issue reset!
      ppuState->ppuThread[PPU_THREAD_0].exceptReg |= PPU_EX_RESET;
      ppuState->ppuThread[PPU_THREAD_1].exceptReg |= PPU_EX_RESET;
      curThread.SPR.SRR1 = 0x200000; // Set SRR1 42-44 = 100
      // ACK and EOI the interrupt:
      u64 intData = 0;
      xenonContext->xenonIIC.readInterrupt(curThread.SPR.PIR * 0x1000 + 0x50050, &intData);
      xenonContext->xenonIIC.writeInterrupt(curThread.SPR.PIR * 0x1000 + 0x50060, 0);
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
  curThread.NIA = 4;

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
  curThread.NIA = 0x100;

  // Reset the registers
  for (int i = 0; i < 32; i++) {
    curThread.GPR[i] = 0;
  }

  return instrCount;
}

// Loads a elf binary at a specificed address
// Returns entrypoint
#define IS_ELF(ehdr) ((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
                      (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
                      (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
                      (ehdr).e_ident[EI_MAG3] == ELFMAG3)
#define SWAP(v, s) v = byteswap_be<s>(v);
u64 PPU::loadElfImage(u8 *data, u64 size) {
  ppuElfExecution = true;
  elf64_hdr* header{ reinterpret_cast<decltype(header)>(data) };
  if (!IS_ELF(*header)) {
    LOG_CRITICAL(Xenon, "Attempting to load a binary which is not a elf!");
    return 0;
  }
  if (header->e_ident[EI_DATA] != 2) {
    LOG_CRITICAL(Xenon, "Attempting to load a binary which is not a elf64!");
    return 0;
  }
  SWAP(header->e_type, u16);
  SWAP(header->e_machine, u16);
  if (header->e_machine != 0x15) {
    LOG_CRITICAL(Xenon, "Attempting to load a binary which is not a PowerPC 64!");
    return 0;
  }

  // Setup HRMOR for elf binaries
  ppuState->SPR.CTRL = 0x800000; // CTRL[TE0] = 1;
  ppuState->SPR.HRMOR = 0x0000000000000000;

  SWAP(header->e_version, u16);
  SWAP(header->e_entry, u64);
  SWAP(header->e_phoff, u64);
  SWAP(header->e_shoff, u64);
  SWAP(header->e_flags, u32);
  SWAP(header->e_ehsize, u16);
  SWAP(header->e_phentsize, u16);
  SWAP(header->e_phnum, u16);
  SWAP(header->e_shentsize, u16);
  SWAP(header->e_shnum, u16);
  SWAP(header->e_shstrndx, u16);

  u64 entry_point = header->e_entry;

  const u16 num_psections = header->e_phnum;

  const Elf64_Phdr* psections = reinterpret_cast<Elf64_Phdr*>(data + header->e_phoff);

  for (size_t i = 0; i < num_psections; i++) {
    if (byteswap_be<u32>(psections[i].p_type) == PT_LOAD) {
      u64 vaddr = byteswap_be<u64>(psections[i].p_vaddr);
      u64 paddr = byteswap_be<u64>(psections[i].p_paddr);
      u64 filesize = byteswap_be<u64>(psections[i].p_filesz);
      u64 memsize = byteswap_be<u64>(psections[i].p_memsz);
      u64 file_offset = byteswap_be<u64>(psections[i].p_offset);
      bool physical_load = true;
      u64 target_addr = physical_load ? paddr : vaddr;
      LOG_INFO(Xenon, "Loading {:#x} bytes from offset {:#x} in the ELF to {:#x}", filesize, file_offset, target_addr);
      PPCInterpreter::MMUMemCpyFromHost(ppuState.get(), target_addr, data + file_offset, filesize);
      if (memsize > filesize) { // If the memory size greater than the file, zero out the remainder
        u64 remainder = memsize - filesize;
        LOG_DEBUG(Xenon, "Zeroing {:#x} bytes at addr {:#x}", remainder, target_addr + filesize);
        PPCInterpreter::MMUWrite(xenonContext, ppuState.get(), 0, target_addr + filesize, remainder);
      }
    }
  }

  LOG_INFO(Xenon, "Done loading elf, entry-point is {:#x}", entry_point);

  curThread.NIA = entry_point;

  return curThread.NIA;
}

// Reads the next instruction from memory and advances the NIP accordingly.
bool PPU::ppuReadNextInstruction() {
  if (ThreadRunning() && ((curThread.CIA > 0xCDCDC00000000000) && ((curThread.CIA - 0xCDCDC00000000000) > 0) || (curThread.NIA == 0 && ppuState->ppuID == 0))) {
    LOG_CRITICAL(Xenon, "PPU{} was unable to get the next instruction! Halting...", ppuState->ppuID);
    Xe_Main->getCPU()->Halt(); // Halt CPU
    Config::imguiDebugWindow = true; // Open the debugger on bad fault
    return false;
  }
  // Update current instruction address
  curThread.CIA = curThread.NIA;
  // Increase next instruction address
  curThread.NIA += 4;
  curThread.iFetch = true;
  // Fetch the instruction from memory
  _instr.opcode = PPCInterpreter::MMURead32(ppuState.get(), curThread.CIA);
  u8 first_byte = (_instr.opcode >> 24) & 0xFF;
  u8 last_byte = (_instr.opcode >> 0) & 0xFF;
  if (_instr.opcode == 0xFFFFFFFF || (first_byte == 0x00 && last_byte != 0x00)) {
    LOG_CRITICAL(Xenon, "PPU{} returned a invalid opcode! Halting...", ppuState->ppuID);
    Xe_Main->getCPU()->Halt(); // Halt CPU
    Config::imguiDebugWindow = true; // Open the debugger on bad fault
    return false;
  }
  if (_ex & PPU_EX_INSSTOR || _ex & PPU_EX_INSTSEGM) {
    return false;
  }
  curThread.iFetch = false;
  return true;
}

// Checks for exceptions and process them in the correct order.
void PPU::ppuCheckExceptions() {
  // Check Exceptions pending and process them in order.
  u16 exceptions = _ex;
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
      if (curThread.SPR.MSR.ME) {
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
    if (exceptions & PPU_EX_PROG && curThread.exceptTrapType == 44) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Illegal Instruction.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_PROG;
      goto end;
    }
    // B. Floating-Point Unavailable
    if (exceptions & PPU_EX_FPU) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Floating-Point Unavailable.", ppuState->ppuName, static_cast<u8>(curThreadId));
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
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Alignment.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_ALIGNM;
      goto end;
    }
    // D. Trace
    if (exceptions & PPU_EX_TRACE) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Trace.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_TRACE;
      goto end;
    }
    // E. Program Trap, System Call, Program Priv Inst, Program Illegal Inst
    // Program Trap
    if (exceptions & PPU_EX_PROG && curThread.exceptTrapType == 46) {
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
      curThread.exceptTrapType == 45) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Privileged Instruction.", ppuState->ppuName, static_cast<u8>(curThreadId));
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
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Imprecise Mode Floating-Point Enabled Exception.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_PROG;
      goto end;
    }

    //
    // 5. External, Decrementer, and Hypervisor Decrementer
    //

    // External
    if (exceptions & PPU_EX_EXT && curThread.SPR.MSR.EE) {
      PPCInterpreter::ppcExternalException(ppuState.get());
      exceptions &= ~PPU_EX_EXT;
      goto end;
    }
    // Decrementer. A dec exception may be present but will only be taken when
    // the EE bit of MSR is set.
    if (exceptions & PPU_EX_DEC && curThread.SPR.MSR.EE) {
      PPCInterpreter::ppcDecrementerException(ppuState.get());
      exceptions &= ~PPU_EX_DEC;
      goto end;
    }
    // Hypervisor Decrementer
    if (exceptions & PPU_EX_HDEC) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Hypervisor Decrementer.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_HDEC;
      goto end;
    }

    // Set the new value for our exception register.
  end:
    _ex = exceptions;
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
  dec = curThread.SPR.DEC;
  newDec = dec - clocksPerInstruction;
  // Update the new decrementer value.
  curThread.SPR.DEC = newDec;
  // Check if Previous decrementer measurement is smaller than current and a
  // decrementer exception is not pending.
  if (newDec > dec && ((_ex & PPU_EX_DEC) == 0)) {
    // The decrementer must issue an interrupt.
    _ex |= PPU_EX_DEC;
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
