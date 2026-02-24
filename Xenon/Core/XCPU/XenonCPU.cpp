/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Thread.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/XenonCPU.h"
#include "Interpreter/PPCInterpreter.h"

#include "Base/PrecisionTimer.h"

namespace Xe::XCPU {

  XenonCPU::XenonCPU(RootBus *inBus, const std::string blPath, const std::string fusesPath, RAM *ramPtr) {
    // Initilize Xenon Context
    xenonContext = std::make_unique<STRIP_UNIQUE(xenonContext)>(inBus, ramPtr);

    // Set SROM to 0.
    memset(xenonContext->SROM.get(), 0, XE_SROM_SIZE);

    // Populate FuseSet
    {
      std::ifstream file(fusesPath);
      if (!file.is_open()) {
        xenonContext->socSecOTPBlock->sec->AsULONGLONG = { 0x9999999999999999 };
      }
      else {
        LOG_INFO(System, "Current FuseSet:");
        std::vector<std::pair<std::string, u64>> fusesets{};
        std::string fuseset;
        while (std::getline(file, fuseset)) {
          if (size_t pos = fuseset.find(": "); pos != std::string::npos) {
            fuseset = fuseset.substr(pos + 2);
          }
          u64 fuse = strtoull(fuseset.c_str(), nullptr, 16);
          LOG_INFO(System, " * FuseSet {:02}: 0x{:X}", fusesets.size(), fuse);
          fusesets.push_back(std::make_pair(fuseset, fuse));
        }

        xenonContext->socSecOTPBlock->sec[0].AsULONGLONG = fusesets[0].second;
        xenonContext->socSecOTPBlock->ConsoleType[0] = fusesets[1].second;
        xenonContext->socSecOTPBlock->ConsoleSequence[0] = fusesets[2].second;
        xenonContext->socSecOTPBlock->UniqueId1[0] = fusesets[3].second;
        xenonContext->socSecOTPBlock->UniqueId2[0] = fusesets[4].second;
        xenonContext->socSecOTPBlock->UniqueId3[0] = fusesets[5].second;
        xenonContext->socSecOTPBlock->UniqueId4[0] = fusesets[6].second;
        xenonContext->socSecOTPBlock->UpdateSequence[0] = fusesets[7].second;
        xenonContext->socSecOTPBlock->EepromKey1[0] = fusesets[8].second;
        xenonContext->socSecOTPBlock->EepromKey2[0] = fusesets[9].second;
        xenonContext->socSecOTPBlock->EepromHash1[0] = fusesets[10].second;
        xenonContext->socSecOTPBlock->EepromHash2[0] = fusesets[11].second;
      }

      // Start timebase timer thread.
      if (!timeBaseThreadActive.load()) {
        timeBaseThreadActive.store(true);
        timeBaseThread = std::thread(&XenonCPU::timeBaseThreadLoop, this);
      }
    }

    // Load 1BL binary if needed.
    if (!Config::xcpu.simulate1BL) {
      // Load 1BL from path.
      std::ifstream file(blPath, std::ios_base::in | std::ios_base::binary);
      if (!file.is_open()) {
        LOG_CRITICAL(Xenon, "Unable to open file: {} for reading. Check your file path. System Stopped!", blPath);
        Base::SystemPause();
      }
      else {
        u64 fileSize = 0;
        // fs::file_size can cause a exception if it is not a valid file
        try {
          std::error_code ec;
          fileSize = std::filesystem::file_size(blPath, ec);
          if (fileSize == -1 || !fileSize) {
            fileSize = 0;
            LOG_ERROR(Base_Filesystem, "Failed to retrieve the file size of {} (Error: {})", blPath, ec.message());
          }
        }
        catch (const std::exception &ex) {
          LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Reason: {}", ex.what());
          return;
        }

        if (fileSize == XE_SROM_SIZE) {
          file.read(reinterpret_cast<char *>(xenonContext->SROM.get()), XE_SROM_SIZE);
          LOG_INFO(Xenon, "1BL Loaded.");
        }
      }
      file.close();
    }

    // Asign Interpreter global CPU context
    PPCInterpreter::xenonContext = xenonContext.get();

    // Setup SOC blocks.
    xenonContext->socPRVBlock.get()->PowerOnResetStatus.AsBITS.SecureMode = 1; // CB Checks this.
    xenonContext->socPRVBlock.get()->PowerManagementControl.AsULONGLONG = 0x382C00000000B001ULL; // Power Management Control.
  }

