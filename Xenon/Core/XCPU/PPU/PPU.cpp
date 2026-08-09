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

// Definition of the thread_local PPU-thread selector.
// Each PPU host thread sets this once before executing guest code.
// All theread related code distinguishes from thread 0 or thread 1 based on this.
thread_local ePPUThreadID curThreadId = ePPUThread_Zero;

// Constructor
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

  // Get execution mode. This should select from any availeable backends and set the exeecution mode to the specified
  // backend if its avialeable.
  u32 executionMode = Base::JoaatStringHash(Config::highlyExperimental.cpuExecutor);
  switch (executionMode) {
    case "Interpreted"_jLower: currentExecMode = eExecutorMode::Interpreter; break;
    default:
      LOG_WARNING(Xenon, "Invalid execution mode '{}'! Defaulting to Interpreted",
                  Config::highlyExperimental.cpuExecutor);
      currentExecMode = eExecutorMode::Interpreter;
      break;
  }

  // Initialize out PPE State pointer.
  ppeState = std::make_unique<STRIP_UNIQUE(ppeState)>();

  // Asign global Xenon context
  xenonContext = inXenonContext;

  // Create the MMU instance for this PPE.
  ppeState->mmu = std::make_shared<Xe::XCPU::MMU::XenonMMU>(xenonContext, ppeState.get());

  // If we have a specific halt address, set it here.
  guestThreadRunState[ePPUThread_Zero].haltOn = Config::debug.haltOnAddress;
  guestThreadRunState[ePPUThread_One].haltOn = Config::debug.haltOnAddress;

  // Initialize the Reservation logic and raise a system reset exception on both threads.
  for (u8 thrdNum = 0; thrdNum < 2; thrdNum++) {
    // Get the PPU Thread State for the current thread ID.
    sPPUThread& thread = ppeState->ppuThread[static_cast<ePPUThreadID>(thrdNum)];

    // Assign and register the PPU Reservation logic for this thread.
    thread.ppuRes = std::make_unique<STRIP_UNIQUE(sPPUThread::ppuRes)>();
    xenonContext->xenonRes.Register(thread.ppuRes.get());

    // Issue a system reset exception on both threads.
    thread.RaiseExc(eExceptionBitmask::ppuSystemResetEx);
  }

  // Set Processor Version Register (PVR) based on the console revision.
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

  // Set the PPU Thread ID (0 - 2).
  ppeState->ppuID = PIR / 2;

  // Set the PPU Name
  ppeState->ppuName = FMT("PPU{}", ppeState->ppuID);

  // Set Processor Information Register (PIR).
  ppeState->ppuThread[ePPUThread_Zero].SPR.PIR = PIR;
  ppeState->ppuThread[ePPUThread_One].SPR.PIR = PIR + 1;
}

// Destructor
PPU::~PPU() {
  // Signal both PPU host threads to quit, then join them.
  for (u8 thr = 0; thr < 2; ++thr) {
    guestThreadRunState[thr].state.store(eThreadState::Quiting);
    guestThreadRunState[thr].active.store(false);
  }

  // Wake any parked (Sleeping) thread so it observes the quit request.
  if (ppeState) ppeState->parkCV.notify_all();

  // Join the host threads.
  for (u8 thr = 0; thr < 2; ++thr) {
    if (guestThreadRunState[thr].hostThread.joinable()) guestThreadRunState[thr].hostThread.join();
  }

  // Reset the PPE state pointer.
  ppeState.reset();
}

