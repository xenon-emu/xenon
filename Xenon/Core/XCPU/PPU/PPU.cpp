// Copyright 2025 Xenon Emulator Project

#include "PPU.h"

#include <assert.h>
#include <chrono>
#include <thread>

#include "Core/Xe_Main.h"
#include "Base/Config.h"
#include "Base/Thread.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/elf_abi.h"

// Clocks per instruction / Ticks per instruction
static constexpr f64 cpi_a = -5.8868;
static constexpr f64 cpi_b = 0.26415;
static constexpr f64 cpi_c = 217.1;

static constexpr u64 get_cpi_value(const u64 instrPerSecond) {
  // Convert IPS to MIPS
  f64 x = static_cast<f64>(instrPerSecond) / 1'000'000.0;

  // Compute CPI: y = a + c / (x + b)
  f64 cpif = cpi_a + (cpi_c / (x + cpi_b));

  u64 cpi = static_cast<u64>(cpif);

  return cpi == 0 ? 1 : cpi;
}

PPU::PPU(XENON_CONTEXT *inXenonContext, RootBus *mainBus, u64 resetVector, u32 PVR,
                  u32 PIR) :
  resetVector(resetVector)
{
  traceFile = NULL;
#ifdef DEBUG_BUILD
  if (Config::debug.createTraceFile) {
    char path[128];
    snprintf(path, 127, "trace_%d.log", PIR);
    traceFile = fopen(path, "w");
  }
#endif

  //
  // Set everything as in POR. See CELL-BE Programming Handbook
  //

  if (ppuThreadState.load() == eThreadState::Unused) {
    return;
  }

  // Allocate memory for our PPU state
  ppuState = std::make_unique<STRIP_UNIQUE(ppuState)>();

  // Set PPU ID (PIR, as 0 indexed. So 0-6)
  ppuState->ppuID = PIR / 2;

  // Set PPU Name
  ppuState->ppuName = fmt::format("PPU{}", ppuState->ppuID);

  // Initialize Both threads as in a Reset
  for (u8 thrdNum = 0; thrdNum < 2; thrdNum++) {
    // Set Reset vector for both threads
    ppuState->ppuThread[static_cast<ePPUThread>(thrdNum)].NIA = XE_RESET_VECTOR;
    // Set MSR for both Threads
    ppuState->ppuThread[static_cast<ePPUThread>(thrdNum)].SPR.MSR.MSR_Hex = 0x9000000000000000;
  }

  // Set Thread Timeout Register
  ppuState->SPR.TTR = 0x1000; // Execute 4096 instructions

  // Asign global Xenon context
  xenonContext = inXenonContext;

  if (Config::xcpu.clocksPerInstruction) {
    clocksPerInstruction = Config::xcpu.clocksPerInstruction;
    LOG_INFO(Xenon, "{}: Using cached CPI from Config, got {}", ppuState->ppuName, clocksPerInstruction);
  } else {
    CalculateCPI();
    if (ppuState->ppuID == 0) {
      Config::xcpu.clocksPerInstruction = clocksPerInstruction;
    }
  }
  if (!Config::highlyExperimental.clocksPerInstructionBypass) {
    LOG_INFO(Xenon, "{}: {} clocks per instruction", ppuState->ppuName, clocksPerInstruction);
  }
  else {
    LOG_INFO(Xenon, "{}: {} clocks per instruction (Overwritten! Actual CPI: {})", ppuState->ppuName, Config::highlyExperimental.clocksPerInstructionBypass, clocksPerInstruction);
    clocksPerInstruction = Config::highlyExperimental.clocksPerInstructionBypass;
  }

  // If we have a specific halt address, set it here
  ppuHaltOn = Config::debug.haltOnAddress;

  for (u8 thrdID = 0; thrdID < 2; thrdID++) {
    PPU_THREAD_REGISTERS &thread = ppuState->ppuThread[static_cast<ePPUThread>(thrdID)];
    thread.ppuRes = std::make_unique<STRIP_UNIQUE(PPU_THREAD_REGISTERS::ppuRes)>();
    memset(thread.ppuRes.get(), 0, sizeof(PPU_RES));
    xenonContext->xenonRes.Register(thread.ppuRes.get());

    // Set the decrementer as per docs. See CBE Public Registers pdf in Docs
    thread.SPR.DEC = 0x7FFFFFFF;

    // Resize ERATs
    thread.iERAT.resizeCache(64);
    thread.dERAT.resizeCache(64);
  }

  // Set PVR and PIR
  ppuState->SPR.PVR.PVR_Hex = PVR;
  ppuState->ppuThread[ePPUThread_Zero].SPR.PIR = PIR;
  ppuState->ppuThread[ePPUThread_One].SPR.PIR = PIR + 1;
}
PPU::~PPU() {
  // Signal we're quitting
  ppuThreadState.store(eThreadState::Quiting);
  std::this_thread::sleep_for(100ms);
  ppuThreadActive = false;
  // Kill the thread
  if (ppuThread.joinable())
    ppuThread.join();
  ppuState.reset();
}

