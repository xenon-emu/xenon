/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "InstructionProfiler.h"

#include "Base/Logging/Log.h"

#include <algorithm>
#include <vector>
#include <mutex>

namespace PPCInterpreter {

  namespace {

    // ALU instructions
    static const std::unordered_set<std::string> aluNames = {
      "mulli", "subfic", "cmpli", "cmpi", "addic", "addi", "addis", "rlwimix", "rlwinmx", "rlwnmx", "ori", "oris",
      "xori", "xoris", "andi", "andis", "crnor", "crandc", "crxor", "crnand", "crand", "creqv", "crorc", "cror",
      "rldiclx", "rldicrx", "rldicx", "rldimix", "rldclx", "rldcrx", "cmp", "subfcx", "subfcox", "mulhdux", "addcx", "addcox",
      "mulhwux", "mfocrf", "slwx", "cntlzwx", "sldx", "andx", "cmpl", "subfx", "subfox", "cntlzdx", "andcx", "mulhdx", "mulhwx",
      "negx", "negox", "norx", "subfex", "subfeox", "addex", "addeox", "mtocrf", "subfzex", "subfzeox", "addzex", "addzeox",
      "subfmex", "subfmeox", "mulldx", "mulldox", "addmex", "addmeox", "mullwx", "mullwox", "addx", "addox", "eqvx", "eciwx",
      "xorx", "orcx", "ecowx", "orx", "divdux", "divduox", "divwux", "divwuox", "nandx", "divdx", "divdox", "divwx", "divwox",
      "srwx", "srdx", "srawx", "sradx", "srawix", "sradix", "extshx", "extsbx", "extswx", "mcrf",
    };

    // FPU instructions
    static const std::unordered_set<std::string> fpuNames = {
      "fdivs", "fsubs", "fadds", "fsqrts", "fres", "fmuls", "fmsubs", "fmadds", "fnmsubs", "fnmadds", "mtfsb1", 
      "mcrfs", "mtfsb0", "mtfsfi", "mffs", "mtfsf", "fcmpu", "frsp", "fctiw", "fctiwz", "fdiv", "fsub", "fadd", 
      "fsqrt", "fsel", "fmul", "frsqrte", "fmsub", "fmadd", "fnmsub", "fnmadd", "fcmpo", "fneg", "fmr", "fnabs",
      "fabs", "fctid", "fctidz", "fcfid",
    };

