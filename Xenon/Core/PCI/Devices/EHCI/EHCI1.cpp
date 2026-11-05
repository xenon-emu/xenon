/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "EHCI1.h"

Xe::PCIDev::EHCI1::EHCI1(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram,
                         std::shared_ptr<USB::PortGroup> ports)
    : EHCI(deviceName, size, 1, parentPCIBridge, ram, std::move(ports)) {}
