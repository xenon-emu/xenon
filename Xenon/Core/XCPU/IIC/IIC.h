// Copyright 2025 Xenon Emulator Project

#pragma once

#include <queue>
#include <mutex>

#include "Base/Types.h"

namespace Xe {
  namespace XCPU {
    namespace IIC {

#define XE_IIC_BASE 0x50000
#define XE_IIC_SIZE 0xFF // 1 IIC Ctrl Block?

      //
      // Interrupt Types
      //
#define PRIO_IPI_4 0x08
#define PRIO_IPI_3 0x10
#define PRIO_SMM 0x14
#define PRIO_SFCX 0x18
#define PRIO_SATA_HDD 0x20
#define PRIO_SATA_CDROM 0x24
#define PRIO_OHCI_0 0x2C
#define PRIO_EHCI_0 0x30
#define PRIO_OHCI_1 0x34
#define PRIO_EHCI_1 0x38
#define PRIO_XMA 0x40
#define PRIO_AUDIO 0x44
#define PRIO_ENET 0x4C
#define PRIO_XPS 0x54
#define PRIO_GRAPHICS 0x58
#define PRIO_PROFILER 0x60
#define PRIO_BIU 0x64
#define PRIO_IOC 0x68
#define PRIO_FSB 0x6C
#define PRIO_IPI_2 0x70
#define PRIO_CLOCK 0x74
#define PRIO_IPI_1 0x78
#define PRIO_NONE 0x7C

      enum XE_IIC_CPU_REG {
        CPU_WHOAMI = 0x0,
        CPU_CURRENT_TSK_PRI = 0x8,
        CPU_IPI_DISPATCH_0 = 0x10,
        INT_0x30 = 0x30,
        ACK = 0x50,
        ACK_SET_CPU_CURRENT_TSK_PRI = 0x58,
        EOI = 0x60,
        EOI_SET_CPU_CURRENT_TSK_PRI = 0x68,
        INT_MCACK = 0x70
      };

      // Xenon Interrupt.
      struct Xe_Int {
        // Flag for determining wheter an interrupt was ack'd to the target PPU thread.
        bool ack = false;
        // Interrupt to signal.
        u64 interrupt = 0;
      };

      // Each logical thread has its own Interrupt Control Block.
      struct PPE_INT_CTRL_BLCK {
        u32 REG_CPU_WHOAMI;
        u32 REG_CPU_CURRENT_TSK_PRI;
        u32 REG_CPU_IPI_DISPATCH_0;
        u32 REG_ACK;
        u32 REG_ACK_SET_CPU_CURRENT_TSK_PRI;
        u32 REG_EOI;
        u32 REG_EOI_SET_CPU_CURRENT_TSK_PRI;
        u32 REG_INT_MCACK;
        // Stored pending interrupts.
        std::vector<Xe_Int> interrupts;
        // Stores wheter an interrupt was already signaled to the target thread.
        bool intSignaled = false;
      };

      struct IIC_State {
        // There are 6 total PPE's in the Xenon.
        PPE_INT_CTRL_BLCK ppeIntCtrlBlck[6] = {};
      };

      class XenonIIC {
      public:
        XenonIIC();
        void writeInterrupt(u64 intAddress, u64 intData);
        void readInterrupt(u64 intAddress, u64* intData);
        bool checkExtInterrupt(u8 ppuID);
        void genInterrupt(u8 interruptType, u8 cpusToInterrupt);

      private:
        IIC_State iicState;
        std::recursive_mutex mutex;
        // Returns the name of the input interrupt ID.
        std::string getIntName(u8 intID);
      };
    } // namespace IIC
  } // namespace XCPU
} // namespace Xe
