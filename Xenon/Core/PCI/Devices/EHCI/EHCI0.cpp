/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "EHCI0.h"

Xe::PCIDev::EHCI0::EHCI0(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram,
                         std::shared_ptr<USB::PortGroup> ports)
    : EHCI(deviceName, size, 0, parentPCIBridge, ram, std::move(ports)) {}