// Starts execution of the PPU.
void PPU::StartExecution(bool setHRMOR) {
  // Only PPU0[thread0] comes up 'Running', every other PPU thread starts 'Sleeping' and is woken
  // either by an IPI or by its sibling enabling it via CTRL[TEx].
  for (u8 thr = 0; thr < 2; ++thr) {
    // See if the current thread is the primary thread for this PPE.
    const bool isPrimaryThread = (ppeState->ppuID == 0 && thr == ePPUThread_Zero);

    // Check if we should start halted.
    if (Config::debug.startHalted) {
      // Start halted, but remember where to resume.
      guestThreadRunState[thr].state.store(eThreadState::Halted);
      guestThreadRunState[thr].previousState.store(isPrimaryThread ? eThreadState::Running : eThreadState::Sleeping);

      LOG_DEBUG(Xenon, "{}.T{} was set to be halted, setting previous state to {}", ppeState->ppuName, thr,
                isPrimaryThread ? "Running" : "Sleeping");
    } else {
      // Only the primary thread (PPU0[thread0]) starts Running, every other thread starts Sleeping.
      guestThreadRunState[thr].state.store(isPrimaryThread ? eThreadState::Running : eThreadState::Sleeping);
      guestThreadRunState[thr].previousState.store(guestThreadRunState[thr].state.load());

      LOG_DEBUG(Xenon, "{}.T{}: Starting execution.", ppeState->ppuName, thr);
    }
  }

  // TLB Software reload Mode?
  ppeState->SPR.LPCR.hexValue = 0x402ULL;

  // HID6?
  ppeState->SPR.HID6.hexValue = 0x1803800000000ULL;

  // TSCR[WEXT] = 1??
  ppeState->SPR.TSCR.hexValue = 0x100000UL;

  // Set Thread Timeout Register
  ppeState->SPR.TTR.hexValue = 0x4000; // Docs say that the recommended value is 16K instructions.

  // Check for instruction tests on PPU0[Thread 0].
  if (Config::xcpu.runInstrTests && ppeState->ppuID == 0) {
    LOG_INFO(Xenon, "Starting PowerPC instruction tests. Testing backend: {}", "Interpreter");
    RunInstructionTests(ppeState.get(), static_cast<ePPUTestingMode>(Config::xcpu.instrTestsMode));
    // Return from execution after tests.
    return;
  }

  // If we're PPU0 then enable thread 0 and set Reset Vector if not default.
  if (ppeState->ppuID == 0 && setHRMOR) {
    // Set CTRL[TEx] to 1 to enable the thread.
    SetThreadEnable(ppeState.get(), ePPUThread_Zero);
    // Enable the thread and set the wake reason to Power-On-Reset (POR).
    EnableGuestThread(ePPUThread_Zero, WAKE_POR);
    // Set HRMOR.
    ppeState->SPR.HRMOR.hexValue = 0x20000000000ULL;
    // Reset vector if not default.
    if (resetVector != 0x100) ppeState->ppuThread[ePPUThread_Zero].NIA = resetVector;
    // Also simulate 1BL if we're told to. This happens on thread 0.
    if (Config::xcpu.simulate1BL) { Simulate1Bl(); }
  }

  // Spawn one host thread per PPU guest thread so the two guest threads run concurrently.
  guestThreadRunState[ePPUThread_Zero].hostThread = std::thread(&PPU::HostThreadLoop, this, ePPUThread_Zero);
  guestThreadRunState[ePPUThread_One].hostThread = std::thread(&PPU::HostThreadLoop, this, ePPUThread_One);
}

// Resets both guest PPU threads
void PPU::ResetGuestThreads() {
  // Signal both PPU threads to reset.
  for (u8 thr = 0; thr < 2; ++thr) {
    guestThreadRunState[thr].state.store(eThreadState::Resetting);
    guestThreadRunState[thr].previousState.store(eThreadState::None);
    guestThreadRunState[thr].resetting.store(true);
  }
}

