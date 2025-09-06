// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <memory>

#include "PowerPC.h"

#include "Core/RootBus/RootBus.h"
#include "Core/XCPU/XenonMMU/XenonMMU.h"

class PPU_JIT;

// Describes the execution backends available for the PPU.
enum class eExecutorMode : u8 {
  Interpreter,
  JIT,
  Hybrid
};

// Current PPU Thread State.
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

// Current 'testing' mode. Used for execution backend testing.
enum class ePPUTestingMode : u8 {
 Interpreter, // Regular interpreter mode
 JITx86,      // X86 JIT mode
};

// Power Procesing Unit. Main execution unit inside the PPE's within the Xenon CPU.
class PPU {
public:
  PPU(XenonContext*inXenonContext, u64 resetVector, u32 PIR);
  ~PPU();

  // Start execution
  void StartExecution(bool setHRMOR = true);

  // Calulate our Clocks Per Instruction
  void CalculateCPI();

  // Reset the PPU state
  void Reset();

  // Debug tools
  void Halt(u64 haltOn = 0, bool requestedByGuest = false, s8 ppuId = 0, ePPUThreadID threadId = ePPUThread_None);
  void Continue();
  void ContinueFromException();
  void Step(int amount = 1);

  // Thread state machine
  void ThreadStateMachine();

  // Thread function
  void ThreadLoop();

  // Returns a pointer to a thread
  sPPUThread *GetPPUThread(u8 thrdID);

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

  // Get ppeState
  sPPEState *GetPPUState() { return ppeState.get(); }

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
  std::unique_ptr<sPPEState> ppeState;

  // Main CPU Context.
  XenonContext *xenonContext = nullptr;

  // Xenon Memory Management Unit
  std::unique_ptr<XCPU::MMU::XenonMMU> xenonMMU;

  // Amount of CPU clocls per instruction executed.
  u32 clocksPerInstruction = 0;

  // Initial reset vector
  u32 resetVector = 0;

  //
  // Exceptions
  //

  void PPUSystemResetException(sPPEState* ppeState);
  void PPUInstStorageException(sPPEState* ppeState);
  void PPUDataStorageException(sPPEState* ppeState);
  void PPUDataSegmentException(sPPEState* ppeState);
  void PPUInstSegmentException(sPPEState* ppeState);
  void PPUSystemCallException(sPPEState* ppeState);
  void PPUDecrementerException(sPPEState* ppeState);
  void PPUProgramException(sPPEState* ppeState);
  void PPUExternalException(sPPEState* ppeState);
  void PPUFPUnavailableException(sPPEState* ppeState);
  void PPUVXUnavailableException(sPPEState* ppeState);

  //
  // JIT
  //

  std::unique_ptr<PPU_JIT> ppuJIT;
  friend class PPU_JIT;
  // Function call epilogue.
  friend bool CallEpilogue(PPU *ppu, sPPEState *ppeState);

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
  bool RunInstructionTests(sPPEState* ppeState, PPU_JIT* ppuJITPtr, ePPUTestingMode testMode);
};
