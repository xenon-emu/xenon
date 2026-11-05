/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/PCI/Bridge/PCIBridge.h"

// A device's level-sensitive interrupt line, as the interrupt controller sees it.
//
// The IIC latches a pending bit per vector, so only edges are forwarded. Raising the interrupt again while the line
// is still up (ISR in service, before it masks or acknowledges the cause) would queue a second delivery whose ISR
// finds nothing to do. The device calls Set with the line's level whenever it may have changed, under the lock
// that guards its interrupt registers.

namespace Xe::PCI {

  class InterruptLine {
  public:
    InterruptLine() = default;
    InterruptLine(PCIBridge *bridge, u8 prio)
        : bridge(bridge)
        , prio(prio) {}

    // Follows the line to level. Returns true when it rose and the interrupt was sent.
    // acknowledged means the driver just acknowledged some cause. If the line is still up after that, another cause
    // arrived since the driver read the status (drivers acknowledge only the bits they read) and no edge follows for
    // it, so the interrupt is sent again.
    bool Set(bool level, bool acknowledged = false) {
      if (level == asserted) {
        if (level && acknowledged && bridge) { bridge->RouteInterrupt(prio); }
        return false;
      }
      asserted = level;
      if (!bridge) { return false; }
      if (level) {
        bridge->RouteInterrupt(prio);
      } else {
        bridge->CancelInterrupt(prio);
      }
      return level;
    }

  private:
    PCIBridge *bridge = nullptr;
    u8 prio = 0;
    bool asserted = false;
  };

} // namespace Xe::PCI
