/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "PPU.h"

#include <assert.h>
#include <chrono>
#include <thread>

#include "Core/XeMain.h"
#include "Base/Config.h"
#include "Base/Thread.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/ElfABI.h"
#include "Core/XCPU/JIT/PPU_JIT.h"

PPU::PPU(Xe::XCPU::XenonContext *inXenonContext, u64 resetVector, u32 PIR) :
  resetVector(resetVector)
{
  traceFile = nullptr;
#ifdef DEBUG_BUILD
  if (Config::debug.createTraceFile) {
    char path[128];
    snprintf(path, 127, "trace_%d.log", PIR);
    traceFile = fopen(path, "w");
  }
#endif
  u32 executionMode = Base::JoaatStringHash(Config::highlyExperimental.cpuExecutor);
  switch (executionMode) {
  case "Interpreted"_jLower:
    currentExecMode = eExecutorMode::Interpreter;
    break;
  case "JIT"_jLower:
    currentExecMode = eExecutorMode::JIT;
    break;
  case "Hybrid"_jLower:
    currentExecMode = eExecutorMode::Hybrid;
    break;
  default:
    LOG_WARNING(Xenon, "Invalid execution mode '{}'! Defaulting to Interpreted", Config::highlyExperimental.cpuExecutor);
    currentExecMode = eExecutorMode::Interpreter;
    break;
  }

  //
  // Set everything as in POR. See CELL-BE Programming Handbook
  //

  if (ppuThreadState.load() == eThreadState::Unused) {
    return;
  }

  // Allocate memory for our PPE state
  ppeState = std::make_unique<STRIP_UNIQUE(ppeState)>();

  // Set PPU Thread ID (PIR, as 0 indexed. So 0-5)
  ppeState->ppuID = PIR / 2;

  // Set PPU Thread Name
  ppeState->ppuName = FMT("PPU{}", ppeState->ppuID);

  // Initialize Both threads as in a Reset
  for (u8 thrdNum = 0; thrdNum < 2; thrdNum++) {
    // Set Reset vector for both threads
    ppeState->ppuThread[static_cast<ePPUThreadID>(thrdNum)].NIA = XE_RESET_VECTOR;
    // Set MSR for both Threads
    ppeState->ppuThread[static_cast<ePPUThreadID>(thrdNum)].SPR.MSR.hexValue = 0x9000000000000000;
  }

  // Set Thread Timeout Register
  ppeState->SPR.TTR.hexValue = 0x4000; // Docs say that the recommended value is 16K instructions.

  ppuJIT = std::make_unique<PPU_JIT>(this);

  // Asign global Xenon context
  xenonContext = inXenonContext;

  xenonMMU = std::make_unique<STRIP_UNIQUE(xenonMMU)>(xenonContext);

  // If we have a specific halt address, set it here
  ppuHaltOn = Config::debug.haltOnAddress;

  for (u8 thrdID = 0; thrdID < 2; thrdID++) {
    sPPUThread &thread = ppeState->ppuThread[static_cast<ePPUThreadID>(thrdID)];
    thread.ppuRes = std::make_unique<STRIP_UNIQUE(sPPUThread::ppuRes)>();
    memset(thread.ppuRes.get(), 0, sizeof(PPU_RES));
    xenonContext->xenonRes.Register(thread.ppuRes.get());

    // Set the decrementer as per docs. See CBE Public Registers pdf in Docs
    thread.SPR.DEC = 0x7FFFFFFF;

    // Resize ERATs
    thread.iERAT.resizeCache(512);
    thread.dERAT.resizeCache(512);
  }

  // Set PVR and PIR
  switch (Config::highlyExperimental.consoleRevison) {
  case Config::eConsoleRevision::Xenon: {
    ppeState->SPR.PVR.hexValue = 0x00710200;
  } break;
  case Config::eConsoleRevision::Zephyr: {
    ppeState->SPR.PVR.hexValue = 0x00710300;
  } break;
  case Config::eConsoleRevision::Falcon: {
    ppeState->SPR.PVR.hexValue = 0x00710500;
  } break;
  case Config::eConsoleRevision::Jasper: {
    ppeState->SPR.PVR.hexValue = 0x00710500;
  } break;
  case Config::eConsoleRevision::Trinity: {
    ppeState->SPR.PVR.hexValue = 0x00710800;
  } break;
  case Config::eConsoleRevision::Corona4GB:
  case Config::eConsoleRevision::Corona: {
    ppeState->SPR.PVR.hexValue = 0x00710800;
  } break;
  case Config::eConsoleRevision::Winchester: {
    ppeState->SPR.PVR.hexValue = 0x00710900;
  } break;
  }
  ppeState->ppuThread[ePPUThread_Zero].SPR.PIR = PIR;
  ppeState->ppuThread[ePPUThread_One].SPR.PIR = PIR + 1;
}
PPU::~PPU() {
  // Signal we're quitting
  ppuThreadState.store(eThreadState::Quiting);
  ppuThreadActive = false;
  // Kill the thread
  if (ppuThread.joinable())
    ppuThread.join();
  ppuJIT.reset();
  ppeState.reset();
}