    // VXU instructions
    static const std::unordered_set<std::string> vxuNames = {
      "vaddubm", "vmaxub", "vrlb", "vmuloub", "vaddfp", "vmrghb",
      "vpkuhum", "vadduhm", "vmaxuh", "vrlh", "vmulouh", "vsubfp", "vmrghh", "vpkuwum", "vadduwm", "vmaxuw", "vrlw",
      "vmrghw", "vpkuhus", "vpkuwus", "vmaxsb", "vslb", "vmulosb", "vrefp", "vmrglb", "vpkshus", "vmaxsh", "vslh",
      "vmulosh", "vrsqrtefp", "vmrglh", "vpkswus", "vaddcuw", "vmaxsw", "vslw", "vexptefp", "vmrglw", "vpkshss", "vsl",
      "vlogefp", "vpkswss", "vaddubs", "vminub", "vsrb", "vmuleub", "vrfin", "vspltb", "vupkhsb", "vadduhs", "vminuh",
      "vsrh", "vmuleuh", "vrfiz", "vsplth", "vupkhsh", "vadduws", "vminuw", "vsrw", "vrfip", "vspltw", "vupklsb", "vsr",
      "vrfim", "vupklsh", "vaddsbs", "vminsb", "vsrab", "vmulesb", "vcfux", "vspltisb", "vpkpx", "vaddshs", "vminsh",
      "vsrah", "vmulesh", "vcfsx", "vspltish", "vupkhpx", "vaddsws", "vminsw", "vsraw", "vctuxs", "vspltisw", "vctsxs",
      "vupklpx", "vsububm", "vavgub", "vand", "vmaxfp", "vslo", "vsubuhm", "vavguh", "vandc", "vminfp", "vsro", "vsubuwm",
      "vavguw","vor", "vxor", "vavgsb", "vnor", "vavgsh", "vsubcuw", "vavgsw", "vsububs", "mfvscr", "vsum4ubs","vsubuhs",
      "mtvscr", "vsum4shs", "vsubuws", "vsum2sws", "vsubsbs", "vsum4sbs", "vsubshs", "vsubsws", "vsumsws", "vcmpequb",
      "vcmpequh", "vcmpequwx", "vcmpeqfp", "vcmpgefp", "vcmpgtub", "vcmpgtuh", "vcmpgtuw", "vcmpgtfp", "vcmpgtsb",
      "vcmpgtsh", "vcmpgtsw", "vcmpbfp", "vmhaddshs", "vmhraddshs", "vmladduhm", "vmsumubm", "vmsummbm", "vmsumuhm",
      "vmsumuhs", "vmsumshm", "vmsumshs", "vsel", "vperm", "vsldoi", "vmaddfp", "vnmsubfp", "vsldoi128", "vperm128",
      "vaddfp128", "vsubfp128", "vmulfp128", "vmaddfp128", "vmaddcfp128", "vnmsubfp128", "vmsum3fp128", "vmsum4fp128",
      "vpkshss128", "vand128", "vpkshus128", "vandc128", "vpkswss128", "vnor128", "vpkswus128", "vor128", "vpkuhum128",
      "vxor128", "vpkuhus128", "vsel128", "vpkuwum128", "vslo128", "vpkuwus128", "vsro128", "vpermwi128", "vpkd3d128",
      "vrlimi128", "vcfpsxws128", "vcfpuxws128", "vcsxwfp128", "vcuxwfp128", "vrfim128", "vrfin128", "vrfip128",
      "vrfiz128", "vrefp128", "vrsqrtefp128", "vexptefp128", "vlogefp128", "vspltw128", "vspltisw128", "vupkd3d128",
      "vcmpeqfp128", "vcmpgefp128", "vcmpgtfp128", "vcmpbfp128", "vcmpequw128", "vrlw128", "vslw128", "vsraw128",
      "vsrw128", "vmaxfp128", "vminfp128", "vmrghw128", "vmrglw128", "vupkhsb128", "vupklsb128"
    };

    // Load/Store instructions
    static const std::unordered_set<std::string> lsNames = {
      // Loads
      "lwz", "lwzu", "lbz", "lbzu", "lhz", "lhzu", "lha", "lhau", "lmw", "lfs", "lfsu", "lfd", "lfdu", "lvsl", "lvebx",
      "lwarx", "ldx", "lwzx", "lvsr", "lvehx", "ldux", "lwzux", "lvewx", "ldarx", "lbzx", "lvx", "lbzux", "lhzx",
      "lhzux", "lwax", "lhax", "lvxl", "lwaux", "lhaux", "lvlx", "ldbrx", "lswx", "lwbrx", "lfsx", "lvrx", "lfsux",
      "lswi", "lfdx", "lfdux", "lvlxl", "lhbrx", "lvrxl", "ld", "ldu", "lwa", "lvsl128", "lvsr128", "lvewx128", 
      "lvx128", "lvxl128", "lvlx128", "lvrx128", "lvlxl128", "lvrxl128",

      // Stores
      "stw", "stwu", "stb", "stbu", "sth", "sthu", "stmw", "stfs", "stfsu", "stfd", "stfdu", "stvebx", "stdx", "stwcx",
      "stwx", "stvehx", "stdux", "stwux", "stvewx", "stdcx", "stbx", "stvx", "stbux", "sthx", "sthux", "stvxl", 
      "stvlx", "stdbrx", "stswx", "stwbrx", "stfsx", "stvrx", "stfsux", "stswi", "stfdx", "stfdux",
      "stvlxl", "sthbrx", "stvrxl", "stfiwx", "std", "stdu", "stvewx128", "stvx128", "stvxl128","stvlx128", "stvrx128",    
      "stvlxl128", "stvrxl128",
    };

