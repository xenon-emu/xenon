/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <filesystem>

#include "Core/XCPU/PPU/PPU.h"
#include "Core/RootBus/RootBus.h"

namespace Xe::XCPU {

  // Xenon - Xbox 360 CPU Implementation.
  //
  // Contains:
  // - 3 PPU's with SMT and VMX support.
  // - 1MB L2 Cache with custom address decoding logic for hashing and crypto on
  // L2 Cache.
  // - Pseudo Random Number Generator.
  // - 64 Kb SRAM.
  // - 32 Kb SROM.
  // - 768 bits of IBM's eFuse technology.
  class XenonCPU {
  public:
    XenonCPU(RootBus *inBus, const std::string blPath, const std::string fusesPath, RAM *ramPtr);
    ~XenonCPU();

    // Starts the CPU at the given reset vector. (Usually address 0x100).
    void Start(u64 resetVector = 0x100);
    // Resets the CPU to POR state and efectively restarts execution.
    void Reset();
    // Halts one or more cores.
    void Halt(u64 haltOn = 0, bool requestedByGuest = false, u8 ppuId = 0, ePPUThreadID threadId = ePPUThread_Zero);
    // Continues execution on all enabled cores after a Halt was issued.
    void Continue();
    // Resumes execution from a previously ocurred exception.
    void ContinueFromException();
    // Steps a given amount of cyles.
    void Step(int amount = 1);
    // Runs a test to determine the 'Clocks per Instruction' the CPU implementation should increase in order to effectively
    // step the time base.
    u32 RunCPITests(u64 resetVector = 0x100);
    // Loads an PowerPC ELF file and starts its execution.
    void LoadElf(const std::string path);
    // Returns true if halted.
    bool IsHalted();
    // Returns true of the halt was due to a 'trap' guest exception.
    bool IsHaltedByGuest();
    // Returns the IIC pointer from our context.  
    IIC::XenonIIC *GetIICPointer() { return &xenonContext->xenonIIC; }
    // Returns a pointer to a given PPU.
    PPU *GetPPU(u8 ppuID);
    // Returns the current CPI value.
    u32 GetCPI() { return sharedCPI; }

  private:
    // Global Xenon CPU Content (shared between PPUs)
    std::unique_ptr<XenonContext> xenonContext;

    // TimeBase frequency timer
    std::chrono::high_resolution_clock::time_point timeBaseUpdate{};

    // High resolution timer thread for accumulating timebase ticks.
    std::thread timeBaseThread{};
    std::atomic<bool> timeBaseThreadActive{ false };
    // Timer thread loop function.
    void timeBaseThreadLoop();

    // The CPI shared across all cores, used for accurate time base emulation. 
    u32 sharedCPI = 0;

    // Power Processing Units, the effective execution units inside the Xbox 360 CPU.
    std::unique_ptr<PPU> ppu0{};
    std::unique_ptr<PPU> ppu1{};
    std::unique_ptr<PPU> ppu2{};
  };

} // Xe::XCPU