void PPU::StartExecution(bool setHRMOR) {
  // If we want to start halted, then set the state
  if (Config::debug.startHalted) {
    ppuThreadState.store(eThreadState::Halted);
    // If we were told to halt on startup, ensure we are able to continue, otherwise it'll deadlock
    // We have it like this to safeguard against sleeping threads waking themselves after continuing,
    // thus destroying the stack
    ppuThreadPreviousState.store(ppeState->ppuID == 0 ? eThreadState::Running : eThreadState::Sleeping);
    LOG_DEBUG(Xenon, "{} was set to be halted, setting previous state to {}", ppeState->ppuName, ppeState->ppuID == 0 ? "Running" : "Sleeping");
  }
  else {
    LOG_DEBUG(Xenon, "{} setting to {}", ppeState->ppuName, ppeState->ppuID == 0 ? "Running" : "Sleeping");
    ppuThreadState.store(ppeState->ppuID == 0 ? eThreadState::Running : eThreadState::Sleeping);
    ppuThreadPreviousState.store(ppuThreadState);
  }

  // TLB Software reload Mode?
  ppeState->SPR.LPCR.hexValue = 0x402ULL;

  // HID6?
  ppeState->SPR.HID6.hexValue = 0x1803800000000ULL;

  // TSCR[WEXT] = 1??
  ppeState->SPR.TSCR.hexValue = 0x100000UL;

  // Check for instruction tests.
  if (Config::xcpu.runInstrTests && ppeState->ppuID == 0) {
    LOG_INFO(Xenon, "Starting PowerPC instruction tests. Testing backend: {}",
      Config::xcpu.instrTestsMode ? "JITx86" : "Interpreter");
    RunInstructionTests(ppeState.get(), ppuJIT.get(), static_cast<ePPUTestingMode>(Config::xcpu.instrTestsMode));
  }

  // If we're PPU0,thread0 then enable THRD 0 and set Reset Vector.
  if (ppeState->ppuID == 0 && setHRMOR) {
    ppeState->SPR.CTRL.TE0 = 1; // Enable Thread 0
    ppeState->SPR.HRMOR.hexValue = 0x20000000000ULL;
    ppeState->ppuThread[ePPUThread_Zero].NIA = resetVector;
    // Also simulate 1BL if we're told to.
    if (Config::xcpu.simulate1BL) { Simulate1Bl(); }
  }

  ppuThread = std::thread(&PPU::ThreadLoop, this);
}

void PPU::Reset() {
  // Signal that we are resetting
  ppuThreadState.store(eThreadState::Resetting);
  ppuThreadPreviousState.store(eThreadState::None);

  // Tell the thread to reset it
  ppuThreadResetting = true;
}