    // System instructions
    static const std::unordered_set<std::string> sysNames = {
      "tdi", "twi", "bc", "sc", "b", "bclr", "rfid", "bcctr", "tw", "td", "mfmsr", "mtmsr", "tlbiel", "tlbie", "mfspr", 
      "mftb", "slbmte", "slbie", "mtspr", "slbia", "tlbsync", 
    };

    static inline bool comparePairDesc(const std::pair<std::string, u64> &a,
      const std::pair<std::string, u64> &b) {
      return a.second > b.second;
    }
  } // anonymous namespace

  InstructionProfiler &InstructionProfiler::Get() noexcept {
    static InstructionProfiler instance;
    return instance;
  }

  InstructionProfiler::InstructionProfiler() noexcept {
    // No initialization required para el mapa din�mico.
  }

  InstructionProfiler::~InstructionProfiler() = default;

  void InstructionProfiler::Increment(const std::string &instrName) noexcept {
    {
      std::shared_lock lock(countersMutex_);
      auto it = counters_.find(instrName);
      if (it != counters_.end()) {
        it->second->fetch_add(1, std::memory_order_relaxed);
        return;
      }
    }

    {
      std::unique_lock lock(countersMutex_);
      auto it = counters_.find(instrName);
      if (it == counters_.end()) {
        auto ptr = std::make_shared<std::atomic<u64>>(0);
        auto res = counters_.emplace(instrName, ptr);
        res.first->second->fetch_add(1, std::memory_order_relaxed);
      }
      else {
        it->second->fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  void InstructionProfiler::Reset() noexcept {
    std::unique_lock lock(countersMutex_);
    for (auto &p : counters_) {
      p.second->store(0, std::memory_order_relaxed);
    }
  }

  void InstructionProfiler::DumpTopAll(size_t topN) const noexcept {
    std::vector<std::pair<std::string, u64>> vec;
    {
      std::shared_lock lock(countersMutex_);
      vec.reserve(counters_.size());
      for (const auto &p : counters_) {
        u64 v = p.second->load(std::memory_order_relaxed);
        if (v) vec.emplace_back(p.first, v);
      }
    }

    if (vec.empty()) {
      LOG_INFO(Xenon, "[InstructionProfiler]: no counts recorded.");
      return;
    }

    std::sort(vec.begin(), vec.end(), comparePairDesc);
    size_t limit = std::min(topN, vec.size());

    LOG_INFO(Xenon, "[InstructionProfiler]: Top {} instructions (all):", limit);

    for (size_t i = 0; i < limit; ++i) {
      const auto &p = vec[i];
      LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, p.first);
    }
  }

  void InstructionProfiler::DumpTopALU(size_t topN) const noexcept {
    std::vector<std::pair<std::string, u64>> vec;
    {
      std::shared_lock lock(countersMutex_);
      vec.reserve(counters_.size());
      for (const auto &p : counters_) {
        if (aluNames.find(p.first) == aluNames.end()) continue;
        u64 v = p.second->load(std::memory_order_relaxed);
        if (!v) continue;
        vec.emplace_back(p.first, v);
      }
    }

    if (vec.empty()) {
      LOG_INFO(Xenon, "[InstructionProfiler]: no ALU instruction counts recorded.");
      return;
    }

    std::sort(vec.begin(), vec.end(), comparePairDesc);
    size_t limit = std::min(topN, vec.size());

    LOG_INFO(Xenon, "[InstructionProfiler]: Top {} ALU instructions (exact list):", limit);

    for (size_t i = 0; i < limit; ++i) {
      const auto &p = vec[i];
      LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, p.first);
    }
  }

  void InstructionProfiler::DumpTopFPU(size_t topN) const noexcept {
    std::vector<std::pair<std::string, u64>> vec;
    {
      std::shared_lock lock(countersMutex_);
      vec.reserve(counters_.size());
      for (const auto &p : counters_) {
        if (fpuNames.find(p.first) == fpuNames.end()) continue;
        u64 v = p.second->load(std::memory_order_relaxed);
        if (!v) continue;
        vec.emplace_back(p.first, v);
      }
    }

    if (vec.empty()) {
      LOG_INFO(Xenon, "[InstructionProfiler]: no FPU instruction counts recorded.");
      return;
    }

    std::sort(vec.begin(), vec.end(), comparePairDesc);
    size_t limit = std::min(topN, vec.size());

    LOG_INFO(Xenon, "[InstructionProfiler]: Top {} FPU instructions (exact list):", limit);

    for (size_t i = 0; i < limit; ++i) {
      const auto &p = vec[i];
      LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, p.first);
    }
  }

