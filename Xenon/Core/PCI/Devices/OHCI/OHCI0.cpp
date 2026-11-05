/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI0.h"

Xe::PCIDev::OHCI0::OHCI0(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram,
                         std::shared_ptr<USB::PortGroup> ports)
    : OHCI(deviceName, size, 0, parentPCIBridge, ram, std::move(ports)) {}