void PPU::Halt(u64 haltOn, bool requestedByGuest, s8 ppuId, ePPUThreadID threadId) {
  if (haltOn && !guestHalt) {
    LOG_DEBUG(Xenon, "Halting PPU{} on address 0x{:X}", ppeState->ppuID, haltOn);
    ppuHaltOn = haltOn;
  }
  guestHalt = requestedByGuest;
#ifndef NO_GFX
  if (guestHalt && XeMain::renderer) {
    XeMain::renderer->SetDebuggerActive(ppuId);
  }
#endif
  if (ppuThreadPreviousState == eThreadState::None) // If we were told to ignore it, then do so
    ppuThreadPreviousState.store(ppuThreadState.load());
  ppuThreadState.store(eThreadState::Halted);
}
void PPU::Continue() {
  if (ppuThreadState.load() == eThreadState::Running)
    return;
  if (ppuThreadPreviousState == eThreadState::Running)
    LOG_DEBUG(Xenon, "Continuing execution on PPU{}", ppeState->ppuID);
  ppuThreadState.store(ppuThreadPreviousState.load());
  ppuThreadPreviousState.store(eThreadState::None);
  guestHalt = false;
}
void PPU::ContinueFromException() {
  if (ppuThreadState.load() == eThreadState::Running)
    return;
  if (ppuThreadPreviousState == eThreadState::Running)
    LOG_DEBUG(Xenon, "Jumping to exception handler");
  if (guestHalt) {
    sPPUThread &thread = ppeState->ppuThread[curThreadId];
    thread.exceptReg |= ppuProgramEx;
    thread.progExceptionType = ppuProgExTypeTRAP;
  }
  ppuThreadState.store(ppuThreadPreviousState.load());
  ppuThreadPreviousState.store(eThreadState::None);
  guestHalt = false;
}
void PPU::Step(int amount) {
  if (ppuThreadState.load() == eThreadState::Running)
    return;
  if (ppuThreadPreviousState == eThreadState::Running)
    LOG_DEBUG(Xenon, "Continuing PPU{} for {} Instructions", ppeState->ppuID, amount);
  ppuStepAmount = amount;
}

// PPU Entry Point.
void PPU::PPURunInstructions(u64 numInstrs, bool enableHalt) {
  // Start Profile
  MICROPROFILE_SCOPEI("[Xe::PPU]", "PPURunInstructions", MP_AUTO);
  for (size_t instrCount = 0; instrCount < numInstrs && ppuThreadActive; ++instrCount) {
    // Halt if needed before executing the next instruction
    if (enableHalt && ppuHaltOn == curThread.NIA) {
      Halt();
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
      PPCInterpreter::ppcExecuteSingleInstruction(ppeState.get());
    }

    // Check for external interrupts
    if (curThread.SPR.MSR.EE && xenonContext->iic.hasPendingInterrupts(curThread.SPR.PIR)) {
      _ex |= ppuExternalEx;
    }

    // Handle pending exceptions
    PPUCheckExceptions();

    // If the thread was suspended due to CTRL being written, we must end execution on said thread.
    if (ppeState->currentThread == 0 && ppeState->SPR.CTRL.TE0 != true) { break; }
    if (ppeState->currentThread == 1 && ppeState->SPR.CTRL.TE1 != true) { break; }

    // Break after exec and if it's halted
    if ((enableHalt && ppuThreadState == eThreadState::Halted) || ppuThreadState == eThreadState::Resetting)
      break;
  }
}