void PPU::StartExecution(bool setHRMOR) {
  // If we want to start halted, then set the state
  if (Config::debug.startHalted) {
    ppuThreadState.store(eThreadState::Halted);
    // If we were told to halt on startup, ensure we are able to continue, otherwise it'll deadlock
    // We have it like this to safeguard against sleeping threads waking themselves after continuing,
    // thus destroying the stack
    ppuThreadPreviousState.store(ppuState->ppuID == 0 ? eThreadState::Running : eThreadState::Sleeping);
    LOG_DEBUG(Xenon, "{} was set to be halted, setting previous state to {}", ppuState->ppuName, ppuState->ppuID == 0 ? "Running" : "Sleeping");
  }
  else {
    LOG_DEBUG(Xenon, "{} setting to {}", ppuState->ppuName, ppuState->ppuID == 0 ? "Running" : "Sleeping");
    ppuThreadState.store(ppuState->ppuID == 0 ? eThreadState::Running : eThreadState::Sleeping);
    ppuThreadPreviousState.store(ppuThreadState);
  }

  // TLB Software reload Mode?
  ppuState->SPR.LPCR = 0x402ULL;

  // HID6?
  ppuState->SPR.HID6 = 0x1803800000000ULL;

  // TSCR[WEXT] = 1??
  ppuState->SPR.TSCR = 0x100000UL;

  // If we're PPU0,thread0 then enable THRD 0 and set Reset Vector.
  if (ppuState->ppuID == 0 && setHRMOR) {
    ppuState->SPR.CTRL = 0x800000UL; // CTRL[TE0] = 1;
    ppuState->SPR.HRMOR = 0x20000000000ULL;
    ppuState->ppuThread[ePPUThread_Zero].NIA = resetVector;
  }

  ppuThread = std::thread(&PPU::ThreadLoop, this);
}

void PPU::CalculateCPI() {
  // Get the instructions per second that we're able to execute.
  u32 instrPerSecond = GetIPS();

  // Get our CPI
  u64 cpi = get_cpi_value(instrPerSecond);

  LOG_INFO(Xenon, "{} Speed: {:#d} instructions per second.", ppuState->ppuName, instrPerSecond);

  // Find a way to calculate the right ticks/IPS ratio.
  clocksPerInstruction = cpi;
}

