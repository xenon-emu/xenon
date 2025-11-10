/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "InstructionProfiler.h"

#include "PPCInterpreter.h"
#include "PPC_Instruction.h"
#include "Base/Logging/Log.h"

#include <vector>
#include <algorithm>
#include <unordered_set>

namespace PPCInterpreter {

InstructionProfiler& InstructionProfiler::Get() noexcept {
  static InstructionProfiler instance;
  return instance;
}

InstructionProfiler::InstructionProfiler() noexcept {
  for (auto &c : counters_) c.store(0, std::memory_order_relaxed);
  isALU_.fill(0);
  isFPU_.fill(0);
  isVXU_.fill(0);
  isLS_.fill(0);

  // ALU
  static const std::unordered_set<std::string> aluNames = {
    "addx","addox","addcx","addcox","addex","addeox","addi","addic","addis",
    "addmex","addmeox","addzex","addzeox",
    "andx","andcx","andi","andis",
    "cmp","cmpi","cmpl","cmpli",
    "cntlzdx","cntlzwx",
    "crand","crandc","creqv","crnand","crnor","cror","crorc","crxor",
    "divdx","divdux","divdox","divduox","divwx","divwox","divwux","divwuox",
    "eqvx",
    "extsbx","extshx","extswx",
    "mcrf","mftb","mfocrf","mtocrf",
    "mulli","mulldx","mulldox","mullwx","mullwox","mulhwx","mulhwux","mulhdx","mulhdux",
    "nandx","negx","negox","norx",
    "orcx","ori","oris","orx",
    "rldicx","rldcrx","rldclx","rldiclx","rldicrx","rldimix",
    "rlwimix","rlwnmx","rlwinmx",
    "sldx","slwx",
    "sradx","sradix","srawx","srawix",
    "srdx","srwx",
    "subfcx","subfcox","subfx","subfox","subfex","subfeox",
    "subfmex","subfmeox","subfzex","subfzeox","subfic",
    "xorx","xori","xoris"
  };

  // FPU
  static const std::unordered_set<std::string> fpuNames = {
    "mcrfs","faddx","fabsx","faddsx","fcmpu","fcmpo","fctiwx","fctidx","fctidzx","fctiwzx",
    "fcfidx","fdivx","fdivsx","fmaddx","fmaddsx","fmulx","fmulsx","fmrx","fmsubx","fmsubsx",
    "fnabsx","fnmaddx","fnmaddsx","fnegx","fnmsubx","fnmsubsx","frspx","fsubx","fselx",
    "fsubsx","fsqrtx","fsqrtsx","frsqrtex","mffsx","mtfsfx","mtfsb0x","mtfsb1x",
    "fctid","fctidz","fctid","fcfid","fctid"
  };

  // VXU
  static const std::unordered_set<std::string> vxuNames = {
    "dss","dst","dstst","mfvscr","mtvscr",
    "vaddfp","vaddfp128","vaddubs","vadduhm","vadduws","vaddshs","vavguh","vand","vand128","vandc","vandc128",
    "vctsxs","vctuxs","vcfsx","vcfux","vcmpbfp","vcmpbfp128","vcmpeqfp","vcmpeqfp128","vcmpequwx","vcmpequw128",
    "vcmpgefp128","vcsxwfp128","vcfpsxws128","vexptefp","vexptefp128","vnmsubfp","vnmsubfp128","vnor","vor","vor128",
    "vspltw","vmaxuw","vmaxsh","vmaxuh","vmaxsw","vminsh","vminuh","vminuw","vmaxfp","vmaxfp128","vminfp","vminfp128",
    "vmaddfp","vmulfp128","vmaddcfp128","vmrghb","vmrghh","vmrghw","vmrghw128","vmrglb","vmrglh","vmrglw","vmrglw128",
    "vperm","vperm128","vpermwi128","vrlh","vrlimi128","vrfin","vrfin128","vrefp","vrefp128","vrsqrtefp","vrsqrtefp128",
    "vsel","vsel128","vsl","vslo","vslb","vslh","vslw","vslw128","vsr","vsrh","vsrah","vsrw","vsrw128","vsraw128",
    "vsldoi","vsldoi128","vspltb","vsplth","vspltisb","vspltish","vspltisw","vspltisw128","vsubfp128","vspltw128",
    "vmsum3fp128","vmsum4fp128","vupkd3d128","vxor","vxor128","vmrghw128","vmrglw128","vmaxfp128","vminfp128"
  };

  // Load/Store
  static const std::unordered_set<std::string> lsNames = {
    // Loads
    "lbz","lbzu","lbzux","lbzx","lha","lhau","lhaux","lhax","lhbrx","lhz","lhzu","lhzux","lhzx",
    "lwz","lwzu","lwzux","lwzx","lfs","lfsu","lfsx","lfsux","lfd","lfdu","lfdx","lfdux",
    "lmw","lswi","lswi","lwarx","lwad","ld","ldu","ldx","ldux","ldbrx","lwad","lvebx","lvewx","lvxl","lvsl","lvrx","lvx",
    // Stores
    "stb","stbu","stbux","stbx","sth","sthbrx","sthu","sthux","sthx","stw","stwbrx","stwcx","stwu","stwux","stwx",
    "stfs","stfsu","stfsux","stfsx","stfd","stfdx","stfdu","stfdux","stfiwx","std","stdu","stdx","stdcx","stmw","stswi",
    "stvx","stvx128","stvewx","stvewx128","stvrx","stvrx128","stvlx","stvxl","stvlx128","stvlxl128","stvrxl","stvrxl128","stvlxl","stvlxl128"
  };

  const auto &nameTable = ppcDecoder.getNameTable();
  for (size_t i = 0; i < nameTable.size(); ++i) {
    const std::string &nm = nameTable[i];
    if (aluNames.find(nm) != aluNames.end()) {
      isALU_[i] = 1;
    }
    if (fpuNames.find(nm) != fpuNames.end()) {
      isFPU_[i] = 1;
    }
    if (vxuNames.find(nm) != vxuNames.end()) {
      isVXU_[i] = 1;
    }
    if (lsNames.find(nm) != lsNames.end()) {
      isLS_[i] = 1;
    }
  }
}

void InstructionProfiler::Increment(u32 tableKey) noexcept {
  if (tableKey < counters_.size()) {
    counters_[tableKey].fetch_add(1, std::memory_order_relaxed);
  }
}

void InstructionProfiler::Reset() noexcept {
  for (auto &c : counters_) c.store(0, std::memory_order_relaxed);
}

static inline bool comparePairDesc(const std::pair<u32, u64>& a,
                                   const std::pair<u32, u64>& b) {
  return a.second > b.second;
}

void InstructionProfiler::DumpTopAll(size_t topN) const noexcept {
  std::vector<std::pair<u32, u64>> vec;
  vec.reserve(counters_.size());
  for (u32 i = 0; i < counters_.size(); ++i) {
    auto v = counters_[i].load(std::memory_order_relaxed);
    if (v) vec.emplace_back(i, v);
  }

  if (vec.empty()) {
    LOG_INFO(Xenon, "[InstructionProfiler]: no counts recorded.");
    return;
  }

  std::sort(vec.begin(), vec.end(), comparePairDesc);
  const auto &nameTable = ppcDecoder.getNameTable();
  size_t limit = std::min(topN, vec.size());

  LOG_INFO(Xenon, "[InstructionProfiler]: Top {} instructions (all):", limit);

  for (size_t i = 0; i < limit; ++i) {
    const auto &p = vec[i];
    const std::string &name = nameTable[p.first];
    LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, name);
  }
}

