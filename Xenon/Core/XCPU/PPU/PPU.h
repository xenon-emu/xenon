/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/XCPU/Context/XenonContext.h"
#include "Core/XCPU/MMU/XenonMMU.h"
#include "PowerPC.h"

#include <atomic>
#include <memory>
#include <thread>

// Describes the execution backends available for the PPU.
// TODO: Move to the execution backend system once we have more than one.
enum class eExecutorMode : u8 {
  Interpreter,
};

// Current 'testing' mode. Used for execution backend testing.
// Same as above, move to the execution backend system.
enum class ePPUTestingMode : u8 {
  Interpreter, // Regular interpreter mode
};

// Current PPU Thread State.
enum class eThreadState : u8 {
  None,      // Not created
  Sleeping,  // Waiting for wakeup
  Halted,    // Halted, but ready for execution
  Running,   // Running
  Resetting, // Recreating handle, same as halted but will resume afterwards
  Quiting    // Currently in a shutdown
};

// Power Procesing Unit. Main execution unit inside the PPE's within the Xenon CPU.
class PPU {
public:
  // Constructor
  PPU(Xe::XCPU::XenonContext* inXenonContext, u64 resetVector, u32 PIR);
  // Destructor
  ~PPU();

  // Start execution of the PPU, initialize the state machine and loop on the first thread.
  // @param setHRMOR: Set HRMOR register to 0x200'0000'0000, as it would seem default on the Xenon.
  void StartExecution(bool setHRMOR = true);

  // Reset both Guest Threads.
  void ResetGuestThreads();

  // Checks if any PPU thread is halted.
  bool IsHalted() {
    return AnyThread([](const sThreadRunState& rs) { return rs.state.load() == eThreadState::Halted; });
  }

  // Checks if any PPU thread is halted due to a guest request.
  bool IsHaltedByGuest() {
    return AnyThread([](const sThreadRunState& rs) { return rs.guestHalt && rs.state.load() == eThreadState::Halted; });
  }

  // Returns a pointer to the PPE state.
  sPPEState* GetPPUState() { return ppeState.get(); }

  // Returns a pointer to a guest PPU thread.
  // @param thrdID: Guest thread ID [0-1] to return the pointer from.
  sPPUThread* GetPPUThread(u8 thrdID);

  // Load an ELF image, copies it into RAM and returns entrypoint.
  // @param data: Pointer to the ELF data stream.
  // @param size: ELF data stream size in memory.
  u64 LoadElfImage(u8* data, u64 size);

  // Current CPU exection mode backend.
  eExecutorMode currentExecMode = eExecutorMode::Interpreter;

  // Trace file stream.
  FILE* traceFile;

  // Debugging tools.
  // NOTE: threadId == ePPUThread_None targets both PPU threads.

  void HaltGuestThread(u64 haltOn = 0, bool requestedByGuest = false, ePPUThreadID threadId = ePPUThread_None);
  void ContinueFromHalt();
  void ContinueFromException();
  void StepInstructions(int amount = 1);

private:
  // Per-PPU Guest thread state. One instance per PPU thread.
  struct sThreadRunState {
    // Host thread driving this Guest PPU thread.
    std::thread hostThread;
    // Current run state.
    std::atomic<eThreadState> state = eThreadState::None;
    // Run state before halting (used to resume after a debugger halt).
    std::atomic<eThreadState> previousState = eThreadState::None;
    // Whether the host thread should keep looping.
    std::atomic<bool> active = true;
    // Whether the thread is currently resetting.
    std::atomic<bool> resetting = false;
    // If non-zero, halt when NIA reaches this address, then clear it.
    u64 haltOn = 0;
    // Set when the guest requested the halt.
    bool guestHalt = false;
    // Amount of instructions left to step while halted.
    u64 stepAmount = 0;
  };

  // Represents the state of two Guest PPU hardware threads inside this PPE.
  sThreadRunState guestThreadRunState[2];

  // Guest PPE Context.
  // Contains both guest PPU Contexts.
  std::unique_ptr<sPPEState> ppeState;

  // Main CPU Context.
  Xe::XCPU::XenonContext* xenonContext = nullptr;

  // Initial reset vector
  u32 resetVector = 0;

  // Applies a predicate to both PPU threads and returns true if any matches.
  template<typename Pred> bool AnyThread(Pred pred) const {
    return pred(guestThreadRunState[ePPUThread_Zero]) || pred(guestThreadRunState[ePPUThread_One]);
  }

  //
  // Per-thread execution
  //

  // Host thread entry point for one Guest PPU thread.
  void HostThreadLoop(ePPUThreadID thrId);

  // Guest thread state machine, handles all execution and codeflow.
  void GuestThreadStateMachine(ePPUThreadID thrId);

  // Runs a specified number of instructions on one PPU thread.
  void RunInstructions(ePPUThreadID thrId, u64 numInstrs, bool enableHalt = true);

  // Halts a single Guest PPU thread based on its thread ID.
  // @param thrId: The thread ID to halt (ePPUThread_Zero or ePPUThread_One).
  // @param haltOn: Optional address to halt on (default is 0, meaning no specific address).
  // @param requestedByGuest :Whether the halt was requested by the guest (default is false).
  void HaltGuestThreadByThreadID(ePPUThreadID thrId, u64 haltOn = 0, bool requestedByGuest = false);

  // Enables a Guest PPU thread with the specified bringup reason.
  // @param thrId: The thread ID to enable (ePPUThread_Zero or ePPUThread_One).
  // @param wakeReason: The reason for enabling the thread (power-on-reset, decrementer wake, etc.).
  void EnableGuestThread(ePPUThreadID thrId, eThreadWakeReason wakeReason);

  //
  // Exceptions
  //

  // Check and process any pending exceptions for the current thread.
  void CheckAndProcessExceptions(sPPEState* ppeState);

  // Process Synchronous exceptions.
  void ProcessSyncExceptions(sPPEState* ppeState);

  // Process Asynchronous exceptions.
  void ProcessAsyncExceptions(sPPEState* ppeState);

  // Delivers an exception to the current thread.
  void DeliverGuestException(sPPEState* ppeState, u16 excType);

  // Checks for external exceptions from the IIC and DEC/HDEC and raises them.
  void CheckAndRaiseExternalExceptions(sPPEState* ppeState);

  // Checks for pending bring-up/enable exceptions for the given thread.
  void CheckForGuestThreadEnablingExceptions(ePPUThreadID thrId);

  //
  // Helpers
  //

  // Read next intruction from memory for the given thread.
  bool ReadNextInstruction(ePPUThreadID thrId);

  // Simulates the behavior of the 1BL inside the Xenon Secure ROM.
  bool Simulate1Bl();

  //
  // Testing Utilities
  //

  // Runs instruction tests on the desired backend.
  bool RunInstructionTests(sPPEState* ppeState, ePPUTestingMode testMode);
};
