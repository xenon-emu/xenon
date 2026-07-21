/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/HLE/HLEFunctionProvider.h"

namespace Xe::Core::HLE {

// https://github.com/CodeAsm/ffplay360/blob/master/Common/XTLOnPC.h
struct XVIDEO_MODE {
  u32 displayWidth;
  u32 displayHeight;
  u32 isInterlaced;
  u32 isWidescreen;
  u32 isHiDef;
  f32 refreshRate;
  u32 videoStandard;
  u32 pixelRate;
  u32 widescreenFlag;
  u32 reserved[3];
};

// HLE provider for Video related functions.
class HLEVideoProvider : public HLEFunctionProvider {
public:
  HLEVideoProvider() = default;
  ~HLEVideoProvider() override = default;

  bool Setup() override;
  eHLECategory GetCategory() const override { return eHLECategory::Video; }
  std::string GetName() const override { return "Video"; }
  void CollectFunctions(std::vector<sHLEFunction> &table) override;

private:
  // Current Video Mode
  XVIDEO_MODE currentVideoMode{};

  // Static HLE trampolines
  static void HLE_VdRetrainEDRAM(sPPEState *ppeState);
  static void HLE_VdIsHSIOTrainingSucceeded(sPPEState *ppeState);
  static void HLE_VdSwap(sPPEState *ppeState);
  static void HLE_VdQueryVideoMode(sPPEState *ppeState);
};

} // namespace Xe::Core::HLE