void PPU::Reset() {
  // Signal that we are resetting
  ppuThreadState.store(eThreadState::Resetting);
  ppuThreadPreviousState.store(eThreadState::None);

  // Tell the thread to reset it
  ppuThreadResetting = true;
}
void PPU::Halt(u64 haltOn, bool requestedByGuest, s8 ppuId, ePPUThread threadId) {
  if (haltOn && !guestHalt) {
    LOG_DEBUG(Xenon, "Halting PPU{} on address {:#x}", ppuState->ppuID, haltOn);
    ppuHaltOn = haltOn;
  }
  guestHalt = requestedByGuest;
  if (guestHalt) {
    guestHaltPPUId = ppuId;
    guestHaltThreadId = threadId;
#ifndef NO_GFX
    Xe_Main->renderer->SetDebuggerActive();
#endif
  }
  if (ppuThreadPreviousState == eThreadState::None) // If we were told to ignore it, then do so
    ppuThreadPreviousState.store(ppuThreadState.load());
  ppuThreadState = eThreadState::Halted;
}
void PPU::Continue() {
  if (ppuThreadState.load() == eThreadState::Running)
    return;
  if (ppuThreadPreviousState == eThreadState::Running)
    LOG_DEBUG(Xenon, "Continuing execution on PPU{}", ppuState->ppuID);
  ppuThreadState.store(ppuThreadPreviousState.load());
  ppuThreadPreviousState = eThreadState::None;
  guestHalt = false;
  guestHaltPPUId = -1;
  guestHaltThreadId = ePPUThread_None;
}
void PPU::ContinueFromException() {
  if (ppuThreadState.load() == eThreadState::Running)
    return;
  if (ppuThreadPreviousState == eThreadState::Running)
    LOG_DEBUG(Xenon, "Jumping to exception handler");
  if (ppuState->ppuID == guestHaltPPUId && guestHaltThreadId != ePPUThread_None) {
    PPU_THREAD_REGISTERS &thread = ppuState->ppuThread[guestHaltThreadId];
    thread.exceptReg |= PPU_EX_PROG;
    thread.exceptTrapType = EX_SRR1_TRAP_TRAP;
  }
  ppuThreadState.store(ppuThreadPreviousState.load());
  ppuThreadPreviousState = eThreadState::None;
  guestHalt = false;
  guestHaltPPUId = -1;
  guestHaltThreadId = ePPUThread_None;
}
void PPU::Step(int amount) {
  if (ppuThreadPreviousState == eThreadState::Running)
    LOG_DEBUG(Xenon, "Continuing PPU{} for {} Instructions", ppuState->ppuID, amount);
  ppuStepAmount = amount;
}

// PPU Entry Point.
void PPU::PPURunInstructions(u64 numInstrs, bool enableHalt) {
  // Start Profile
  MICROPROFILE_SCOPEI("[Xe::PPU]", "PPURunInstructions", MP_AUTO);
  for (size_t instrCount = 0; instrCount < numInstrs && ppuThreadActive; ++instrCount) {
    // Halt if needed before executing the instruction
    if (enableHalt && (ppuHaltOn && ppuHaltOn == curThread.CIA) && !guestHalt) {
      // Halt all cores
      // TODO(Vali0004): Change halt to be per-core or change halt continuing to be per-core
      Xe_Main->getCPU()->Halt();
    }

    // Read next instruction
    bool readNextInstr = false;
    // Profile read next instruction
    {
      MICROPROFILE_SCOPEI("[Xe::PPU]", "ReadNextInstruction", MP_AUTO);
      readNextInstr = PPUReadNextInstruction();
    }
    if (readNextInstr) {
#ifdef DEBUG_BUILD
      if (traceFile) {
        const std::string instrName = PPCInterpreter::PPCInterpreter_getFullName(_instr.opcode);
        fprintf(traceFile, "%llx: 0x%x %s\n", curThread.CIA, _instr.opcode, instrName.c_str());
      }
#endif
      // Start Profile
      MICROPROFILE_SCOPEI("[Xe::PPU]", "ExecuteSingleInstruction", MP_AUTO);
      // Execute instruction
      PPCInterpreter::ppcExecuteSingleInstruction(ppuState.get());
    }

    // Increase Time Base Counter if enabled
    CheckTimeBaseStatus();

    // Check for external interrupts
    if (curThread.SPR.MSR.EE && xenonContext->xenonIIC.checkExtInterrupt(curThread.SPR.PIR)) {
      _ex |= PPU_EX_EXT;
    }

    // Handle pending exceptions
    PPUCheckExceptions();

    // Break after exec and if it's halted
    if ((enableHalt && ppuThreadState == eThreadState::Halted) || ppuThreadState == eThreadState::Resetting)
      break;
  }
}