// PPU Thread state machine, handles all execution and codeflow
void PPU::ThreadStateMachine() {
  // Check if we should exit or not
  ppuThreadActive = ppuThreadState.load() != eThreadState::None;
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
    if (currentExecMode == eExecutorMode::Interpreter) {
      if (!ppuThreadResetting && (state & ePPUThreadBit_Zero)) {
        // Thread 0 is running, process instructions until we reach TTR timeout.
        curThreadId = ePPUThread_Zero;
        PPURunInstructions(ppeState->SPR.TTR.hexValue, ppuHaltOn != 0);
      }
      if (!ppuThreadResetting && (state & ePPUThreadBit_One)) {
        // Thread 1 is running, process instructions until we reach TTR timeout.
        curThreadId = ePPUThread_One;
        PPURunInstructions(ppeState->SPR.TTR.hexValue, ppuHaltOn != 0);
      }
    } else {
      if (!ppuThreadResetting && (state & ePPUThreadBit_Zero)) {
        // Thread 1 is running, process instructions until we reach TTR timeout.
        curThreadId = ePPUThread_Zero;
        ppuJIT->ExecuteJITInstrs(ppeState->SPR.TTR.hexValue, ppuThreadActive, ppuHaltOn != 0);
      }
      if (!ppuThreadResetting && (state & ePPUThreadBit_One)) {
        // Thread 1 is running, process instructions until we reach TTR timeout.
        curThreadId = ePPUThread_One;
        ppuJIT->ExecuteJITInstrs(ppeState->SPR.TTR.hexValue, ppuThreadActive, ppuHaltOn != 0);
      }
    }
  } break;
  case eThreadState::Halted: {
    // Check if we should exit or not
    ppuThreadActive = ppuThreadState.load() != eThreadState::None;
    // Handle stepping
    u8 state = GetCurrentRunningThreads();
    if (currentExecMode == eExecutorMode::Interpreter) {
      if (state & ePPUThreadBit_Zero) {
        curThreadId = ePPUThread_Zero;
        if (ppuStepAmount > 0) {
          PPURunInstructions(ppuStepAmount, false);
          ppuStepAmount = 0; // Ensure step mode doesn't continue indefinitely
        }
      }
      if (state & ePPUThreadBit_One) {
        curThreadId = ePPUThread_One;
        if (ppuStepAmount > 0) {
          PPURunInstructions(ppuStepAmount, false);
          ppuStepAmount = 0; // Ensure step mode doesn't continue indefinitely
        }
      }
    } else {
      if (state & ePPUThreadBit_Zero) {
        curThreadId = ePPUThread_Zero;
        if (ppuStepAmount > 0) {
          ppuJIT->ExecuteJITInstrs(ppuStepAmount, ppuThreadActive, false);
          ppuStepAmount = 0; // Ensure step mode doesn't continue indefinitely
        }
      }
      if (state & ePPUThreadBit_One) {
        curThreadId = ePPUThread_One;
        if (ppuStepAmount > 0) {
          ppuJIT->ExecuteJITInstrs(ppuStepAmount, ppuThreadActive, false);
          ppuStepAmount = 0; // Ensure step mode doesn't continue indefinitely
        }
      }
    }
  } break;
  case eThreadState::Sleeping: {
    // Waiting for an event, do nothing
    std::this_thread::sleep_for(1ns); // Don't burn the CPU
  } break;
  case eThreadState::Unused: {
    ppuThreadState.store(eThreadState::None);
  } break;
  case eThreadState::Resetting: {
    if (ppeState.get())
      LOG_INFO(Xenon, "PPU{} is resetting!", ppeState->ppuID);
    else
      LOG_INFO(Xenon, "A PPU is in the middle of resetting!");
    ppuThreadState.store(eThreadState::None);
  } break;
  case eThreadState::Quiting: {
    ppuThreadState.store(eThreadState::None);
  } break;
  default: {

  } break;
  }
}
void PPU::ThreadLoop() {
  // Set thread name
  if (ppeState.get())
    Base::SetCurrentThreadName("[Xe] " + ppeState->ppuName);
  while (ppuThreadActive) {
    // Start Profile
    MICROPROFILE_SCOPEI("[Xe::PPU]", "ThreadLoop", MP_AUTO);
    // Run state machine
    ThreadStateMachine();

    // If our thread is not active while running, abort early.
    // We are likely destroying the handle
    if (!ppuThreadActive)
      break;

    if (PPUCheckInterrupts())
      continue;
  }
  // Thread is done executing, just tell it to exit
  ppuThreadActive = false;
}

