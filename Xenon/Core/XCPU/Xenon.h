// Copyright 2025 Xenon Emulator Project

#pragma once

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

#include "Core/RootBus/RootBus.h"
#include "Core/XCPU/PPU/PPU.h"

#include <filesystem>

class Xenon {
public:
  Xenon(RootBus *inBus, const std::string blPath, const std::string fusesPath);
  ~Xenon();

  void Start(u64 resetVector = 0x100);

  u32 RunCPITests(u64 resetVector = 0x100);

  void LoadElf(const std::string path);

  void Reset();

  void Halt(u64 haltOn = 0);

  void Continue();

  void Step(int amount = 1);

  bool IsHalted();

  Xe::XCPU::IIC::XenonIIC *GetIICPointer() { return &xenonContext.xenonIIC; }

  PPU *GetPPU(u8 ppuID);

private:
  // System Bus
  RootBus *mainBus = nullptr;

  // Global Xenon CPU Content (shared between PPUs)
  XENON_CONTEXT xenonContext = {};

  // Power Processing Units, the effective execution units inside the XBox
  // 360 CPU.
  std::unique_ptr<PPU> ppu0{};
  std::unique_ptr<PPU> ppu1{};
  std::unique_ptr<PPU> ppu2{};
};
