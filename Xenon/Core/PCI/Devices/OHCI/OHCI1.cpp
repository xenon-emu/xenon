/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI1.h"

Xe::PCIDev::OHCI1::OHCI1(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram,
                         std::shared_ptr<USB::PortGroup> ports)
    : OHCI(deviceName, size, 1, parentPCIBridge, ram, std::move(ports)) {}