// Returns a pointer to the specified thread.
sPPUThread *PPU::GetPPUThread(u8 thrdID) {
  return &ppeState->ppuThread[static_cast<ePPUThreadID>(thrdID)];
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
  for (u8 i = 0; i != 4; ++i) {
    PPCInterpreter::MMUWrite32(ppeState.get(), 4 + (i * 4), ipsCalibrationCode[i]);
  }

  // Set our NIP to our calibration code address
  curThread.NIA = 4;

  // Start a timer
  auto timerStart = std::chrono::steady_clock::now();

  // Instruction count
  u64 instrCount = 0;

  // Execute the amount of cycles we're requested
  while (auto timerEnd = std::chrono::steady_clock::now() <= timerStart + 1s) {
    if (currentExecMode != eExecutorMode::Interpreter) {
      ppuJIT->ExecuteJITInstrs(4, ppuThreadActive);
      instrCount += 4;
      continue;
    } else {
      PPUReadNextInstruction();
      PPCInterpreter::ppcExecuteSingleInstruction(ppeState.get());
    }
    instrCount++;
  }

  // Zero out the memory after execution
  for (s32 i = 0; i < 4; i++) {
    PPCInterpreter::MMUWrite32(ppeState.get(), 4 + (i * 4), 0x00000000);
  }

  // Set the NIP back to default
  curThread.NIA = 0x100;

  // Reset the registers
  memset(curThread.GPR, 0, sizeof(curThread.GPR));

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
  ppeState->SPR.CTRL.hexValue = 0x800000; // CTRL[TE0] = 1;
  ppeState->SPR.HRMOR.hexValue = 0x0000000000000000;

  // Loaded ELF Header type (elf32/elf64)
  bool elf32 = true; // Assume little endian file

  // We assume elf32 header unless specified otherwise
  elf32_hdr *header{ reinterpret_cast<decltype(header)>(data) };
  elf64_hdr *header64{ reinterpret_cast<decltype(header64)>(data) };

  // Check header:
  if (!IS_ELF(*header)) {
    LOG_CRITICAL(Xenon, "Attempting to load a binary which is not in elf format! Killing execution.");
    return 0;
  }

  // Check ELF header type (elf32/elf64):
  if (header->e_ident[EI_CLASS] == 1) { // ELF32 File Header
    LOG_INFO(Xenon, "ELF32 Header found.");
  } else {
    LOG_INFO(Xenon, "ELF64 Header found.");
    elf32 = false;
  }

  // Check data endianness after offset 0x10.
  if (header->e_ident[EI_DATA] == 1) {
    LOG_CRITICAL(Xenon, "Header data is in little-endian format. Xbox 360 is a BE machine. Killing execution.");
    return 0;
  } else {
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
  } else {
    if (header->e_machine == 0x14) { // PowerPC
      LOG_INFO(Xenon, "Target ISA: PowerPC");
    } else { // PowerPC64
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
  LOG_INFO(Xenon, "ELF Entry Point: 0x{:X}", entryPoint);

  // Get the number of entries in the program header table.
  const auto progHeaderNumSections = READ(header, ->e_phnum);
  LOG_INFO(Xenon, "Number of entries in Program HT: {}", progHeaderNumSections);

  // Get the number of entries in the section header table.
  const auto sectHeaderNumSections = READ(header, ->e_shnum);
  LOG_INFO(Xenon, "Number of entries in Section HT: {}", sectHeaderNumSections);

  // Get the program header table data at specified offset.
  elf32_phdr *progHeaderTableData = reinterpret_cast<elf32_phdr*>(data + header->e_phoff);
  elf64_phdr *progHeaderTableData64 = reinterpret_cast<elf64_phdr*>(data + header64->e_phoff);

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
      LOG_INFO(Xenon, "Loading 0x{:X} bytes from offset 0x{:X} in the ELF to address 0x{:X}",
        filesize, file_offset, target_addr);
      PPCInterpreter::MMUMemCpyFromHost(ppeState.get(), target_addr, data + file_offset, filesize);
      if (memsize > filesize) { // Memory size greater than file, zero out remainder
        u64 remainder = memsize - filesize;
        PPCInterpreter::MMUMemSet(ppeState.get(), target_addr + filesize, 0, remainder);
      }
    }
  }
  LOG_INFO(Xenon, "ELF loaded successfully");

  curThread.NIA = entryPoint;

  return curThread.NIA;
}

// Reads the next instruction from memory and advances the NIP accordingly.
bool PPU::PPUReadNextInstruction() {
  ePPUThreadID thrId = curThreadId;
  sPPUThread &thread = ppeState->ppuThread[thrId];
  // Update previous instruction address
  thread.PIA = thread.CIA;
  // Update current instruction address
  thread.CIA = thread.NIA;
  // Increase next instruction address
  thread.NIA += 4;
  thread.instrFetch = true;
  // Fetch the instruction from memory
  _instr.opcode = PPCInterpreter::MMURead32(ppeState.get(), thread.CIA, thrId);
  if (_instr.opcode == 0xFFFFFFFF || _instr.opcode == 0xCDCDCDCD) {
    LOG_CRITICAL(Xenon, "PPU{} returned an invalid opcode found. Data = {:#x}, PIA [{:#x}] -> CIA [{:#x}]. Halting...", 
      ppeState->ppuID, _instr.opcode, thread.PIA, thread.CIA);
    Halt();
    return false;
  }
  if (_ex & ppuInstrStorageEx || _ex & ppuInstrSegmentEx) {
    return false;
  }
  thread.instrFetch = false;
  return true;
}

