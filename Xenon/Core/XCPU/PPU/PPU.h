// Copyright 2025 Xenon Emulator Project

#pragma once

#include <memory>

#include "PowerPC.h"

#include "Core/RootBus/RootBus.h"

class PPU {
public:
  PPU(XENON_CONTEXT *inXenonContext, RootBus *mainBus, u32 PVR,
                  u32 PIR, const char *ppuName);
  ~PPU();

  void Halt();
  void Continue();
  void Step(int amount = 1);

  void StartExecution();

  // Returns a pointer to a thread.
  PPU_THREAD_REGISTERS *GetPPUThread(u8 thrdID);

  bool IsHalted() { return ppcHalt; }
private:
  // Thread handle
  std::thread ppuThread;

  // PPU running?
  bool ppuRunning = false;

  // Can we quit?
  bool ppuExecutionDone = true;

  // Halt thread
  bool ppcHalt = false;

  // Should we single step?
  bool ppcStep = false;
  // Amount of instructions to step
  int ppcStepAmount = 1;
  // How many times we have stepped since activating it
  // Counter ticks up until step amount is reached
  int ppcStepCounter = 0;

  // Reset ocurred or signaled?
  bool systemReset = false;

  // Execution threads inside this PPU.
  std::shared_ptr<PPU_STATE> ppuState;

  // Main CPU Context.
  XENON_CONTEXT *xenonContext = nullptr;

  // Amount of CPU ticks per instruction executed.
  u32 ticksPerInstruction = 0;

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
