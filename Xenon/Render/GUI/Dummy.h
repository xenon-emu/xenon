// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/GUI/GUI.h"

#include "Base/Logging/Log.h"

namespace Render {

class DummyGUI : public GUI {
public:
  void InitBackend(void *context) override;
  void ShutdownBackend() override;
  void BeginSwap() override;
  void EndSwap() override;
};

} // namespace Render