// PPU Thread state machine, handles all execution and codeflow
void PPU::ThreadStateMachine() {
  // Check if we should exit or not
  ppuThreadActive = ppuThreadState.load() != eThreadState::None &&
                    ppuThreadState.load() != eThreadState::Quiting;
  // Signal a reset if needed
  if (ppuThreadResetting) {
    ppuThreadState.store(eThreadState::Resetting);
  }
  switch (ppuThreadState) {
  case eThreadState::Executing: {
    ppuThreadState.store(eThreadState::Running);
  } break;
  case eThreadState::Running: {
    // Check our threads to see if any are running
    u8 state = GetCurrentRunningThreads();
    if (ppuThreadActive && !ppuThreadResetting  && (state & ePPUThreadBit_Zero)) {
      // Thread 0 is running, process instructions until we reach TTR timeout.
      curThreadId = ePPUThread_Zero;
      PPURunInstructions(ppuState->SPR.TTR);
    }
    if (ppuThreadActive && !ppuThreadResetting && (state & ePPUThreadBit_One)) {
      // Thread 1 is running, process instructions until we reach TTR timeout.
      curThreadId = ePPUThread_One;
      PPURunInstructions(ppuState->SPR.TTR);
    }
  } break;
  case eThreadState::Halted: {
    // Check if we should exit or not
    ppuThreadActive = ppuThreadState.load() != eThreadState::None &&
                      ppuThreadState.load() != eThreadState::Quiting;
    // Handle stepping
    u8 state = GetCurrentRunningThreads();
    if (ppuThreadActive && state & ePPUThreadBit_Zero) {
      curThreadId = ePPUThread_Zero;
      if (ppuStepAmount > 0) {
        PPURunInstructions(ppuStepAmount, false);
        ppuStepAmount = 0; // Ensure step mode doesn't continue indefinitely
      }
    } 
    if (ppuThreadActive && state & ePPUThreadBit_One) {
      curThreadId = ePPUThread_One;
      if (ppuStepAmount > 0) {
        PPURunInstructions(ppuStepAmount, false);
        ppuStepAmount = 0; // Ensure step mode doesn't continue indefinitely
      }
    }
  } break;
  case eThreadState::Sleeping: {
    // Waiting for an event, do nothing
    std::this_thread::sleep_for(1ms); // Don't burn the CPU
  } break;
  case eThreadState::Unused: {
    ppuThreadState.store(eThreadState::None);
  } break;
  case eThreadState::Resetting: {
    if (ppuState.get())
      LOG_INFO(Xenon, "PPU{} is resetting!", ppuState->ppuID);
    else
      LOG_INFO(Xenon, "A PPU is in the middle of resetting!");
    ppuThreadActive = false;
  } break;
  case eThreadState::Quiting: {
    if (ppuState.get()) {
      if (ppuState->ppuID == 0) {
        // Ensure the log is flushed
        fmt::print("\n");
      }
      LOG_INFO(Xenon, "PPU{} is exiting!", ppuState->ppuID);
    }
    ppuThreadState.store(eThreadState::None);
  } break;
  default: {

  } break;
  }
}
void PPU::ThreadLoop() {
  // Set thread name
  if (ppuState.get())
    Base::SetCurrentThreadName("[Xe] " + ppuState->ppuName);
  while (ppuThreadActive) {
    // Start Profile
    MICROPROFILE_SCOPEI("[Xe::PPU]", "ThreadLoop", MP_AUTO);
    // Check if we should exit or not
    ppuThreadActive = ppuThreadState.load() != eThreadState::None &&
                      ppuThreadState.load() != eThreadState::Quiting &&
                      ppuThreadState.load() != eThreadState::Resetting;

    // Run state machine
    ThreadStateMachine();

    // If our thread is not active while running, abort early.
    // We are likely destroying the handle
    if (!ppuThreadActive)
      break;

    // Check for external interrupts that enable execution
    if (ppuThreadActive && xenonContext->xenonIIC.checkExtInterrupt(curThread.SPR.PIR)) {
      // Check if we should actually enable the thread or not
      bool WEXT = (ppuState->SPR.TSCR & 0x100000) >> 20;
      if (!WEXT) {
        continue;
      }

      if (!ppuThreadResetting && ppuThreadState.load() == eThreadState::Halted || ppuThreadState.load() == eThreadState::Sleeping) {
        LOG_DEBUG(Xenon, "{} was previously halted or sleeping, bringing online", ppuState->ppuName);
        ppuThreadState.store(eThreadState::Running);
      }

      // Enable thread 0 execution
      ppuState->SPR.CTRL = 0x800000;

      // Issue reset
      ppuState->ppuThread[ePPUThread_Zero].exceptReg |= PPU_EX_RESET;
      ppuState->ppuThread[ePPUThread_One].exceptReg |= PPU_EX_RESET;
      
      PPU_THREAD_REGISTERS &thread = curThread;

      thread.SPR.SRR1 = 0x200000; // Set SRR1[42:44] = 100

      // ACK and EOI the interrupt
      u64 intData = 0;
      xenonContext->xenonIIC.readInterrupt(thread.SPR.PIR * 0x1000 + 0x50050, reinterpret_cast<u8*>(&intData), sizeof(intData));
      intData = 0;
      xenonContext->xenonIIC.writeInterrupt(thread.SPR.PIR * 0x1000 + 0x50060, reinterpret_cast<u8*>(&intData), sizeof(intData));
    }
  }
  // Thread is done executing, just tell it to exit
  ppuThreadActive = false;
}