  void InstructionProfiler::DumpTopVXU(size_t topN) const noexcept {
    std::vector<std::pair<std::string, u64>> vec;
    {
      std::shared_lock lock(countersMutex_);
      vec.reserve(counters_.size());
      for (const auto &p : counters_) {
        if (vxuNames.find(p.first) == vxuNames.end()) continue;
        u64 v = p.second->load(std::memory_order_relaxed);
        if (!v) continue;
        vec.emplace_back(p.first, v);
      }
    }

    if (vec.empty()) {
      LOG_INFO(Xenon, "[InstructionProfiler]: no VXU instruction counts recorded.");
      return;
    }

    std::sort(vec.begin(), vec.end(), comparePairDesc);
    size_t limit = std::min(topN, vec.size());

    LOG_INFO(Xenon, "[InstructionProfiler]: Top {} VXU instructions (exact list):", limit);

    for (size_t i = 0; i < limit; ++i) {
      const auto &p = vec[i];
      LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, p.first);
    }
  }

  void InstructionProfiler::DumpTopLS(size_t topN) const noexcept {
    std::vector<std::pair<std::string, u64>> vec;
    {
      std::shared_lock lock(countersMutex_);
      vec.reserve(counters_.size());
      for (const auto &p : counters_) {
        if (lsNames.find(p.first) == lsNames.end()) continue;
        u64 v = p.second->load(std::memory_order_relaxed);
        if (!v) continue;
        vec.emplace_back(p.first, v);
      }
    }

    if (vec.empty()) {
      LOG_INFO(Xenon, "[InstructionProfiler]: no Load/Store instruction counts recorded.");
      return;
    }

    std::sort(vec.begin(), vec.end(), comparePairDesc);
    size_t limit = std::min(topN, vec.size());

    LOG_INFO(Xenon, "[InstructionProfiler]: Top {} Load/Store instructions (exact list):", limit);

    for (size_t i = 0; i < limit; ++i) {
      const auto &p = vec[i];
      LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, p.first);
    }
  }

  void InstructionProfiler::DumpTopSYS(size_t topN) const noexcept {
    std::vector<std::pair<std::string, u64>> vec;
    {
      std::shared_lock lock(countersMutex_);
      vec.reserve(counters_.size());
      for (const auto &p : counters_) {
        if (sysNames.find(p.first) == sysNames.end()) continue;
        u64 v = p.second->load(std::memory_order_relaxed);
        if (!v) continue;
        vec.emplace_back(p.first, v);
      }
    }

    if (vec.empty()) {
      LOG_INFO(Xenon, "[InstructionProfiler]: no System instruction counts recorded.");
      return;
    }

    std::sort(vec.begin(), vec.end(), comparePairDesc);
    size_t limit = std::min(topN, vec.size());

    LOG_INFO(Xenon, "[InstructionProfiler]: Top {} System instructions (exact list):", limit);

    for (size_t i = 0; i < limit; ++i) {
      const auto &p = vec[i];
      LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, p.first);
    }
  }

  void InstructionProfiler::DumpInstrCounts(eInstrProfileDumpType dumpType, size_t topN) {
    if (dumpType & ALU) { DumpTopALU(topN); }
    if (dumpType & VXU) { DumpTopVXU(topN); }
    if (dumpType & FPU) { DumpTopFPU(topN); }
    if (dumpType & LS) { DumpTopLS(topN); }
    if (dumpType & SYS) { DumpTopSYS(topN); }
  }

} // namespace PPCInterpreter