// Checks for CPU bringup interrupts
bool PPU::PPUCheckInterrupts() {
  // Check if we are allowed to enable thread zero if the thread is sleeping...
  bool WEXT = (ppeState->SPR.TSCR.hexValue & 0x100000) >> 20;

  // Check for external interrupts that enable execution
  if (ppuThreadActive && !ppuThreadResetting && (ppuThreadState.load() == eThreadState::Halted 
    || ppuThreadState.load() == eThreadState::Sleeping) && WEXT) {
    // Check for an external interrupt that enables execution.
    if (!xenonContext->iic.hasPendingInterrupts(curThread.SPR.PIR, true)) {
      return true;
    }

    // Proceed.
    LOG_DEBUG(Xenon, "{} was previously halted or sleeping, bringing online", ppeState->ppuName);
    ppuThreadState.store(eThreadState::Running);
    
    // Enable thread 0 execution and issue a system reset exception.
    ppeState->SPR.CTRL.TE0 = 1;
    ppeState->ppuThread[ePPUThread_Zero].exceptReg |= ppuSystemResetEx;

    sPPUThread &thread = curThread;

    thread.SPR.SRR1 = 0x200000; // Set SRR1[42:44] = 100

    // ACK and EOI the interrupt
    u64 intData = 0;
    xenonContext->iic.Read(thread.SPR.PIR * 0x1000 + 0x50050, reinterpret_cast<u8*>(&intData), sizeof(intData));
    intData = 0;
    xenonContext->iic.Write(thread.SPR.PIR * 0x1000 + 0x50060, reinterpret_cast<u8*>(&intData), sizeof(intData));
  }

  return false;
}

// Checks for exceptions and process them in the correct order.
bool PPU::PPUCheckExceptions() {
  sPPUThread &thread = curThread;
  // Start Profile
  MICROPROFILE_SCOPEI("[Xe::PPU]", "CheckExceptions", MP_AUTO);
  // Check Exceptions pending and process them in order.
  u16 &exceptions = _ex;
  if (exceptions != ppuNone) {
    // Exceptions are pending, check and process them in order.

    // Non Maskable:
    //
    // 1. System Reset
    //
    if (exceptions & ppuSystemResetEx) {
      PPUSystemResetException(ppeState.get());
      exceptions &= ~ppuSystemResetEx;
      return true;
    }
    //
    // 2. Machine Check
    //
    if (exceptions & ppuMachineCheckEx) {
      if (thread.SPR.MSR.ME) {
        PPUSystemResetException(ppeState.get());
        exceptions &= ~ppuMachineCheckEx;
        return true;
      } else {
        // Checkstop Mode. Hard Fault.
        LOG_CRITICAL(Xenon, "{}: CHECKSTOP!", ppeState->ppuName);
        // A checkstop is a full - stop of the processor that requires a System
        // Reset to recover.
        XeMain::ShutdownCPU();
      }
    }
    // Maskable:
    //
    // 3. Instruction-Dependent
    //
    // A. Program - Illegal Instruction
    if (exceptions & ppuProgramEx && thread.progExceptionType == ppuProgExTypeILL) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Illegal Instruction.", ppeState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~ppuProgramEx;
      return true;
    }
    // B. Floating-Point Unavailable
    if (exceptions & ppuFPUnavailableEx) {
      PPUFPUnavailableException(ppeState.get());
      exceptions &= ~ppuFPUnavailableEx;
      return true;
    }
    // C. Data Storage, Data Segment, or Alignment
    // Data Storage
    if (exceptions & ppuDataStorageEx) {
      PPUDataStorageException(ppeState.get());
      exceptions &= ~ppuDataStorageEx;
      return true;
    }
    // Data Segment
    if (exceptions & ppuDataSegmentEx) {
      PPUDataSegmentException(ppeState.get());
      exceptions &= ~ppuDataSegmentEx;
      return true;
    }
    // Alignment
    if (exceptions & ppuAlignmentEx) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Alignment.", ppeState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~ppuAlignmentEx;
      return true;
    }
    // D. Trace
    if (exceptions & ppuTraceEx) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Trace.", ppeState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~ppuTraceEx;
      return true;
    }
    // E. Program Trap, System Call, Program Priv Inst, Program Illegal Inst
    // Program Trap
    if (exceptions & ppuProgramEx && thread.progExceptionType == ppuProgExTypeTRAP) {
      PPUProgramException(ppeState.get());
      exceptions &= ~ppuProgramEx;
      return true;
    }
    // System Call
    if (exceptions & ppuSystemCallEx) {
      PPUSystemCallException(ppeState.get());
      exceptions &= ~ppuSystemCallEx;
      return true;
    }
    // Program - Privileged Instruction
    if (exceptions & ppuProgramEx && thread.progExceptionType == ppuProgExTypePRIV) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Privileged Instruction.", ppeState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~ppuProgramEx;
      return true;
    }
    // F. Instruction Storage and Instruction Segment
    // Instruction Storage
    if (exceptions & ppuInstrStorageEx) {
      PPUInstStorageException(ppeState.get());
      exceptions &= ~ppuInstrStorageEx;
      return true;
    }
    // Instruction Segment
    if (exceptions & ppuInstrSegmentEx) {
      PPUInstSegmentException(ppeState.get());
      exceptions &= ~ppuInstrSegmentEx;
      return true;
    }
    //
    // 4. Program - Imprecise Mode Floating-Point Enabled Exception
    //
    if (exceptions & ppuProgramEx) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Imprecise Mode Floating-Point Enabled Exception.", ppeState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~ppuProgramEx;
      return true;
    }
    //
    // 5. External, Decrementer, and Hypervisor Decrementer
    //
    // External
    if (exceptions & ppuExternalEx && thread.SPR.MSR.EE) {
      PPUExternalException(ppeState.get());
      exceptions &= ~ppuExternalEx;
      return true;
    }
    // Decrementer. A dec exception may be present but will only be taken when
    // the EE bit of MSR is set.
    if (exceptions & ppuDecrementerEx && thread.SPR.MSR.EE) {
      PPUDecrementerException(ppeState.get());
      exceptions &= ~ppuDecrementerEx;
      return true;
    }
    // Hypervisor Decrementer
    if (exceptions & ppuHypervisorDecrementerEx) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Hypervisor Decrementer.", ppeState->ppuName, static_cast<u8>(curThreadId));
      exceptions &= ~ppuHypervisorDecrementerEx;
      return true;
    }
    // VX Unavailable.
    if (exceptions & ppuVXUnavailableEx) {
      PPUVXUnavailableException(ppeState.get());
      exceptions &= ~ppuVXUnavailableEx;
      return true;
    }
  }
  // No exceptions have ocurred.
  return false;
}

