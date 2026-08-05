/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "PPU.h"

#include "Base/Config.h"
#include "Base/Logging/Log.h"
#include "Base/Thread.h"
#include "Core/XCPU/ElfABI.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XeMain.h"

#include <assert.h>
#include <chrono>
#include <thread>

PPU::PPU(Xe::XCPU::XenonContext* inXenonContext, u64 resetVector, u32 PIR)
    : resetVector(resetVector) {
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
    case "Interpreted"_jLower: currentExecMode = eExecutorMode::Interpreter; break;
    default:
      LOG_WARNING(Xenon, "Invalid execution mode '{}'! Defaulting to Interpreted",
                  Config::highlyExperimental.cpuExecutor);
      currentExecMode = eExecutorMode::Interpreter;
      break;
  }

  //
  // Set everything as in POR. See CELL-BE Programming Handbook
  //

  if (ppuThreadState.load() == eThreadState::Unused) { return; }

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

  // Asign global Xenon context
  xenonContext = inXenonContext;

  // The PPE owns the MMU.
  ppeState->mmu = std::make_shared<Xe::XCPU::MMU::XenonMMU>(xenonContext, ppeState.get());

  // If we have a specific halt address, set it here
  ppuHaltOn = Config::debug.haltOnAddress;

  for (u8 thrdID = 0; thrdID < 2; thrdID++) {
    sPPUThread& thread = ppeState->ppuThread[static_cast<ePPUThreadID>(thrdID)];
    thread.ppuRes = std::make_unique<STRIP_UNIQUE(sPPUThread::ppuRes)>();
    memset(thread.ppuRes.get(), 0, sizeof(PPU_RES));
    xenonContext->xenonRes.Register(thread.ppuRes.get());

    // Set the decrementer as per docs. See CBE Public Registers pdf in Docs
    thread.SPR.DEC = 0x7FFFFFFF;
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

// Destructor
PPU::~PPU() {
  // Signal we're quitting
  ppuThreadState.store(eThreadState::Quiting);
  ppuThreadActive = false;
  // Kill the thread
  if (ppuThread.joinable()) ppuThread.join();
  ppeState.reset();
}

void PPU::StartExecution(bool setHRMOR) {
  // If we want to start halted, then set the state
  if (Config::debug.startHalted) {
    ppuThreadState.store(eThreadState::Halted);
    // If we were told to halt on startup, ensure we are able to continue,
    // otherwise it'll deadlock We have it like this to safeguard against
    // sleeping threads waking themselves after continuing, thus destroying the
    // stack
    ppuThreadPreviousState.store(ppeState->ppuID == 0 ? eThreadState::Running : eThreadState::Sleeping);
    LOG_DEBUG(Xenon, "{} was set to be halted, setting previous state to {}", ppeState->ppuName,
              ppeState->ppuID == 0 ? "Running" : "Sleeping");
  } else {
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
    LOG_INFO(Xenon, "Starting PowerPC instruction tests. Testing backend: {}", "Interpreter");
    RunInstructionTests(ppeState.get(), static_cast<ePPUTestingMode>(Config::xcpu.instrTestsMode));
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
  if (guestHalt && XeMain::renderer) { XeMain::renderer->SetDebuggerActive(ppuId); }
#endif
  if (ppuThreadPreviousState == eThreadState::None) // If we were told to ignore it, then do so
    ppuThreadPreviousState.store(ppuThreadState.load());
  ppuThreadState.store(eThreadState::Halted);
}
void PPU::Continue() {
  if (ppuThreadState.load() == eThreadState::Running) return;
  if (ppuThreadPreviousState == eThreadState::Running)
    LOG_DEBUG(Xenon, "Continuing execution on PPU{}", ppeState->ppuID);
  ppuThreadState.store(ppuThreadPreviousState.load());
  ppuThreadPreviousState.store(eThreadState::None);
  guestHalt = false;
}
void PPU::ContinueFromException() {
  if (ppuThreadState.load() == eThreadState::Running) return;
  if (ppuThreadPreviousState == eThreadState::Running) LOG_DEBUG(Xenon, "Jumping to exception handler");
  if (guestHalt) {
    sPPUThread& thread = ppeState->ppuThread[curThreadId];
    thread.RaiseExc(ppuProgramEx);
    thread.progExceptionType = ppuProgExTypeTRAP;
  }
  ppuThreadState.store(ppuThreadPreviousState.load());
  ppuThreadPreviousState.store(eThreadState::None);
  guestHalt = false;
}
void PPU::Step(int amount) {
  if (ppuThreadState.load() == eThreadState::Running) return;
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
    if (enableHalt && ppuHaltOn == curThread.NIA) { Halt(); }

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

    if (xenonContext->iic.hasPendingInterrupts(curThread.SPR.PIR)) { curThread.RaiseExc(ppuExternalEx); }

    curThread.CheckDecSignals();

    // Handle pending exceptions
    if (curThread.HasExc(SyncExceptionMask)) { PPUProcessSyncExceptions(ppeState.get()); }
    if (curThread.HasExc(AsyncExceptionMask)) { PPUProcessAsyncExceptions(ppeState.get()); }

    // If the thread was suspended due to CTRL being written, we must end
    // execution on said thread.
    if (ppeState->currentThread == 0 && ppeState->SPR.CTRL.TE0 != true) { break; }
    if (ppeState->currentThread == 1 && ppeState->SPR.CTRL.TE1 != true) { break; }

    // Break after exec and if it's halted
    if ((enableHalt && ppuThreadState == eThreadState::Halted) || ppuThreadState == eThreadState::Resetting) break;
  }
}

// PPU Thread state machine, handles all execution and codeflow
void PPU::ThreadStateMachine() {
  // Check if we should exit or not
  ppuThreadActive = ppuThreadState.load() != eThreadState::None;
  // Signal a reset if needed
  if (ppuThreadResetting) { ppuThreadState.store(eThreadState::Resetting); }
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
      if (ppeState.get()) LOG_INFO(Xenon, "PPU{} is resetting!", ppeState->ppuID);
      else LOG_INFO(Xenon, "A PPU is in the middle of resetting!");
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
  if (ppeState.get()) Base::SetCurrentThreadName("[Xe] " + ppeState->ppuName);
  while (ppuThreadActive) {
    // Start Profile
    MICROPROFILE_SCOPEI("[Xe::PPU]", "ThreadLoop", MP_AUTO);
    // Run state machine
    ThreadStateMachine();

    // If our thread is not active while running, abort early.
    // We are likely destroying the handle
    if (!ppuThreadActive) break;

    if (PPUCheckInterrupts()) continue;
  }
  // Thread is done executing, just tell it to exit
  ppuThreadActive = false;
}

// Returns a pointer to the specified thread.
sPPUThread* PPU::GetPPUThread(u8 thrdID) { return &ppeState->ppuThread[static_cast<ePPUThreadID>(thrdID)]; }

// Loads a elf binary at a specificed address
// Returns entrypoint
#define IS_ELF(ehdr)                                                                                                   \
  ((ehdr).e_ident[EI_MAG0] == ELFMAG0 && (ehdr).e_ident[EI_MAG1] == ELFMAG1 && (ehdr).e_ident[EI_MAG2] == ELFMAG2      \
   && (ehdr).e_ident[EI_MAG3] == ELFMAG3)
template<typename T> void bswap_elf(T& value) { value = byteswap_be<T>(value); }
#define SWAP(header, entry)                                                                                            \
  if (elf32) {                                                                                                         \
    bswap_elf(header entry);                                                                                           \
  } else {                                                                                                             \
    bswap_elf(header##64 entry);                                                                                       \
  }
#define READ(header, entry) elf32 ? header entry : header##64 entry
u64 PPU::loadElfImage(u8* data, u64 size) {
  // Setup HRMOR for elf binaries
  ppeState->SPR.CTRL.hexValue = 0x800000; // CTRL[TE0] = 1;
  ppeState->SPR.HRMOR.hexValue = 0x0000000000000000;

  // Loaded ELF Header type (elf32/elf64)
  bool elf32 = true; // Assume little endian file

  // We assume elf32 header unless specified otherwise
  elf32_hdr* header{reinterpret_cast<decltype(header)>(data)};
  elf64_hdr* header64{reinterpret_cast<decltype(header64)>(data)};

  // Check header:
  if (!IS_ELF(*header)) {
    LOG_CRITICAL(Xenon, "Attempting to load a binary which is not in elf "
                        "format! Killing execution.");
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
    LOG_CRITICAL(Xenon, "Header data is in little-endian format. Xbox 360 is a "
                        "BE machine. Killing execution.");
    return 0;
  } else {
    LOG_INFO(Xenon, "Header data is in big-endian format.");
  }

  // Byteswap required.
  bswap_elf(header->e_type);    // Offsett 0x10.
  bswap_elf(header->e_machine); // Offsett 0x12.
  bswap_elf(header->e_version); // Offsett 0x14.

  std::string elfType = "Unknown";
#define D_TYPE(v, t, s)                                                                                                \
  case t: v = #t ": " s; break
#define D_TYPE_L(v, t, l, s)                                                                                           \
  case t: v = #l ": " s; break
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
    LOG_CRITICAL(Xenon, "Attempting to load an ELF binary which does not "
                        "target the PowerPC/PowerPC64 ISA! Killing Execution.");
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
      LOG_INFO(Xenon,
               "Loading 0x{:X} bytes from offset 0x{:X} in the ELF to address "
               "0x{:X}",
               filesize, file_offset, target_addr);
      ppeState->mmu->MMUMemCpyFromHost(target_addr, data + file_offset, filesize);
      if (memsize > filesize) { // Memory size greater than file, zero out remainder
        u64 remainder = memsize - filesize;
        ppeState->mmu->MMUMemSet(target_addr + filesize, 0, remainder);
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
  sPPUThread& thread = ppeState->ppuThread[thrId];
  // Update previous instruction address
  thread.PIA = thread.CIA;
  // Update current instruction address
  thread.CIA = thread.NIA;
  // Increase next instruction address
  thread.NIA += 4;
  thread.instrFetch = true;
  // Fetch the instruction from memory
  _instr.opcode = ppeState->mmu->MMURead32(thread.CIA, thrId);
  if (_instr.opcode == 0xFFFFFFFF || _instr.opcode == 0xCDCDCDCD) {
    LOG_CRITICAL(Xenon,
                 "PPU{} returned an invalid opcode found. Data = {:#x}, PIA "
                 "[{:#x}] -> CIA [{:#x}]. Halting...",
                 ppeState->ppuID, _instr.opcode, thread.PIA, thread.CIA);
    Halt();
    return false;
  }
  if (thread.HasExc(ppuInstrStorageEx) || thread.HasExc(ppuInstrSegmentEx)) { return false; }
  thread.instrFetch = false;
  return true;
}

// Checks for CPU bringup interrupts
bool PPU::PPUCheckInterrupts() {
  // Check if we are allowed to enable thread zero if the thread is sleeping...
  bool WEXT = (ppeState->SPR.TSCR.hexValue & 0x100000) >> 20;

  // Check for external interrupts that enable execution
  if (ppuThreadActive && !ppuThreadResetting
      && (ppuThreadState.load() == eThreadState::Halted || ppuThreadState.load() == eThreadState::Sleeping) && WEXT) {
    // Check for an external interrupt that enables execution.
    if (!xenonContext->iic.hasPendingInterrupts(curThread.SPR.PIR, true)) { return true; }

    // Proceed.
    LOG_DEBUG(Xenon, "{} was previously halted or sleeping, bringing online", ppeState->ppuName);
    ppuThreadState.store(eThreadState::Running);

    // Enable thread 0 execution and issue a system reset exception.
    ppeState->SPR.CTRL.TE0 = 1;
    ppeState->ppuThread[ePPUThread_Zero].RaiseExc(ppuSystemResetEx);

    sPPUThread& thread = curThread;
    thread.SPR.SRR1 = 0x200000; // Set SRR1[42:44] = 100
  }

  return false;
}

// Returns current executing thread by reading CTRL register
u8 PPU::GetCurrentRunningThreads() {
  if (!ppeState) return ePPUThreadBit_None;

  // Extract bits 22-23 in one step and directly map them to thread states
  u8 ctrlTE = (ppeState->SPR.CTRL.hexValue >> 22) & 0b11;
  // If the thread state was changed to shut down both threads, set the thread
  // state to sleeping.
  if (!(ppeState->SPR.CTRL.TE0 || ppeState->SPR.CTRL.TE1)) { ppuThreadState.store(eThreadState::Sleeping); }

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
  // Since we dont actually have a FSB to make use of (nor we need one ofc) we
  // can simply bypass this.

  // Zero out Secure RAM:
  LOG_INFO(Xenon, " * Zeroing SRAM.");
  ppeState->mmu->MMUMemSet(0x10000, 0, 0x10000);

  // Verify CB's offset in NAND and fetch its header contents.
  // CB's offset should be stored in the NAND header at location 0x8.
  u32 cbOffset = ppeState->mmu->MMURead32(NAND_MEMORY_MAPPED_ADDR + 8);

  // Verification is nothing but a mere address alignment and a not zero check.
  if (cbOffset == 0) {
    LOG_CRITICAL(Xenos, "CB Offset verification failed, returned address is {:#x}.", cbOffset);
    return false;
  }

  // Read CB header, we don't print anything as SFCX code should have already
  // done this.
  Xe::PCIDev::BL_HEADER cbHeader = {};
  ppeState->mmu->MMURead(NAND_MEMORY_MAPPED_ADDR + cbOffset, 16, reinterpret_cast<u8*>(&cbHeader));

  // Byteswap header data.
  cbHeader.entryPoint = byteswap_be(cbHeader.entryPoint);
  cbHeader.length = byteswap_be(cbHeader.length);

  LOG_INFO(Xenon, " * Found CB Header at offset {:#x}, entry point {:#x}, size {:#x}.", cbOffset, cbHeader.entryPoint,
           cbHeader.length);

  // Copy CB data from NAND.
  LOG_INFO(Xenon, " * Fetching CB data.");
  std::vector<u8> cbData;
  for (size_t idx = 0; idx < cbHeader.length; idx++) {
    cbData.push_back(ppeState->mmu->MMURead8(NAND_MEMORY_MAPPED_ADDR + cbOffset + idx));
  }

  // Initialize HMAC key.

  // All good.
  return true;
}

//
// Exception Processing
//

// Exceptions in the CELL/BE and thus the Xenon, are subdivided onto two main
// categories:
// -------------------
// *** Synchronous ***
// -------------------
// * Data Storage
// * Data Segment
// * Instruction Storage
// * Instruction Segment
// * Alignment
// * Program
// * Floating-Point Unavailable
// * System Call
// * Trace
// * VXU Unavailable
// * Maintenance (instruction-caused)
// --------------------
// *** Asynchronous ***
// --------------------
// * System Reset
// * Machine Check
// * System Error
// * Decrementer
// * Hypervisor Decrementer
// * Thermal Management
// * Maintenance (system-caused)
// * External (direct and mediated)

// Aditionally, interrupts are handled in a certain priority:
// Higer to lower:
// NOTE: All load/stores handle Unavailable type interrupts
// * System reset interrupt (highest priority exception)
// * Machine check interrupt
// * Instruction-dependent
// * > Fixed-Point Loads and Stores
// * > Floating-Point Loads and Stores
// * > VXU Loads and Stores
// * > Other Instructions
// *   > Trap type of program interrupt
// *   > System call
// *   > Privileged Instruction type of program interrupt
// *   > Illegal Instruction type of program interrupt
// *   > Trace interrupt
// * > Instruction segment interrupt
// * > Instruction storage interrupt
// * Thermal management interrupt
// * System error interrupt
// * Maintenance interrupt
// * External interrupt
// * Hypervisor decrementer Interrupt
// * Decrementer Interrupt

// Process Synchronous exceptions, single priority-ordered dispatcher.
void PPU::PPUProcessSyncExceptions(sPPEState* ppeState) {
  sPPUThread& thread = curThread;

  // NOTE: Already arranged by order of execution.
  // TODO: Missing Alignment, Trace and Maintenance exceptions.
  if (thread.HasExc(ppuDataStorageEx)) {
    PPUDeliverException(ppeState, ppuDataStorageEx);
    return;
  }
  if (thread.HasExc(ppuDataSegmentEx)) {
    PPUDeliverException(ppeState, ppuDataSegmentEx);
    return;
  }
  if (thread.HasExc(ppuFPUnavailableEx)) {
    PPUDeliverException(ppeState, ppuFPUnavailableEx);
    return;
  }
  if (thread.HasExc(ppuVXUnavailableEx)) {
    PPUDeliverException(ppeState, ppuVXUnavailableEx);
    return;
  }
  if (thread.HasExc(ppuProgramEx)) {
    PPUDeliverException(ppeState, ppuProgramEx);
    return;
  }
  if (thread.HasExc(ppuSystemCallEx)) {
    PPUDeliverException(ppeState, ppuSystemCallEx);
    return;
  }
  if (thread.HasExc(ppuInstrStorageEx)) {
    PPUDeliverException(ppeState, ppuInstrStorageEx);
    return;
  }
  if (thread.HasExc(ppuInstrSegmentEx)) {
    PPUDeliverException(ppeState, ppuInstrSegmentEx);
    return;
  }
}

// Process Asynchronous exceptions.
// Priority-ordered dispatcher: System Reset and
// Machine Check are non-maskable; External/Decrementer/HV-Decrementer are gated
// by MSR[EE] (the gate is evaluated here, at delivery time, not when the
// exception was raised).
void PPU::PPUProcessAsyncExceptions(sPPEState* ppeState) {
  sPPUThread& thread = curThread;

  // NOTE: Already arranged by order of execution.
  // TODO: Missing System Error, Thermal Management and Maintenance exceptions.

  // System Reset (0x100) - non-maskable. Does not set interruptTaken so that a
  // synchronous exception can still be raised from within the reset handler.
  if (thread.HasExc(ppuSystemResetEx)) {
    PPUDeliverException(ppeState, ppuSystemResetEx);
    return;
  }

  // Machine Check (0x200, delivered via the reset vector on Xenon) -
  // non-maskable.
  if (thread.HasExc(ppuMachineCheckEx)) {
    if (thread.SPR.MSR.ME) {
      PPUDeliverException(ppeState, ppuMachineCheckEx);
      return;
    }
    // Checkstop Mode. Hard Fault. A checkstop is a full-stop of the processor
    // that requires a System Reset to recover.
    LOG_CRITICAL(Xenon, "{}: CHECKSTOP!", ppeState->ppuName);
    XeMain::ShutdownCPU();
    return;
  }

  // Maskable by MSR[EE].
  if (thread.SPR.MSR.EE) {
    // External (0x500)
    if (thread.HasExc(ppuExternalEx)) {
      PPUDeliverException(ppeState, ppuExternalEx);
      return;
    }
    // Decrementer (0x900)
    if (thread.HasExc(ppuDecrementerEx)) {
      PPUDeliverException(ppeState, ppuDecrementerEx);
      return;
    }
  }

  // Hypervisor Decrementer (0x980, saves to HSRR0/HSRR1).
  if (thread.HasExc(ppuHypervisorDecrementerEx)) {
    if (!ppeState->SPR.LPCR.HDICE) {
      thread.ClearExc(ppuHypervisorDecrementerEx);
    } else if (!thread.SPR.MSR.HV || thread.SPR.MSR.EE) {
      PPUDeliverException(ppeState, ppuHypervisorDecrementerEx);
      return;
    }
  }
}

//
// Exception definitions.
//

// Unified exception delivery core.
// A single routine drives every PPU interrupt: it saves the machine state into the appropriate save/restore register
// pair, composes the new MSR for the handler and redirects NIA to the architected vector.
void PPU::PPUDeliverException(sPPEState* ppeState, u16 excType) {
  sPPUThread& thread = curThread;

  // Per-vector descriptor.
  u64 vector = 0;         // architected interrupt vector offset.
  bool srr0IsNIA = false; // false: SRR0 = CIA (re-execute faulting insn, synchronous);
                          // true : SRR0 = NIA (resume after, asynchronous/continue).
  bool toHSRR = false;    // true: save to HSRR0/HSRR1 (hypervisor-directed).
  u64 srr1SetBits = 0;    // cause bits OR'd into the saved (H)SRR1.

  switch (excType) {
    case ppuSystemResetEx:
      vector = 0x100;
      srr0IsNIA = true;
      LOG_DEBUG(Xenon, "Thread {}: Reset Exception.", thread.SPR.PIR);
      break;
    case ppuMachineCheckEx:
      vector = 0x100;
      srr0IsNIA = true;
      break; // In Xenon, MC enters the reset handler.
    case ppuDataStorageEx: vector = 0x300; break;
    case ppuDataSegmentEx: vector = 0x380; break;
    case ppuInstrStorageEx:
      vector = 0x400;
      srr1SetBits = 0x40000000;
      break;
    case ppuInstrSegmentEx: vector = 0x480; break;
    case ppuExternalEx:
      vector = 0x500;
      srr0IsNIA = true;
      break;
    case ppuAlignmentEx: vector = 0x600; break;
    case ppuProgramEx: vector = 0x700; break; // Cause set from progExceptionType below.
    case ppuFPUnavailableEx: vector = 0x800; break;
    case ppuDecrementerEx:
      vector = 0x900;
      srr0IsNIA = true;
      break;
    case ppuHypervisorDecrementerEx:
      vector = 0x980;
      srr0IsNIA = true;
      toHSRR = true;
      break;
    case ppuSystemCallEx:
      vector = 0xC00;
      srr0IsNIA = true;
      break;
    case ppuTraceEx: vector = 0xD00; break;
    case ppuPerformanceMonitorEx: vector = 0xF00; break;
    case ppuVXUnavailableEx: vector = 0xF20; break;
    default:
      LOG_ERROR(Xenon, "[{}](Thrd{:#d}): PPUDeliverException called with unknown type {:#x}.", ppeState->ppuName,
                static_cast<s8>(curThreadId), excType);
      return;
  }

  // SRR0 = the saved program counter. In 32-bit mode (MSR[SF] = 0) the saved address is truncated to 32 bits.
  u64 savedPC = srr0IsNIA ? thread.NIA : thread.CIA;
  if (!thread.SPR.MSR.SF) savedPC = static_cast<u32>(savedPC);

  // SRR1 = (MSR & save-mask) | cause bits. The 0xFFFFFFFF87C0FFFF mask is the
  // PPC's "MSR bits copied into SRR1" set.
  u64 srr1 = (thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF) | srr1SetBits;
  if (excType == ppuProgramEx) BSET(srr1, 64, thread.progExceptionType);
  if (excType == ppuSystemResetEx)
    // Preserve the SRR1[42:44] wake cause already staged by PPUCheckInterrupts.
    srr1 |= (thread.SPR.SRR1 & 0x0000000000380000ULL);

  if (toHSRR) {
    thread.SPR.HSRR0 = savedPC;
    thread.SPR.HSRR1 = srr1;
  } else {
    thread.SPR.SRR0 = savedPC;
    thread.SPR.SRR1 = srr1;
  }

  // New MSR for the handler: clear IR/DR/EE/PR/... and force SF|HV (0x9...0).
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // Clears IR and DR (and EE/PR/...) bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;

  thread.NIA = vector;
  thread.ClearExc(excType);
}