// Returns a pointer to the specified thread.
PPU_THREAD_REGISTERS *PPU::GetPPUThread(u8 thrdID) {
  return &ppuState->ppuThread[static_cast<ePPUThread>(thrdID)];
}

// This is the calibration code for the GetIPS() function. It branches to the
// 0x4 location in memory.
static constexpr u32 ipsCalibrationCode[] = {
    0x55726220, //  rlwinm   r18,r11,12,8,16
    0x723D7825, //  andi.    r29,r17,0x7825
    0x65723D78, //  oris     r18,r11,0x3D78
    0x4BFFFFF4  //  b        ipsCalibrationCode
};

// Performs a test using a loop to check the amount of IPS we're able to
// execute
u32 PPU::GetIPS() {
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
  while (auto timerEnd = std::chrono::steady_clock::now() <= timerStart + 1s) {
    PPUReadNextInstruction();
    PPCInterpreter::ppcExecuteSingleInstruction(ppuState.get());
    instrCount++;
  }

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
template <typename T>
void bswap_elf(T &value) {
  value = byteswap_be<T>(value);
}
#define SWAP(header, entry) if (elf32) { bswap_elf(header entry); } else { bswap_elf(header##64 entry); }
#define READ(header, entry) elf32 ? header entry : header##64 entry
u64 PPU::loadElfImage(u8* data, u64 size) {
  // Setup HRMOR for elf binaries
  ppuState->SPR.CTRL = 0x800000; // CTRL[TE0] = 1;
  ppuState->SPR.HRMOR = 0x0000000000000000;

  // Loaded ELF Header type (elf32/elf64)
  bool elf32 = true; // Assume little endian file

  // We assume elf32 header unless specified otherwise
  elf32_hdr* header{ reinterpret_cast<decltype(header)>(data) };
  elf64_hdr* header64{ reinterpret_cast<decltype(header64)>(data) };

  // Check header:
  if (!IS_ELF(*header)) {
    LOG_CRITICAL(Xenon, "Attempting to load a binary which is not in elf format! Killing execution.");
    return 0;
  }

  // Check ELF header type (elf32/elf64):
  if (header->e_ident[EI_CLASS] == 1) { // ELF32 File Header
    LOG_INFO(Xenon, "ELF32 Header found.");
  }
  else {
    LOG_INFO(Xenon, "ELF64 Header found.");
    elf32 = false;
  }

  // Check data endianness after offset 0x10.
  if (header->e_ident[EI_DATA] == 1) {
    LOG_CRITICAL(Xenon, "Header data is in little-endian format. Xbox 360 is a BE machine. Killing execution.");
    return 0;
  }
  else {
    LOG_INFO(Xenon, "Header data is in big-endian format.");
  }

  // Byteswap required.
  bswap_elf(header->e_type);    // Offsett 0x10.
  bswap_elf(header->e_machine); // Offsett 0x12.
  bswap_elf(header->e_version); // Offsett 0x14.

  std::string elfType = "Unknown";
  #define D_TYPE(v, t, s) case t: v = #t ": " s; break
  #define D_TYPE_L(v, t, l, s) case t: v = #l ": " s; break
  switch (header->e_type) {
    D_TYPE(elfType, ET_NONE, "Unknown");
    D_TYPE(elfType, ET_REL, "Relocatable file");
    D_TYPE(elfType, ET_EXEC, "Executable file");
    D_TYPE(elfType, ET_DYN, "Shared object");
    D_TYPE(elfType, ET_CORE, "Core file");
    D_TYPE_L(elfType, 0xFE00, ET_LOOS, "Operating system specific");
    D_TYPE_L(elfType, 0xFEFF, ET_HIOS, "Operating system specific");
    D_TYPE(elfType, ET_LOPROC, "Processor specific");
    D_TYPE(elfType, ET_HIPROC, "Processor specific");
  }
  LOG_INFO(Xenon, "ELF Type: {}", elfType);

  // Check for machine type (PowerPC/PowerPC64):
  if (header->e_machine != 0x14 && header->e_machine != 0x15) {
    LOG_CRITICAL(Xenon, "Attempting to load an ELF binary which does not target the PowerPC/PowerPC64 ISA! Killing Execution.");
    return 0;
  }
  else {
    if (header->e_machine == 0x14) { // PowerPC
      LOG_INFO(Xenon, "Target ISA: PowerPC");
    }
    else { // PowerPC64
      LOG_INFO(Xenon, "Target ISA: PowerPC64");
    }
  }

  // Header-specific offsets:
  SWAP(header, ->e_entry);
  SWAP(header, ->e_phoff);
  SWAP(header, ->e_shoff);
  SWAP(header, ->e_flags);
  SWAP(header, ->e_ehsize);
  SWAP(header, ->e_phentsize);
  SWAP(header, ->e_phnum);
  SWAP(header, ->e_shentsize);
  SWAP(header, ->e_shnum);
  SWAP(header, ->e_shstrndx);

  // ELF Entry point.
  const auto entryPoint = READ(header, ->e_entry);
  LOG_INFO(Xenon, "ELF Entry Point: {:#x}", entryPoint);

  // Get the number of entries in the program header table.
  const auto progHeaderNumSections = READ(header, ->e_phnum);
  LOG_INFO(Xenon, "Number of entries in Program HT: {}", progHeaderNumSections);

  // Get the number of entries in the section header table.
  const auto sectHeaderNumSections = READ(header, ->e_shnum);
  LOG_INFO(Xenon, "Number of entries in Section HT: {}", sectHeaderNumSections);

  // Get the program header table data at specified offset.
  elf32_phdr* progHeaderTableData = reinterpret_cast<elf32_phdr*>(data + header->e_phoff);
  elf64_phdr* progHeaderTableData64 = reinterpret_cast<elf64_phdr*>(data + header64->e_phoff);

  // Load Segments from Program segment table.
  for (size_t idx = 0; idx < progHeaderNumSections; idx++) {
    SWAP(progHeaderTableData, [idx].p_type);
    SWAP(progHeaderTableData, [idx].p_offset);
    SWAP(progHeaderTableData, [idx].p_vaddr);
    SWAP(progHeaderTableData, [idx].p_paddr);
    SWAP(progHeaderTableData, [idx].p_filesz);
    SWAP(progHeaderTableData, [idx].p_memsz);
    SWAP(progHeaderTableData, [idx].p_flags);
    SWAP(progHeaderTableData, [idx].p_align);

    if (READ(progHeaderTableData, [idx].p_type) == PT_LOAD) {
      u64 vaddr = READ(progHeaderTableData, [idx].p_vaddr);
      u64 paddr = READ(progHeaderTableData, [idx].p_paddr);
      u64 filesize = READ(progHeaderTableData, [idx].p_filesz);
      u64 memsize = READ(progHeaderTableData, [idx].p_memsz);
      u64 file_offset = READ(progHeaderTableData, [idx].p_offset);
      bool physical_load = true;
      u64 target_addr = physical_load ? paddr : vaddr;
      LOG_INFO(Xenon, "Loading {:#x} bytes from offset {:#x} in the ELF to address {:#x}",
        filesize, file_offset, target_addr);
      PPCInterpreter::MMUMemCpyFromHost(ppuState.get(), target_addr, data + file_offset, filesize);
      if (memsize > filesize) { // Memory size greater than file, zero out remainder
        u64 remainder = memsize - filesize;
        PPCInterpreter::MMUMemSet(ppuState.get(), target_addr + filesize, 0, remainder);
      }
    }
  }
  LOG_INFO(Xenon, "ELF loaded successfully");

  curThread.NIA = entryPoint;

  return curThread.NIA;
}

// Reads the next instruction from memory and advances the NIP accordingly.
bool PPU::PPUReadNextInstruction() {
  PPU_THREAD_REGISTERS &thread = curThread;
  // Update previous instruction address
#ifndef NO_GFX
  thread.PIA = thread.NIA - 4;
#endif
  // Update current instruction address
  thread.CIA = thread.NIA;
  // Increase next instruction address
  thread.NIA += 4;
  thread.iFetch = true;
  // Fetch the instruction from memory
#ifndef NO_GFX
  if (Xe_Main.get() && Xe_Main->renderer->DebuggerActive())
    _previnstr.opcode = PPCInterpreter::MMURead32(ppuState.get(), thread.PIA);
#endif
  _instr.opcode = PPCInterpreter::MMURead32(ppuState.get(), thread.CIA);
#ifndef NO_GFX
  if (Xe_Main.get() && Xe_Main->renderer->DebuggerActive())
    _nextinstr.opcode = PPCInterpreter::MMURead32(ppuState.get(), thread.NIA);
#endif
  if (_instr.opcode == 0xFFFFFFFF) {
    LOG_CRITICAL(Xenon, "PPU{} returned a invalid opcode! Halting...", ppuState->ppuID);
    Xe_Main->getCPU()->Halt(); // Halt CPU
    Config::imgui.debugWindow = true; // Open the debugger on bad fault
    return false;
  }
  if (_ex & PPU_EX_INSSTOR || _ex & PPU_EX_INSTSEGM) {
    return false;
  }
  thread.iFetch = false;
  return true;
}

// Checks for exceptions and process them in the correct order.
void PPU::PPUCheckExceptions() {
  PPU_THREAD_REGISTERS& thread = curThread;
  // Start Profile
  MICROPROFILE_SCOPEI("[Xe::PPU]", "CheckExceptions", MP_AUTO);
  // Check Exceptions pending and process them in order.
  u16 &exceptions = _ex;
  if (exceptions != PPU_EX_NONE) {
    // Halt on any exception
    if (Config::debug.haltOnExceptions) {
      Xe_Main->xenonCPU->Halt(); // Halt all cores
    }
    // Non Maskable:
    //
    // 1. System Reset
    //
    if (exceptions & PPU_EX_RESET) {
      PPCInterpreter::ppcResetException(ppuState.get());
      exceptions &= ~PPU_EX_RESET;
      return;
    }
    //
    // 2. Machine Check
    //
    if (exceptions & PPU_EX_MC) {
      if (thread.SPR.MSR.ME) {
        PPCInterpreter::ppcResetException(ppuState.get());
        exceptions &= ~PPU_EX_MC;
        return;
      } else {
        // Checkstop Mode. Hard Fault.
        LOG_CRITICAL(Xenon, "{}: CHECKSTOP!", ppuState->ppuName);
        // TODO: Properly end execution.
        // A checkstop is a full - stop of the processor that requires a System
        // Reset to recover.
        SYSTEM_PAUSE();
      }
    }
    // Maskable:
    //
    // 3. Instruction-Dependent
    //
    // A. Program - Illegal Instruction
    if (exceptions & PPU_EX_PROG && thread.exceptTrapType == EX_SRR1_TRAP_ILL) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Illegal Instruction.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_PROG;
      return;
    }
    // B. Floating-Point Unavailable
    if (exceptions & PPU_EX_FPU) {
      PPCInterpreter::ppcFPUnavailableException(ppuState.get());
      exceptions &= ~PPU_EX_FPU;
      return;
    }
    // C. Data Storage, Data Segment, or Alignment
    // Data Storage
    if (exceptions & PPU_EX_DATASTOR) {
      PPCInterpreter::ppcDataStorageException(ppuState.get());
      exceptions &= ~PPU_EX_DATASTOR;
      return;
    }
    // Data Segment
    if (exceptions & PPU_EX_DATASEGM) {
      PPCInterpreter::ppcDataSegmentException(ppuState.get());
      exceptions &= ~PPU_EX_DATASEGM;
      return;
    }
    // Alignment
    if (exceptions & PPU_EX_ALIGNM) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Alignment.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_ALIGNM;
      return;
    }
    // D. Trace
    if (exceptions & PPU_EX_TRACE) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Trace.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_TRACE;
      return;
    }
    // E. Program Trap, System Call, Program Priv Inst, Program Illegal Inst
    // Program Trap
    if (exceptions & PPU_EX_PROG && thread.exceptTrapType == EX_SRR1_TRAP_TRAP) {
      PPCInterpreter::ppcProgramException(ppuState.get());
      exceptions &= ~PPU_EX_PROG;
      return;
    }
    // System Call
    if (exceptions & PPU_EX_SC) {
      PPCInterpreter::ppcSystemCallException(ppuState.get());
      exceptions &= ~PPU_EX_SC;
      return;
    }
    // Program - Privileged Instruction
    if (exceptions & PPU_EX_PROG && thread.exceptTrapType == EX_SRR1_TRAP_PRIV) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Privileged Instruction.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_PROG;
      return;
    }
    // F. Instruction Storage and Instruction Segment
    // Instruction Storage
    if (exceptions & PPU_EX_INSSTOR) {
      PPCInterpreter::ppcInstStorageException(ppuState.get());
      exceptions &= ~PPU_EX_INSSTOR;
      return;
    }
    // Instruction Segment
    if (exceptions & PPU_EX_INSTSEGM) {
      PPCInterpreter::ppcInstSegmentException(ppuState.get());
      exceptions &= ~PPU_EX_INSTSEGM;
      return;
    }
    //
    // 4. Program - Imprecise Mode Floating-Point Enabled Exception
    //
    if (exceptions & PPU_EX_PROG) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Imprecise Mode Floating-Point Enabled Exception.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_PROG;
      return;
    }
    //
    // 5. External, Decrementer, and Hypervisor Decrementer
    //
    // External
    if (exceptions & PPU_EX_EXT && thread.SPR.MSR.EE) {
      PPCInterpreter::ppcExternalException(ppuState.get());
      exceptions &= ~PPU_EX_EXT;
      return;
    }
    // Decrementer. A dec exception may be present but will only be taken when
    // the EE bit of MSR is set.
    if (exceptions & PPU_EX_DEC && thread.SPR.MSR.EE) {
      PPCInterpreter::ppcDecrementerException(ppuState.get());
      exceptions &= ~PPU_EX_DEC;
      return;
    }
    // Hypervisor Decrementer
    if (exceptions & PPU_EX_HDEC) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Hypervisor Decrementer.", ppuState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~PPU_EX_HDEC;
      return;
    }
    // VX Unavailable.
    if (exceptions & PPU_EX_VXU) {
      PPCInterpreter::ppcVXUnavailableException(ppuState.get());
      exceptions &= ~PPU_EX_VXU;
      return;
    }
  }
}