// Updates the time base based on the amount of ticks and checks for decrementer
// interrupts if enabled.
void PPU::UpdateTimeBase(u64 tbTicks) {
  // HID6[15]: Time-base and decrementer facility enable.
  // 0 -> TBU, TBL, DEC, HDEC, and the hang-detection logic do not
  // update. 1 -> TBU, TBL, DEC, HDEC, and the hang-detection logic
  // are enabled to update
  if (ppeState->SPR.HID6.tb_enable) {
    // The Decrementer and the Time Base are driven by the same time frequency.
    u32 newDec = 0;
    u32 dec = 0;
    // Update the Time Base.
    ppeState->SPR.TB.hexValue += tbTicks;
    // Get the decrementer value.
    dec = curThread.SPR.DEC;
    newDec = dec - tbTicks;
    // Update the new decrementer value.
    curThread.SPR.DEC = newDec;
    // Check if Previous decrementer measurement is smaller than current and a
    // decrementer exception is not pending.
    if (newDec > dec && !(_ex & ppuDecrementerEx)) {
      // The decrementer must issue an interrupt.
      _ex |= ppuDecrementerEx;
    }
  }
}

// Returns current executing thread by reading CTRL register
u8 PPU::GetCurrentRunningThreads() {
  if (!ppeState)
    return ePPUThreadBit_None;

  // Extract bits 22-23 in one step and directly map them to thread states
  u8 ctrlTE = (ppeState->SPR.CTRL.hexValue >> 22) & 0b11;
  // If the thread state was changed to shut down both threads, set the thread state to sleeping.
  if (!(ppeState->SPR.CTRL.TE0 || ppeState->SPR.CTRL.TE1)) {
    ppuThreadState.store(eThreadState::Sleeping);
  }

  // Directly map ctrlTE to thread states using bit shifting
  return (ctrlTE & 0b01) * ePPUThreadBit_One | (ctrlTE & 0b10) / 2 * ePPUThreadBit_Zero;
}

