// Copyright 2025 Xenon Emulator Project

#pragma once

#include <memory>

#include "PowerPC.h"

#include "Core/RootBus/RootBus.h"

class PPU {
public:
  PPU(XENON_CONTEXT *inXenonContext, RootBus *mainBus, u64 resetVector, u32 PVR,
                  u32 PIR, const char *ppuName);
  ~PPU();

  // Start execution
  void StartExecution(bool setHRMOR = true);

  // Calulate our Clocks Per Instruction
  void CalculateCPI();
  
  // Reset the PPU state
  void Reset();

  // Debug tools
  void Halt(u64 haltOn = 0);
  void Continue();
  void Step(int amount = 1);

  // Thread function
  void Thread();

  // Returns a pointer to a thread
  PPU_THREAD_REGISTERS *GetPPUThread(u8 thrdID);

  // Returns if the PPU is halted
  bool IsHalted() { return ppuHalt; }

  // Sets the clocks per instruction
  void SetCPI(u32 CPI) { clocksPerInstruction = CPI; }
  // Gets the clocks per instruction
  u32 GetCPI() { return clocksPerInstruction; }

  // Get ppuState
  PPU_STATE *GetPPUState() { return ppuState.get(); }

  // Load a elf image from host memory. Copies into RAM
  // Returns entrypoint
  u64 loadElfImage(u8 *data, u64 size);
private:
  // Thread handle
  std::thread ppuThread;

  // PPU running?
  bool ppuRunning = false;

  // Can we quit?
  bool ppuExecutionDone = true;

  // Are we running a elf? Should we early exit?
  bool ppuElfExecution = false;

  // Halt thread
  bool ppuHalt = false;
  // Halt on startup
  bool ppuStartHalted = false;
  // If this is set to a non-zero value, it will halt on that address then clear it
  u64 ppuHaltOn = 0;

  // Should we single step?
  bool ppuStep = false;
  // Amount of instructions to step
  int ppuStepAmount = 1;
  // How many times we have stepped since activating it
  // Counter ticks up until step amount is reached
  int ppuStepCounter = 0;

  // Reset ocurred or signaled?
  bool systemReset = false;

  // Execution threads inside this PPU.
  std::shared_ptr<PPU_STATE> ppuState;

  // Main CPU Context.
  XENON_CONTEXT *xenonContext = nullptr;

  // Amount of CPU clocls per instruction executed.
  u32 clocksPerInstruction = 0;

  // Initial reset vector
  u32 resetVector = 0;

  // Helpers

  // Returns the number of instructions per second the current
  // host computer can process.
  u32 getIPS();
  // Read next intruction from memory,
  bool ppuReadNextInstruction();
  // Check for pending exceptions.
  void ppuCheckExceptions();
  // Updates the current PPU's time base and decrementer based on
  // the amount of ticks per instr we should perform.
  void updateTimeBase();
  // Gets the current running threads.
  PPU_THREAD getCurrentRunningThreads();
};