void PPU::CheckTimeBaseStatus() {
  // Start Profile
  MICROPROFILE_SCOPEI("[Xe::PPU]", "CheckTimeBaseStatus", MP_AUTO);
  // Increase Time Base Counter
  if (xenonContext->timeBaseActive) {
    // HID6[15]: Time-base and decrementer facility enable.
    // 0 -> TBU, TBL, DEC, HDEC, and the hang-detection logic do not
    // update. 1 -> TBU, TBL, DEC, HDEC, and the hang-detection logic
    // are enabled to update
    if (ppuState->SPR.HID6 & 0x1000000000000) {
      UpdateTimeBase();
    }
  }
}

// Updates the time base based on the amount of ticks and checks for decrementer
// interrupts if enabled.
void PPU::UpdateTimeBase() {
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

// Returns current executing thread by reading CTRL register
u8 PPU::GetCurrentRunningThreads() {
  if (!ppuState)
    return ePPUThreadBit_None;

  // Extract bits 22-23 in one step and directly map them to thread states
  u8 ctrlTE = (ppuState->SPR.CTRL >> 22) & 0b11;

  // Directly map ctrlTE to thread states using bit shifting
  return (ctrlTE & 0b01) * ePPUThreadBit_One | (ctrlTE & 0b10) / 2 * ePPUThreadBit_Zero;
}
