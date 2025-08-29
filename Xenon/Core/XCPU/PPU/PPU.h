// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <memory>

#include "PowerPC.h"

#include "Core/RootBus/RootBus.h"

class PPU_JIT;

enum class eExecutorMode : u8 {
  Interpreter,
  JIT,
  Hybrid
};

enum class eThreadState : u8 {
  None,        // Not created
  Unused,      // Should we create a handle? (Only really used in elf loading and single-core testing)
  Sleeping,    // Waiting for wakeup
  Halted,      // Halted, but ready for execution
  Running,     // Running
  Executing,   // Actively running opcodes
  Resetting,   // Recreating handle, same as halted but will resume afterwards
  Quiting      // Currently in a shutdown
};

enum class ePPUTestingMode : u8 {
 Interpreter, // Regular interpreter mode
 JITx86,      // X86 JIT mode
};

class PPU {
public:
  PPU(XENON_CONTEXT *inXenonContext, RootBus *mainBus, u64 resetVector, u32 PIR);
  ~PPU();

  // Start execution
  void StartExecution(bool setHRMOR = true);

  // Calulate our Clocks Per Instruction
  void CalculateCPI();

  // Reset the PPU state
  void Reset();

  // Debug tools
  void Halt(u64 haltOn = 0, bool requestedByGuest = false, s8 ppuId = 0, ePPUThread threadId = ePPUThread_None);
  void Continue();
  void ContinueFromException();
  void Step(int amount = 1);

  // Thread state machine
  void ThreadStateMachine();

  // Thread function
  void ThreadLoop();

  // Returns a pointer to a thread
  PPU_THREAD_REGISTERS *GetPPUThread(u8 thrdID);

  // Runs a specified number of instructions
  void PPURunInstructions(u64 numInstrs, bool enableHalt = true);

  // Sets the clocks per instruction
  void SetCPI(u32 CPI) { clocksPerInstruction = CPI; }
  // Gets the clocks per instruction
  u32 GetCPI() { return clocksPerInstruction; }

  // Checks if the thread is active
  bool ThreadActive() {
    return ppuThreadState == eThreadState::Executing ||
           ppuThreadState == eThreadState::Running;
  }

  // Checks if the thread is halted
  bool IsHalted() {
    return ppuThreadState == eThreadState::Halted;
  }

  // Checks if the thread is halted
  bool IsHaltedByGuest() {
    return guestHalt && IsHalted();
  }

  // Returns the thread state
  eThreadState ThreadState() { return ppuThreadState; }

  // Get ppuState
  PPU_STATE *GetPPUState() { return ppuState.get(); }

  // Load a elf image from host memory. Copies into RAM
  // Returns entrypoint
  u64 loadElfImage(u8 *data, u64 size);

  FILE *traceFile;

  eExecutorMode currentExecMode = eExecutorMode::Interpreter;
private:
  // Thread handle
  std::thread ppuThread;

  // PPU running?
  std::atomic<eThreadState> ppuThreadState = eThreadState::None;

  // Thread active?
  volatile bool ppuThreadActive = true;

  // Thread resetting?
  volatile bool ppuThreadResetting = false;

  // PPU thread state before halting
  std::atomic<eThreadState> ppuThreadPreviousState = eThreadState::None;

  // If this is set to a non-zero value, it will halt on that address then clear it
  u64 ppuHaltOn = 0;

  // If this is set, then the guest requested us to halt. Opens another option in the debugger
  bool guestHalt = false;

  // Amount of instructions to step
  u64 ppuStepAmount = 0;

  // Execution threads inside this PPU.
  std::unique_ptr<PPU_STATE> ppuState;

  // Main CPU Context.
  XENON_CONTEXT *xenonContext = nullptr;

  // Amount of CPU clocls per instruction executed.
  u32 clocksPerInstruction = 0;

  // Initial reset vector
  u32 resetVector = 0;
  
  //
  // Exceptions
  //

  void PPUSystemResetException(PPU_STATE* ppuState);
  void PPUInstStorageException(PPU_STATE* ppuState);
  void PPUDataStorageException(PPU_STATE* ppuState);
  void PPUDataSegmentException(PPU_STATE* ppuState);
  void PPUInstSegmentException(PPU_STATE* ppuState);
  void PPUSystemCallException(PPU_STATE* ppuState);
  void PPUDecrementerException(PPU_STATE* ppuState);
  void PPUProgramException(PPU_STATE* ppuState);
  void PPUExternalException(PPU_STATE* ppuState);
  void PPUFPUnavailableException(PPU_STATE* ppuState);
  void PPUVXUnavailableException(PPU_STATE* ppuState);

  //
  // JIT
  //

  std::unique_ptr<PPU_JIT> ppuJIT;
  friend class PPU_JIT;
  // Function call epilogue.
  friend bool CallEpilogue(PPU *ppu, PPU_STATE *ppuState);

  //
  // Helpers
  //
 
  // Returns the number of instructions per second the current
  // host computer can process.
  u32 GetIPS();
  // Read next intruction from memory
  bool PPUReadNextInstruction();
  // Checks for pending exceptions
  bool PPUCheckInterrupts();
  // Checks for pending exceptions
  bool PPUCheckExceptions();
  // Checks if it should update the time base
  void CheckTimeBaseStatus();
  // Updates the current PPU's time base and decrementer based on
  // the amount of ticks per instr we should perform.
  void UpdateTimeBase();
  // Gets the current running threads.
  u8 GetCurrentRunningThreads();
  // Simulates the behavior of the 1BL inside the Xenon Secure ROM.
  bool Simulate1Bl();

  //
  // Testing Utilities
  //
  
  // Runs instruction tests on the desired backend.
  bool RunInstructionTests(PPU_STATE* ppuState, PPU_JIT* ppuJITPtr, ePPUTestingMode testMode);
};