// Halts a single PPU thread by it's thread ID.
void PPU::HaltGuestThreadByThreadID(ePPUThreadID thrId, u64 haltOn, bool requestedByGuest) {
  // Bounds check
  u8 threadIndex = static_cast<u8>(thrId);
  if (threadIndex != 0 && threadIndex != 1) {
    LOG_ERROR(Xenon, "PPU{}.T{}: Invalid thread ID {:#d} for halting!", ppeState->ppuID, threadIndex,
              static_cast<u8>(thrId));
    return;
  }

  // Get the Guest Thread Run State for the specified thread ID.
  sThreadRunState& rs = guestThreadRunState[thrId];

  // If we got a halt address, refelct it on out state.
  if (haltOn && !rs.guestHalt) {
    LOG_DEBUG(Xenon, "PPU{}.T{}: Halting on address {:#x}", ppeState->ppuID, static_cast<u8>(thrId), haltOn);
    rs.haltOn = haltOn;
  }

  // Signal wheter this halt was guest requested.
  rs.guestHalt = requestedByGuest;

  // If we have a renderer, set the debugger active for this PPU Guest thread on 'guest halt'.
#ifndef NO_GFX
  if (rs.guestHalt && XeMain::renderer) { XeMain::renderer->SetDebuggerActive(ppeState->ppuID); }
#endif

  // Save the previous state so we can resume after a halt.
  if (rs.previousState == eThreadState::None) // If we were told to ignore it, then do so
    rs.previousState.store(rs.state.load());

  // Set the state to Halted.
  rs.state.store(eThreadState::Halted);
}

// Halt both guestthreads unless specified otherwise.
void PPU::HaltGuestThread(u64 haltOn, bool requestedByGuest, ePPUThreadID threadId) {
  // If threadId is None, halt both threads. Otherwise, halt the specified thread.
  if (threadId == ePPUThread_None) {
    HaltGuestThreadByThreadID(ePPUThread_Zero, haltOn, requestedByGuest);
    HaltGuestThreadByThreadID(ePPUThread_One, haltOn, requestedByGuest);
  } else {
    HaltGuestThreadByThreadID(threadId, haltOn, requestedByGuest);
  }
}

// Continue after halt.
void PPU::ContinueFromHalt() {
  for (u8 thr = 0; thr < 2; ++thr) {
    // Get the Guest Thread Run State for the specified thread ID.
    sThreadRunState& rs = guestThreadRunState[thr];
    // Only resume halted threads.
    if (rs.state.load() == eThreadState::Running) continue;

    if (rs.previousState == eThreadState::Running)
      LOG_DEBUG(Xenon, "PPU{}.T{}: Continuing execution after halt. ", ppeState->ppuID, thr);

    // Update state from previous stored state, then clear previous state.
    rs.state.store(rs.previousState.load());
    rs.previousState.store(eThreadState::None);
    rs.guestHalt = false;
  }
}

// Continues any halted threads after an exception.
void PPU::ContinueFromException() {
  for (u8 thr = 0; thr < 2; ++thr) {
    // Get the Guest Thread Run State for the specified thread ID.
    sThreadRunState& rs = guestThreadRunState[thr];
    // Only resume halted threads.
    if (rs.state.load() == eThreadState::Running) continue;

    if (rs.previousState == eThreadState::Running)
      LOG_DEBUG(Xenon, "PPU{}.T{}: Issuing a Program Exception.", ppeState->ppuID, thr);

    // Raise the proper exception on the halted thread if it was a 'guest halt'.
    if (rs.guestHalt) {
      sPPUThread& thread = ppeState->ppuThread[thr];
      thread.RaiseExc(ppuProgramEx);
      thread.progExceptionType = ppuProgExTypeTRAP;
    }

    // Update state from previous stored state, then clear previous state.
    rs.state.store(rs.previousState.load());
    rs.previousState.store(eThreadState::None);
    rs.guestHalt = false;
  }
}

// Updates the amount of instructions to be stepped on any halted threads.
void PPU::StepInstructions(int amount) {
  for (u8 thr = 0; thr < 2; ++thr) {
    // Get the Guest Thread Run State for the specified thread ID.
    sThreadRunState& rs = guestThreadRunState[thr];
    // Only step halted threads.
    if (rs.state.load() == eThreadState::Running) continue;

    if (rs.previousState == eThreadState::Running)
      LOG_DEBUG(Xenon, "PPU{}.T{}: Continuing for {:#d} Instructions", ppeState->ppuID, thr, amount);

    // Reflect the step amount on the state.
    rs.stepAmount = amount;
  }
}

