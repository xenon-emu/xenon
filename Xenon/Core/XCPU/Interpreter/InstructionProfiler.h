/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstddef>

namespace PPCInterpreter {

  enum eInstrProfileDumpType : unsigned int {
    ALU = 1 << 0,
    VXU = 1 << 1,
    FPU = 1 << 2,
    LS = 1 << 3,
    SYS = 1 << 4,
    ALL = ALU | VXU | FPU | LS | SYS
  };

  // Quick instruction profiler for PPC Interpreter.
  class InstructionProfiler {
  public:
    static InstructionProfiler &Get() noexcept;

    // Ahora acepta el nombre de la instrucci�n (string) en lugar de un �ndice de tabla.
    void Increment(const std::string &instrName) noexcept;

    // Reset all counters.
    void Reset() noexcept;

    // Dumps Top instructions counts.
    void DumpTopAll(size_t topN = 20) const noexcept;

    // Dumps instruction counts based on the dumpType flags.
    void DumpInstrCounts(eInstrProfileDumpType dumpType, size_t topN = 20);

    InstructionProfiler() noexcept;
    ~InstructionProfiler();

  private:
    void DumpTopALU(size_t topN = 20) const noexcept;
    void DumpTopFPU(size_t topN = 20) const noexcept;
    void DumpTopVXU(size_t topN = 20) const noexcept;
    void DumpTopLS(size_t topN = 20) const noexcept;
    void DumpTopSYS(size_t topN = 20) const noexcept;
    std::unordered_map<std::string, std::shared_ptr<std::atomic<u64>>> counters_;
    mutable std::shared_mutex countersMutex_;
  };

} // namespace PPCInterpreter