  XenonCPU::~XenonCPU() {
    // First signal timer thread to stop and wait for it to exit.
    timeBaseThreadActive.store(false);

    // Ensure thread is joined before destroying resources it may touch.
    try {
      if (timeBaseThread.joinable()) {
        timeBaseThread.join();
      }
    } catch (const std::system_error &e) {
      LOG_ERROR(Xenon, "Failed to join timeBaseThread: {}", e.what());
      // Proceed with shutdown; std::terminate would be worse here.
    }

    LOG_INFO(Xenon, "Shutting PPU cores down...");
    ppu0.reset();
    ppu1.reset();
    ppu2.reset();
    xenonContext.reset();
  }

  void XenonCPU::Start(u64 resetVector) {
    // If we already have active objects, halt cpu and kill threads
    if (ppu0.get()) {
      Halt();
      ppu0.reset();
      ppu1.reset();
      ppu2.reset();
    }
    // Create PPU elements
    ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(xenonContext.get(), resetVector, 0); // Threads 0-1
    ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(xenonContext.get(), resetVector, 2); // Threads 2-3
    ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(xenonContext.get(), resetVector, 4); // Threads 4-5
    // Start execution on the main thread
    ppu0->StartExecution();
    // Start execution on the other threads
    ppu1->StartExecution();
    ppu2->StartExecution();
  }