// Enables one Guest PPU thread, and sets the wake reason for the System Reset exception.
void PPU::EnableGuestThread(ePPUThreadID thrId, eThreadWakeReason wakeReason) {
  // Get the target Guest Thread State.
  sPPUThread& thread = ppeState->ppuThread[thrId];

  // Get the architected wake reason into SRR1[42:44].
  thread.SPR.SRR1 = ThreadWakeReasonToSRR1(wakeReason);

  // Issue the System Reset exception.
  DeliverGuestException(ppeState.get(), ppuSystemResetEx);

  // Set the thread state to Running so the thread loop will execute instructions.
  guestThreadRunState[thrId].state.store(eThreadState::Running);

  LOG_DEBUG(Xenon, "{}.T{} brought online (Reason: {}), issuing System Reset Exception.", ppeState->ppuName,
            static_cast<u8>(thrId), ThreadWakeReasonToString(wakeReason));
}

// Runs a burst of instructions on a single PPU thread.
void PPU::RunInstructions(ePPUThreadID thrId, u64 numInstrs, bool enableHalt) {
  // Start Profile
  MICROPROFILE_SCOPEI("[Xe::PPU]", "PPURunInstructions", MP_AUTO);
  // Get the Guest Thread Run State.
  sThreadRunState& rs = guestThreadRunState[thrId];
  // Get the PPU Thread State for the current thread ID.
  sPPUThread& thread = ppeState->ppuThread[thrId];

  // Execute while numInstr > 0.
  for (size_t instrCount = 0; instrCount < numInstrs && rs.active.load(); ++instrCount) {
    // Consume a pending bring-up if present.
    const s8 wake = thread.pendingWakeReason.exchange(-1, std::memory_order_acquire);

    // Enable the guest thread if we have a pending wake reason.
    if (wake >= eThreadWakeReason::WAKE_POR && wake < eThreadWakeReason::WAKE_MAX) {
      EnableGuestThread(thrId, static_cast<eThreadWakeReason>(wake));
    }

    // Halt if needed before executing the next instruction
    if (enableHalt && rs.haltOn == thread.NIA) { HaltGuestThread(thrId); }

    // Check external interrupts and raise an exception if found.
    CheckAndRaiseExternalExceptions(ppeState.get());

    // Handle any pending exceptions
    CheckAndProcessExceptions(ppeState.get());

    // Read next instruction
    if (ReadNextInstruction(thrId)) {
#ifdef DEBUG_BUILD
      if (traceFile) {
        const std::string instrName = PPCInterpreter::PPCInterpreter_getFullName(thread.CI.opcode);
        fprintf(traceFile, "%llx: 0x%x %s\n", thread.CIA, thread.CI.opcode, instrName.c_str());
      }
#endif
      // Start Profile
      MICROPROFILE_SCOPEI("[Xe::PPU]", "ExecuteSingleInstruction", MP_AUTO);
      // Execute instruction (resolves the active thread via the thread_local curThreadId == thrId).
      PPCInterpreter::ppcExecuteSingleInstruction(ppeState.get());
    }

    // If the thread was suspended due to CTRL being written, we must end execution on said thread.
    if (!IsThreadEnabled(ppeState.get(), thrId)) { break; }

    // Break after exec and if it's halted.
    const eThreadState st = rs.state.load();
    if ((enableHalt && st == eThreadState::Halted) || st == eThreadState::Resetting) break;
  }
}

