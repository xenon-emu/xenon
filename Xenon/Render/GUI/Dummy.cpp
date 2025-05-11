// Copyright 2025 Xenon Emulator Project

#include "Dummy.h"

void Render::DummyGUI::InitBackend(void *context) {
  LOG_INFO(Render, "DummyGUI::InitBackend");
}

void Render::DummyGUI::ShutdownBackend() {
  LOG_INFO(Render, "DummyGUI::ShutdownBackend");
}

void Render::DummyGUI::BeginSwap() {
  LOG_INFO(Render, "DummyGUI::BeginSwap");
}

void Render::DummyGUI::EndSwap() {
  LOG_INFO(Render, "DummyGUI::EndSwap");
}