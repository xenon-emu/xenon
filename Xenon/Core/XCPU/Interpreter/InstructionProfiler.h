/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>

namespace PPCInterpreter {

  enum eInstrProfileDumpType {
    ALU = 0x1,
    FPU = 0x2,
    VXU = 0x4,
    LS = 0x8,
    All = 0xF,
  };

// Quick instruction profiler for PPC Interpreter.
class InstructionProfiler {
public:
  static InstructionProfiler& Get() noexcept;

  // Increases the reference counter for each table entry (uses PPCDecoder).
  void Increment(u32 tableKey) noexcept;

  // Reset all counters.
  void Reset() noexcept;

  // Dumps Top instructions counts.
  void DumpTopAll(size_t topN = 20) const noexcept;

  // Dumps instruction counts based on the dumpType flags.
  void DumpInstrCounts(eInstrProfileDumpType dumpType, size_t topN = 20);

private:
  InstructionProfiler() noexcept;
  ~InstructionProfiler() = default;

  void DumpTopALU(size_t topN = 20) const noexcept;
  void DumpTopFPU(size_t topN = 20) const noexcept;
  void DumpTopVXU(size_t topN = 20) const noexcept;
  void DumpTopLS(size_t topN = 20) const noexcept;

  static constexpr size_t kTableSize = 0x20000;
  std::array<std::atomic<u64>, kTableSize> counters_;
  std::array<char, kTableSize> isALU_; // 0 = no, 1 = ALU
  std::array<char, kTableSize> isFPU_;
  std::array<char, kTableSize> isVXU_;
  std::array<char, kTableSize> isLS_;
};

} // namespace PPCInterpreter