// Guest thread state machine, handles all execution and codeflow for one PPU thread.
void PPU::GuestThreadStateMachine(ePPUThreadID thrId) {
  // Get the Guest Thread Run State.
  sThreadRunState& rs = guestThreadRunState[thrId];

  // Check if we should exit or not.
  rs.active.store(rs.state.load() != eThreadState::None);

  // Signal a reset if needed.
  if (rs.resetting.load()) { rs.state.store(eThreadState::Resetting); }

  // Is this PPU thread enabled by its CTRL[TEx] bit?
  const bool teEnabled = IsThreadEnabled(ppeState.get(), thrId);

  switch (rs.state.load()) {
    case eThreadState::Running: {
      // If the thread is disabled or was disabled via CTRL, we must sleep until awaken again.
      if (!teEnabled) {
        // Thread was disabled, park it until re-enabled.
        rs.state.store(eThreadState::Sleeping);
        break;
      }
      if (currentExecMode == eExecutorMode::Interpreter && !rs.resetting.load()) {
        // Process instructions until we reach the TTR timeout.
        RunInstructions(thrId, ppeState->SPR.TTR.hexValue, rs.haltOn != 0);
      }
    } break;
    case eThreadState::Halted: {
      // Check if we should exit or not.
      rs.active.store(rs.state.load() != eThreadState::None);

      // Handle stepping
      // TODO: Stepping is currently unimplemented!!!!!
      if (currentExecMode == eExecutorMode::Interpreter && teEnabled && rs.stepAmount > 0) {
        RunInstructions(thrId, rs.stepAmount, false);
        rs.stepAmount = 0; // Ensure step mode doesn't continue indefinitely.
      }
    } break;
    case eThreadState::Sleeping: {
      // Park until a wake event notifies the per-PPU conditional var, or a short timeout elapses.
      std::unique_lock<std::mutex> lk(ppeState->parkMutex);
      // Timeout of 2ms.
      ppeState->parkCV.wait_for(lk, 2ms);
    } break;
    case eThreadState::Resetting: {
      if (ppeState.get()) LOG_INFO(Xenon, "PPU{}.T{}: Thread is resetting!", ppeState->ppuID, static_cast<u8>(thrId));
      else LOG_INFO(Xenon, "A PPU is in the middle of resetting!");
      rs.state.store(eThreadState::None);
    } break;
    case eThreadState::Quiting: {
      // Thread's exiting. Update status.
      rs.state.store(eThreadState::None);
    } break;
    default: {
    } break;
  }
}

// PPU Host Thread Loop
void PPU::HostThreadLoop(ePPUThreadID thrId) {
  // Bind this host thread to its PPU thread ID.
  curThreadId = thrId;
  // Get the Guest Thread Run State.
  sThreadRunState& rs = guestThreadRunState[thrId];

  // Set this thread's name.
  if (ppeState.get()) Base::SetCurrentThreadName(FMT("[Xe] {}.T{}", ppeState->ppuName, static_cast<u8>(thrId)));

  // Loop while the thread is active.
  while (rs.active.load()) {
    // Start Profile
    MICROPROFILE_SCOPEI("[Xe::PPU]", "Guest Thread Loop", MP_AUTO);

    // Run the guest state machine.
    GuestThreadStateMachine(thrId);

    // If our thread is not active while running, abort early.
    // We are likely destroying the handle.
    if (!rs.active.load()) break;

    // Check enabling interrupts (external IPI, DEC, etc...)
    CheckForGuestThreadEnablingExceptions(thrId);
  }

  // Thread is done executing, just tell it to exit.
  rs.active.store(false);
}

// Returns a pointer to the specified thread.
sPPUThread* PPU::GetPPUThread(u8 thrdID) { return &ppeState->ppuThread[static_cast<ePPUThreadID>(thrdID)]; }

// Helper macros for loading ELF files and performing byteswap.
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