void InstructionProfiler::DumpTopALU(size_t topN) const noexcept {
  std::vector<std::pair<u32, u64>> vec;
  vec.reserve(counters_.size());
  const auto &nameTable = ppcDecoder.getNameTable();
  for (u32 i = 0; i < counters_.size(); ++i) {
    if (!isALU_[i]) continue;
    auto v = counters_[i].load(std::memory_order_relaxed);
    if (!v) continue;
    vec.emplace_back(i, v);
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
    const std::string &name = nameTable[p.first];
    LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, name);
  }
}

void InstructionProfiler::DumpTopFPU(size_t topN) const noexcept {
  std::vector<std::pair<u32, u64>> vec;
  vec.reserve(counters_.size());
  const auto &nameTable = ppcDecoder.getNameTable();
  for (u32 i = 0; i < counters_.size(); ++i) {
    if (!isFPU_[i]) continue;
    auto v = counters_[i].load(std::memory_order_relaxed);
    if (!v) continue;
    vec.emplace_back(i, v);
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
    const std::string &name = nameTable[p.first];
    LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, name);
  }
}

void InstructionProfiler::DumpTopVXU(size_t topN) const noexcept {
  std::vector<std::pair<u32, u64>> vec;
  vec.reserve(counters_.size());
  const auto &nameTable = ppcDecoder.getNameTable();
  for (u32 i = 0; i < counters_.size(); ++i) {
    if (!isVXU_[i]) continue;
    auto v = counters_[i].load(std::memory_order_relaxed);
    if (!v) continue;
    vec.emplace_back(i, v);
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
    const std::string &name = nameTable[p.first];
    LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, name);
  }
}

void InstructionProfiler::DumpTopLS(size_t topN) const noexcept {
  std::vector<std::pair<u32, u64>> vec;
  vec.reserve(counters_.size());
  const auto &nameTable = ppcDecoder.getNameTable();
  for (u32 i = 0; i < counters_.size(); ++i) {
    if (!isLS_[i]) continue;
    auto v = counters_[i].load(std::memory_order_relaxed);
    if (!v) continue;
    vec.emplace_back(i, v);
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
    const std::string &name = nameTable[p.first];
    LOG_INFO(Xenon, "  {:3} : {:>12} hits - {}", i + 1, p.second, name);
  }
}

void InstructionProfiler::DumpInstrCounts(eInstrProfileDumpType dumpType, size_t topN) {
  if(dumpType & ALU) { DumpTopALU(topN); }
  if(dumpType & VXU) { DumpTopALU(topN); }
  if(dumpType & FPU) { DumpTopALU(topN); }
  if(dumpType & LS) { DumpTopALU(topN); }
}

} // namespace PPCInterpreter