  void XenonCPU::LoadElf(const std::string path) {
    ppu0.reset();
    ppu1.reset();
    ppu2.reset();
    ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(xenonContext.get(), 0, 0); // Threads 0-1
    ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(xenonContext.get(), 0, 2); // Threads 2-3
    ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(xenonContext.get(), 0, 4); // Threads 4-5
    std::filesystem::path filePath{ path };
    std::ifstream file{ filePath, std::ios_base::in | std::ios_base::binary };
    u64 fileSize = 0;
    // fs::file_size can cause a exception if it is not a valid file
    try {
      std::error_code ec;
      fileSize = std::filesystem::file_size(filePath, ec);
      if (fileSize == -1 || !fileSize) {
        fileSize = 0;
        LOG_ERROR(Base_Filesystem, "Failed to retrieve the file size of {} (Error: {})", filePath.string(), ec.message());
      }
    }
    catch (const std::exception &ex) {
      LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Exception: {}",
        ex.what());
      return;
    }
    std::unique_ptr<u8[]> elfBinary = std::make_unique<u8[]>(fileSize);
    file.read(reinterpret_cast<char *>(elfBinary.get()), fileSize);
    file.close();
    ppu0->loadElfImage(elfBinary.get(), fileSize);
    // Start execution on the main thread
    ppu0->StartExecution(false);
    // Start execution on the other threads
    ppu1->StartExecution(false);
    ppu2->StartExecution(false);
  }

  void XenonCPU::Reset() {
    if (ppu0.get())
      ppu0->Reset();
    std::this_thread::sleep_for(200ms);
    if (ppu1.get())
      ppu1->Reset();
    std::this_thread::sleep_for(200ms);
    if (ppu2.get())
      ppu2->Reset();
    std::this_thread::sleep_for(200ms);
  }

  void XenonCPU::Halt(u64 haltOn, bool requestedByGuest, u8 ppuId, ePPUThreadID threadId) {
    if (ppu0.get())
      ppu0->Halt(haltOn, requestedByGuest, ppuId, threadId);
    if (ppu1.get())
      ppu1->Halt(haltOn, requestedByGuest, ppuId, threadId);
    if (ppu2.get())
      ppu2->Halt(haltOn, requestedByGuest, ppuId, threadId);
  }

  void XenonCPU::Continue() {
    if (ppu0.get())
      ppu0->Continue();
    if (ppu1.get())
      ppu1->Continue();
    if (ppu2.get())
      ppu2->Continue();
  }

  void XenonCPU::ContinueFromException() {
    if (ppu0.get())
      ppu0->ContinueFromException();
    if (ppu1.get())
      ppu1->ContinueFromException();
    if (ppu2.get())
      ppu2->ContinueFromException();
  }

  void XenonCPU::Step(int amount) {
    if (ppu0.get())
      ppu0->Step(amount);
    if (ppu1.get())
      ppu1->Step(amount);
    if (ppu2.get())
      ppu2->Step(amount);
  }

  bool XenonCPU::IsHalted() {
    if (ppu0.get() && ppu0->IsHalted()) {
      return true;
    }
    if (ppu1.get() && ppu1->IsHalted()) {
      return true;
    }
    if (ppu2.get() && ppu2->IsHalted()) {
      return true;
    }
    return false;
  }

  bool XenonCPU::IsHaltedByGuest() {
    if (ppu0.get() && ppu0->IsHaltedByGuest()) {
      return true;
    }
    if (ppu1.get() && ppu1->IsHaltedByGuest()) {
      return true;
    }
    if (ppu2.get() && ppu2->IsHaltedByGuest()) {
      return true;
    }
    return false;
  }

  PPU *XenonCPU::GetPPU(u8 ppuID) {
    switch (ppuID) {
    case 0:
      return ppu0.get();
    case 1:
      return ppu1.get();
    case 2:
      return ppu2.get();
    }

    return nullptr;
  }

  // TimeBase thread for increasing global XCPU Timer counter.
  // Uses a hybrid sleep+spin approach for accuracy with low CPU usage:
  // - Sleeps for the bulk of the interval using OS-assisted precision sleep
  // - Spins for the final ~1µs to hit the target precisely
  // - Uses elapsed-based tick calculation for accuracy
  // - Compensates for drift via absolute deadline advancement
  void XenonCPU::timeBaseThreadLoop() {
    Base::SetCurrentThreadName("[Xe] CPU Timer Thread");

    // Request critical priority for the current thread
    Base::SetCurrentThreadPriority(Base::ThreadPriority::Critical);

    // Xbox 360 timebase frequency: 50 MHz → 20 ns per tick.
    constexpr u64 NS_PER_TICK = 20ULL;

    // Target update interval in nanoseconds.
    // 100 us = 5000 ticks at 50 MHz. This isn't ideal but should work most of the time due to
    // it being higher than most OS's timers 50us resolution.
    constexpr u64 TARGET_INTERVAL_NS = 100'000ULL;

    // Sleep threshold: spin for the final portion to hit the deadline precisely.
    // Since WaitableTimers on Windows can't reliably achieve sub-500µs precision,
    // and our target interval is us, we set the spin threshold equal to the
    // target so the entire interval is handled by the CPU-friendly _mm_pause spin.
    // The OS sleep only activates if TARGET_INTERVAL is raised above this value.
    constexpr u64 SPIN_THRESHOLD_NS = TARGET_INTERVAL_NS;

    // Maximum ticks to apply in a single update. Prevents decrementer storms
    // after system suspend/resume or long stalls.
    constexpr u64 MAX_TICKS_PER_UPDATE = 50000ULL; // 1ms worth of ticks

    u64 lastUpdateNs = Base::GetMonotonicNanos();
    u64 nextDeadlineNs = lastUpdateNs + TARGET_INTERVAL_NS;

    while (timeBaseThreadActive.load(std::memory_order_relaxed)) {
      u64 now = Base::GetMonotonicNanos();

      // Sleep phase: use absolute-deadline sleep for the bulk of the wait.
      // Absolute sleep eliminates the timing gap between measuring "now" and
      // entering the kernel sleep — the OS sleeps until the wall-clock deadline
      // rather than for a computed relative duration.
      u64 sleepDeadline = nextDeadlineNs - SPIN_THRESHOLD_NS;
      if (now < sleepDeadline) {
        Base::PrecisionSleepUntil(sleepDeadline);
      }

      // Spin phase: busy-wait for the remaining interval with CPU-friendly hints.
      // We batch multiple _mm_pause instructions between QPC checks to reduce
      // measurement overhead. Each QPC call costs ~25-50ns; batching 8 pauses
      // cuts QPC call frequency by 8x, giving ~4x tighter deadline precision.
      while ((now = Base::GetMonotonicNanos()) < nextDeadlineNs) {
        Base::SpinHint();
        Base::SpinHint();
        Base::SpinHint();
        Base::SpinHint();
        Base::SpinHint();
        Base::SpinHint();
        Base::SpinHint();
        Base::SpinHint();
      }

      // Calculate elapsed time since last update and convert to ticks.
      u64 elapsedNs = now - lastUpdateNs;
      u64 ticks = elapsedNs / NS_PER_TICK;

      // Clamp to prevent storms after long stalls (e.g. system suspend).
      if (ticks > MAX_TICKS_PER_UPDATE) {
        ticks = MAX_TICKS_PER_UPDATE;
      }

      if (ticks > 0 && xenonContext->timeBaseActive.load(std::memory_order_relaxed)) {
        ppu0->UpdateTimeBase(ticks);
        ppu1->UpdateTimeBase(ticks);
        ppu2->UpdateTimeBase(ticks);
      }

      lastUpdateNs = now;

      // Advance deadline by TARGET_INTERVAL_NS (drift compensation).
      // If we overshot, the next interval will be shorter to catch up.
      nextDeadlineNs += TARGET_INTERVAL_NS;

      // If we've fallen too far behind (> 1ms), reset the deadline
      // to avoid a burst of rapid catch-up updates.
      if (now > nextDeadlineNs + 1'000'000ULL) {
        nextDeadlineNs = now + TARGET_INTERVAL_NS;
      }
    }
  }

} // Xe::XCPU