// Does a mostly complete simulation of the 1Bl inside the SROM.
// This piece of code, in a nutshell does the following:
// * Trains the CPU's FSB TX and RX lines.
// * Verifies the offset of CB from NAND.
// * Fetches and validates the CB header from NAND.
// * Copies the encrypted the CB header from NAND to internal Secure ROM.
// * Generates the CB's HMAC key.
// * Initializes the CB's RC4 decryption key.
// * RC4 decrypts CB and verifies it.
// * Sets up some states and registers and jumps to CB in the Secure ROM.
bool PPU::Simulate1Bl() {
  LOG_INFO(Xenon, "1BL Simulation started:");
  // Since we dont actually have a FSB to make use of (nor we need one ofc) we can simply bypass this.
  
  // Zero out Secure RAM:
  LOG_INFO(Xenon, " * Zeroing SRAM.");
  PPCInterpreter::MMUMemSet(ppeState.get(), 0x10000, 0, 0x10000);

  // Verify CB's offset in NAND and fetch its header contents.
  // CB's offset should be stored in the NAND header at location 0x8.
  u32 cbOffset = PPCInterpreter::MMURead32(ppeState.get(), NAND_MEMORY_MAPPED_ADDR + 8);
  
  // Verification is nothing but a mere address alignment and a not zero check.
  if (cbOffset == 0) {
    LOG_CRITICAL(Xenos, "CB Offset verification failed, returned address is {:#x}.", cbOffset);
    return false;
  }

  // Read CB header, we don't print anything as SFCX code should have already done this.
  Xe::PCIDev::BL_HEADER cbHeader = {};
  PPCInterpreter::MMURead(xenonContext, ppeState.get(), NAND_MEMORY_MAPPED_ADDR + cbOffset, 16, reinterpret_cast<u8*>(&cbHeader));

  // Byteswap header data.
  cbHeader.entryPoint = byteswap_be(cbHeader.entryPoint);
  cbHeader.length = byteswap_be(cbHeader.length);

  LOG_INFO(Xenon, " * Found CB Header at offset {:#x}, entry point {:#x}, size {:#x}.", cbOffset, cbHeader.entryPoint,
    cbHeader.length);

  // Copy CB data from NAND.
  LOG_INFO(Xenon, " * Fetching CB data.");
  std::vector<u8> cbData;
  for (size_t idx = 0; idx < cbHeader.length; idx++) {
    cbData.push_back(PPCInterpreter::MMURead8(ppeState.get(), NAND_MEMORY_MAPPED_ADDR + cbOffset + idx));
  }

  // Initialize HMAC key.

  // All good.
  return true;
}

//
// Exception definitions.
//

// Format: Exception name (Reset Vector)

// System reset Exception (0x100)
void PPU::PPUSystemResetException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_INFO(Xenon, "[{}](Thrd{:#d}): System Reset exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x100;
}

// Data Storage Exception (0x300)
void PPU::PPUDataStorageException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Data Storage exception. EA: 0x{:X}.", ppeState->ppuName, static_cast<s8>(curThreadId), thread.SPR.DAR);
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x300;
}

// Data Segment Exception (0x380)
void PPU::PPUDataSegmentException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Data Segment exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x380;
}

// Instruction Storage Exception (0x400)
void PPU::PPUInstStorageException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Instruction Storage exception. EA = 0x{:X}", ppeState->ppuName, static_cast<s8>(curThreadId), thread.CIA);
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.SRR1 |= 0x40000000;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x400;
}

// Instruction Segment Exception (0x480)
void PPU::PPUInstSegmentException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Instruction Segment exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x480;
}

// External Exception (0x500)
void PPU::PPUExternalException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): External exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x500;
}

// Program Exception (0x700)
void PPU::PPUProgramException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Program exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  BSET(thread.SPR.SRR1, 64, thread.progExceptionType);
  thread.NIA = 0x700;
}

// FP Unavailable Exception (0x800)
void PPU::PPUFPUnavailableException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): FPU exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x800;
}

// Decrementer Exception (0x900)
void PPU::PPUDecrementerException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): Decrementer exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0x900;
}

// System Call Exception (0xC00)
void PPU::PPUSystemCallException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): System Call exception. Syscall ID: 0x{:X}", ppeState->ppuName, static_cast<s8>(curThreadId), GPR(0));
  thread.SPR.SRR0 = thread.NIA;
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0xC00;
}

// VX Unavailable Exception (0xF20)
void PPU::PPUVXUnavailableException(sPPEState* ppeState) {
  sPPUThread& thread = curThread;
  LOG_TRACE(Xenon, "[{}](Thrd{:#d}): VXU exception.", ppeState->ppuName, static_cast<s8>(curThreadId));
  thread.SPR.SRR0 = thread.CIA; // See Cell Vector SIMD PEM, page 104, table 5.4.
  thread.SPR.SRR1 = thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF;
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // This clears both IR and DR bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;
  thread.NIA = 0xF20;
}