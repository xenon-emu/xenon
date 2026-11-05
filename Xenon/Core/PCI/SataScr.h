/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/PCI/PCIe.h"
#include "Core/PCI/SATA.h"

#include <cstring>

// The SATA status and control registers (SCR) of one SATA channel: SStatus, SError, SControl and the hotplug
// register. They're 16 bytes at offset 0x10 of the channel's command block (BAR0), with a copy at XE_SIS_SCR_BASE
// of its config space. The HDD and ODD channels behave the same here.
//
// Sources: the Xbox 360 Linux Kernel patches for the SiS-style SCR block.

namespace Xe::PCI {

  class SataScr {
  public:
    // What a write to SControl did to the link. The device reacts to it (abandons its command, resets itself).
    enum class Link {
      None,
      ComResetAsserted, // DET 1: the host holds COMRESET
      Offline,          // DET 4: the PHY is taken offline
      ComResetReleased  // back to DET 0: the link comes up again and the device sends its signature
    };

    u32 sStatus = 0;
    u32 sError = 0;
    u32 sControl = 0;
    // Offset 0x1C isn't SActive on this southbridge, it's a device presence change (hotplug) register. When the kernel
    // detects a device it reads it, sets bit 5 (0x20) if SStatus DET is nonzero or bit 4 (0x10) if it is zero, and
    // writes it back, which arms detection of the opposite transition. Bits 2 and 3 (0x04, 0x08) report a presence
    // change. The kernel's interrupt handler clears bits 4 and 5 when it finds either set, logs a hotplug and then
    // returns to the firmware or resets the console, so they must never show up on the HDD channel. The ODD channel is
    // exempt, which is why a disc swap doesn't reset the machine. Nothing here latches them.
    u32 hotplug = 0;

    // Power-on (and platform reset) state. present: a device is attached to the channel. error: SError right
    // after power-on.
    void Reset(bool present, u32 error) {
      this->present = present;
      sControl = PowerOnControl;
      sStatus = present ? StatusLinkUp : 0;
      sError = present ? error : 0;
      hotplug = present ? PowerOnHotplug : 0;
    }

    // Bytes offset... of the block, offset 0..15 from SStatus.
    void Read(u32 offset, u8 *data, u64 size) const {
      const u32 scr[4] = {sStatus, sError, sControl, hotplug};
      for (u64 i = 0; i < size && offset + i < 16; ++i) {
        const u32 o = static_cast<u32>(offset + i);
        data[i] = static_cast<u8>(scr[o / 4] >> (8 * (o % 4)));
      }
    }

    Link Write(u32 offset, const u8 *data, u64 size) {
      // Gather the written bytes per register.
      u32 value[4] = {};
      u32 mask[4] = {};
      for (u64 i = 0; i < size && offset + i < 16; ++i) {
        const u32 o = static_cast<u32>(offset + i);
        value[o / 4] |= static_cast<u32>(data[i]) << (8 * (o % 4));
        mask[o / 4] |= 0xFFu << (8 * (o % 4));
      }
      // SStatus is read-only.
      // SError: write 1 to clear (the kernel writes 0xFFFFFFFF).
      if (mask[1]) { sError &= ~value[1]; }
      // SControl: DET drives COMRESET.
      Link link = Link::None;
      if (mask[2]) { link = WriteControl((sControl & ~mask[2]) | value[2]); }
      // Hotplug: the arming bits are plain storage, the change bits are write-1-to-clear.
      if (mask[3]) {
        const u32 merged = (hotplug & ~mask[3]) | value[3];
        hotplug = (merged & ~HotplugChangeBits) | (hotplug & HotplugChangeBits & ~value[3]);
      }
      return link;
    }

    // The copy in configuration space (at XE_SIS_SCR_BASE). Call it after every change.
    void Mirror(PCI_TYPE0_CONFIG_SPACE &config) const {
      memcpy(&config.data[XE_SIS_SCR_BASE + 0x0], &sStatus, 4);
      memcpy(&config.data[XE_SIS_SCR_BASE + 0x4], &sError, 4);
      memcpy(&config.data[XE_SIS_SCR_BASE + 0x8], &sControl, 4);
      memcpy(&config.data[XE_SIS_SCR_BASE + 0xC], &hotplug, 4);
    }

  private:
    // SStatus: DET 3 (device present, PHY up), SPD Gen1, IPM active.
    static constexpr u32 StatusLinkUp = 0x113;
    // SStatus while COMRESET is held: device present, PHY not up.
    static constexpr u32 StatusLinkDown = 0x001;
    // SStatus with the PHY offline (SControl DET 4).
    static constexpr u32 StatusOffline = 0x004;
    // SError DIAG bits latched when the link comes back: PhyRdy change (N) and COMWAKE (W).
    static constexpr u32 ErrorLinkUp = (1u << 16) | (1u << 18);
    // SControl after power-on: Partial and Slumber disabled.
    static constexpr u32 PowerOnControl = 0x300;
    // Hotplug register after power-on.
    static constexpr u32 PowerOnHotplug = 0x40;
    // Hotplug presence change bits.
    static constexpr u32 HotplugChangeBits = 0x0C;

    Link WriteControl(u32 value) {
      const u32 oldDet = sControl & SCONTROL_DET_BITS;
      const u32 det = value & SCONTROL_DET_BITS;
      sControl = value;
      if (!present) { return Link::None; }

      if (det == SCONTROL_DET_INIT) {
        if (oldDet != SCONTROL_DET_INIT) {
          sStatus = StatusLinkDown;
          return Link::ComResetAsserted;
        }
      } else if (det == SCONTROL_DET_DISABLE) {
        if (oldDet != SCONTROL_DET_DISABLE) {
          sStatus = StatusOffline;
          return Link::Offline;
        }
      } else if (det == SCONTROL_DET_NO_DEV && (oldDet == SCONTROL_DET_INIT || oldDet == SCONTROL_DET_DISABLE)) {
        sStatus = StatusLinkUp;
        sError |= ErrorLinkUp;
        return Link::ComResetReleased;
      }
      return Link::None;
    }

    bool present = false;
  };

} // namespace Xe::PCI