// Loads a elf binary at a specificed address. Returns entrypoint
u64 PPU::LoadElfImage(u8* data, u64 size) {
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

// Reads the next instruction from memory and advances the NIP accordingly. Returns false if an exception was raised
// during the read.
bool PPU::ReadNextInstruction(ePPUThreadID thrId) {
  sPPUThread& thread = ppeState->ppuThread[thrId];
  // Update previous instruction address
  thread.PIA = thread.CIA;
  // Update current instruction address
  thread.CIA = thread.NIA;
  // Increase next instruction address
  thread.NIA += 4;
  thread.instrFetch = true;
  // Fetch the instruction from memory
  thread.CI.opcode = ppeState->mmu->MMURead32(thread.CIA, thrId);
  if (thread.CI.opcode == 0xFFFFFFFF || thread.CI.opcode == 0xCDCDCDCD) {
    LOG_CRITICAL(Xenon,
                 "PPU{}.T{} returned an invalid opcode found. Data = {:#x}, PIA "
                 "[{:#x}] -> CIA [{:#x}]. Halting...",
                 ppeState->ppuID, static_cast<u8>(thrId), thread.CI.opcode, thread.PIA, thread.CIA);
    HaltGuestThread(thrId);
    return false;
  }

  // Check for storage exceptions.
  if (thread.HasStorageExceptions()) { return false; }

  thread.instrFetch = false;
  return true;
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

//*****************************************************************************
// Exception Processing
//*****************************************************************************

// Exceptions in the CELL/BE and thus the Xenon, are subdivided onto two main categories:
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

// Checks for any pending exceptions and processes them accordingly.
void PPU::CheckAndProcessExceptions(sPPEState* ppeState) {
  // Get the PPU Thread State for the current thread ID.
  sPPUThread& thread = curThread;

  // Check and process synchronous exceptions first, then asynchronous exceptions.
  // NOTE: Only one exception is processed at a time, so if any exception is found, it will be processed and the rest
  // will be handled on the next check.

  if (thread.HasExc(SyncExceptionMask)) {
    ProcessSyncExceptions(ppeState);
    return;
  }

  if (thread.HasExc(AsyncExceptionMask)) {
    ProcessAsyncExceptions(ppeState);
    return;
  }
}

// Process Synchronous exceptions.
void PPU::ProcessSyncExceptions(sPPEState* ppeState) {
  sPPUThread& thread = curThread;

  // NOTE: Already arranged by order of execution.
  // TODO: Missing Alignment, Trace and Maintenance exceptions.
  if (thread.HasExc(ppuDataStorageEx)) {
    DeliverGuestException(ppeState, ppuDataStorageEx);
    return;
  }
  if (thread.HasExc(ppuDataSegmentEx)) {
    DeliverGuestException(ppeState, ppuDataSegmentEx);
    return;
  }
  if (thread.HasExc(ppuFPUnavailableEx)) {
    DeliverGuestException(ppeState, ppuFPUnavailableEx);
    return;
  }
  if (thread.HasExc(ppuVXUnavailableEx)) {
    DeliverGuestException(ppeState, ppuVXUnavailableEx);
    return;
  }
  if (thread.HasExc(ppuProgramEx)) {
    DeliverGuestException(ppeState, ppuProgramEx);
    return;
  }
  if (thread.HasExc(ppuSystemCallEx)) {
    DeliverGuestException(ppeState, ppuSystemCallEx);
    return;
  }
  if (thread.HasExc(ppuInstrStorageEx)) {
    DeliverGuestException(ppeState, ppuInstrStorageEx);
    return;
  }
  if (thread.HasExc(ppuInstrSegmentEx)) {
    DeliverGuestException(ppeState, ppuInstrSegmentEx);
    return;
  }
}

// Process Asynchronous exceptions.
void PPU::ProcessAsyncExceptions(sPPEState* ppeState) {
  sPPUThread& thread = curThread;

  // NOTE: Already arranged by order of execution.
  // TODO: Missing System Error, Thermal Management and Maintenance exceptions.

  // System Reset (0x100) - non-maskable. Does not set interruptTaken so that a
  // synchronous exception can still be raised from within the reset handler.
  if (thread.HasExc(ppuSystemResetEx)) {
    DeliverGuestException(ppeState, ppuSystemResetEx);
    return;
  }

  // Machine Check (0x200, delivered via the reset vector on Xenon) -
  // non-maskable.
  if (thread.HasExc(ppuMachineCheckEx)) {
    if (thread.SPR.MSR.ME) {
      DeliverGuestException(ppeState, ppuMachineCheckEx);
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
      DeliverGuestException(ppeState, ppuExternalEx);
      return;
    }
    // Mediated external (0x500) - hypervisor-requested via LPCR[MER]. The hypervisor sets MER to present a virtual
    // external interrupt to the partition, it shares the 0x500 vector, and the handler distinguishes it by MER being
    // set. Havent't seen it on Xenon, but it's defined in the CBEA so it's best to have it.
    if (ppeState->SPR.LPCR.MER) {
      DeliverGuestException(ppeState, ppuExternalEx);
      return;
    }
    // Decrementer (0x900)
    if (thread.HasExc(ppuDecrementerEx)) {
      DeliverGuestException(ppeState, ppuDecrementerEx);
      return;
    }
  }

  // Hypervisor Decrementer (0x980, saves to HSRR0/HSRR1).
  if (thread.HasExc(ppuHypervisorDecrementerEx)) {
    if (!ppeState->SPR.LPCR.HDICE) {
      thread.ClearExc(ppuHypervisorDecrementerEx);
    } else if (!thread.SPR.MSR.HV || thread.SPR.MSR.EE) {
      DeliverGuestException(ppeState, ppuHypervisorDecrementerEx);
      return;
    }
  }
}

// Delivers one guest exception of the specified type.
// Saves the machine state into the appropriate save/restore register pair, composes the new MSR for the handler and
// redirects NIA to the architected vector.
void PPU::DeliverGuestException(sPPEState* ppeState, u16 excType) {
  // Get the target thread to deliver the exception to.
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
      break;
    case ppuMachineCheckEx:
      vector = 0x200;
      srr0IsNIA = true;
      break;
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
      LOG_ERROR(Xenon, "[{}](Thrd{:#d}): DeliverGuestException called with unknown type {:#x}.", ppeState->ppuName,
                static_cast<s8>(curThreadId), excType);
      return;
  }

  // SRR0 = the saved instruction address. Depending on the interrupt type it's either the NIA or the CIA.
  u64 SRR0 = srr0IsNIA ? thread.NIA : thread.CIA;
  // Truncate to 32 bits if the thread is in 32-bit mode (MSR[SF] = 0).
  if (!thread.SPR.MSR.SF) SRR0 = static_cast<u32>(SRR0);

  // SRR1 = (MSR & save-mask) | cause bits. The 0xFFFFFFFF87C0FFFF mask is precomputed PPC architecture set of MSR bits
  // saved.
  u64 SRR1 = (thread.SPR.MSR.hexValue & 0xFFFFFFFF87C0FFFF) | srr1SetBits;

  // Program exception type is saved in SRR1 for program exceptions.
  if (excType == ppuProgramEx) BSET(SRR1, 64, thread.progExceptionType);

  if (excType == ppuSystemResetEx)
    // Preserve the SRR1[42:44] wake cause already staged by PPUCheckInterrupts.
    SRR1 |= (thread.SPR.SRR1 & 0x0000000000380000ULL);

  // Set the save/restore registers for the exception. Hypervisor-directed exceptions save to HSRR0/HSRR1.
  if (toHSRR) {
    thread.SPR.HSRR0 = SRR0;
    thread.SPR.HSRR1 = SRR1;
  } else {
    thread.SPR.SRR0 = SRR0;
    thread.SPR.SRR1 = SRR1;
  }

  // New MSR for the handler: clear IR/DR/EE/PR/... and force SF|HV (0x9...0).
  thread.SPR.MSR.hexValue &= 0xFFFFFFFFFFFF10C8; // Clears IR and DR (and EE/PR/...) bits.
  thread.SPR.MSR.hexValue |= 0x9000000000000000;

  // Set the new NIA to the architected vector for the exception.
  thread.NIA = vector;

  // Clear the exception from the thread's pending exception set.
  thread.ClearExc(excType);
}

// Checks the IIC for external exceptions and both the HDEC and DEC sources and raises the appropriate exception if
// found.
void PPU::CheckAndRaiseExternalExceptions(sPPEState* ppeState) {
  // Get the PPU Thread State for the current thread ID.
  sPPUThread& thread = curThread;

  // Check Dec signals.
  thread.CheckDecSignals();

  // Check for external interrupts from the IIC. If any is indeed found raise the appropriate exception.
  if (xenonContext->iic.hasPendingInterrupts(thread.SPR.PIR)) { thread.RaiseExc(ppuExternalEx); }
}

// Checks for PPU Thread bring-up/thread-enable interrupts for a single PPU thread.
void PPU::CheckForGuestThreadEnablingExceptions(ePPUThreadID thrId) {
  // Get the active thread's Run State.
  sThreadRunState& rs = guestThreadRunState[thrId];

  // Return if the thread's not active or resetting.
  if (!rs.active.load() || rs.resetting.load()) { return; }

  // Get the Guest thread state.
  const eThreadState st = rs.state.load();
  // Return if the Thread is sleeping or halted.
  if (st != eThreadState::Halted && st != eThreadState::Sleeping) { return; }

  // Get the PPU Thread.
  sPPUThread& thread = ppeState->ppuThread[thrId];

  //
  // Thread-enable wake: our sibling set our CTRL[TEx] bit and queued our System Reset via the CTRLWR path.
  //

  const bool teSet = IsThreadEnabled(ppeState.get(), thrId);
  if (teSet && st == eThreadState::Sleeping) {
    rs.state.store(eThreadState::Running);
    return;
  }

  //
  // Napped-thread decrementer/HDEC wake. A thread that disabled itself via CTRL[TE]=0 sits Sleeping with TE clear.
  // Without this it could sleep forever through its own decrementer. Gate on TSCR[WDEC0]/[WDEC1].
  //

  if (st == eThreadState::Sleeping && !teSet) {
    uTSCR tscr{};
    tscr.hexValue = ppeState->SPR.TSCR.hexValue;
    const bool WDEC = (thrId == ePPUThread_Zero) ? tscr.WDEC0 : tscr.WDEC1;
    // Wake reason.
    s8 wakeReason = -1;

    // Check if the decrementer or hypervisor decrementer expired and wake the thread if so and the WDEC is enabled for
    // said thread. Note that the HDEC exception is discarded. We issue a system reset since the thread is sleeping.
    if (WDEC && thread.hdecExpired.load(std::memory_order_acquire)) {
      thread.hdecExpired.store(false, std::memory_order_release);
      wakeReason = WAKE_HDEC;
    } else if (WDEC && thread.decExpired.load(std::memory_order_acquire)) {
      thread.decExpired.store(false, std::memory_order_release);
      wakeReason = WAKE_DEC;
    }

    // Check for a valid wake reason.
    if (wakeReason >= 0) {
      LOG_DEBUG(Xenon, "{}.T{} woken from nap (wake reason {})", ppeState->ppuName, static_cast<u8>(thrId), wakeReason);
      // Enable the thread via CTRL.
      SetThreadEnable(ppeState.get(), thrId);
      // Update the Guest Thread State.
      rs.state.store(eThreadState::Running);
      // Store the wake reason.
      thread.pendingWakeReason.store(wakeReason, std::memory_order_release);
      return;
    }
  }

  //
  // External interrupt wakeup.
  //

  const bool WEXT = (ppeState->SPR.TSCR.hexValue & 0x100000) >> 20;
  if (WEXT && thrId == ePPUThread_Zero) {
    // Check for an external interrupt that enables execution.
    if (!xenonContext->iic.hasPendingInterrupts(thread.SPR.PIR, true)) { return; }

    // Proceed.
    rs.state.store(eThreadState::Running);

    // Enable thread 0 and post an external-wake bringup.
    SetThreadEnable(ppeState.get(), ePPUThread_Zero);
    thread.pendingWakeReason.store(WAKE_EXTERNAL, std::memory_order_release);
  }

  return;
}
