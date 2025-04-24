// Copyright 2025 Xenon Emulator Project

#include "IIC.h"

#include "Core/Xe_Main.h"

#include "Base/Assert.h"
#include "Base/Logging/Log.h"

#ifdef DEBUG
#define IIC_DEBUG
#endif // DEBUG

Xe::XCPU::IIC::XenonIIC::XenonIIC() {
  for (s8 idx = 0; idx < 6; idx++) {
    // Set pending interrupts to 0.
    iicState.crtlBlck[idx].REG_ACK = PRIO_NONE;
  }
}

void Xe::XCPU::IIC::XenonIIC::writeInterrupt(u64 intAddress, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::IIC]", "WriteInterrupt", MP_AUTO);

  if (size < 4) {
    LOG_CRITICAL(Xenon, "Invalid interrupt write! Expected a size of at least 4, got {} instead", size);
    return;
  }
  mutex.lock();

  u64 intData = 0;
  memcpy(&intData, data, size);

  constexpr u32 mask = 0xF000;
  u8 ppuID = static_cast<u8>((intAddress & mask) >> 12);
  u8 reg = intAddress & 0xFF;
  u8 intType = (intData >> 56) & 0xFF;
  u8 cpusToInterrupt = (intData >> 40) & 0xFF;
  u32 dataBs = static_cast<u32>(byteswap_be<u64>(intData));
  
  auto &ctrlBlock = iicState.crtlBlck[ppuID];
  switch (reg) {
  case Xe::XCPU::IIC::CPU_WHOAMI:
#ifdef IIC_DEBUG
    LOG_DEBUG(Xenon_IIC, "Control block number {:#x} beign set to PPU {:#x}",
      ppuID, static_cast<u8>(dataBs));
#endif // IIC_DEBUG
    ctrlBlock.REG_CPU_WHOAMI = dataBs;
    break;
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI:
    ctrlBlock.REG_CPU_CURRENT_TSK_PRI = dataBs;
    break;
  case Xe::XCPU::IIC::CPU_IPI_DISPATCH_0:
    ctrlBlock.REG_CPU_IPI_DISPATCH_0 = dataBs;
    mutex.unlock();
    genInterrupt(intType, cpusToInterrupt);
    mutex.lock();
    break;
  case Xe::XCPU::IIC::INT_0x30:
    // Dont know what this does, lets cause an interrupt?
    mutex.unlock();
    genInterrupt(intType, cpusToInterrupt);
    mutex.lock();
    break;
  case Xe::XCPU::IIC::EOI:
    // If there are interrupts stored in the queue, remove the ack'd.
    for (auto it = ctrlBlock.interrupts.begin(); it != ctrlBlock.interrupts.end(); ++it) {
      if (it->ack) {
#ifdef IIC_DEBUG
        LOG_DEBUG(Xenon_IIC, "EOI interrupt {} for thread {:#x} ", getIntName(static_cast<u8>(it->first)), ppuID);
#endif
        ctrlBlock.interrupts.erase(it);
        ctrlBlock.intSignaled = false;
        break;
      }
    }
    break;
  case Xe::XCPU::IIC::EOI_SET_CPU_CURRENT_TSK_PRI:
    // If there are interrupts stored in the queue, remove the ack'd
    for (auto it = ctrlBlock.interrupts.begin(); it != ctrlBlock.interrupts.end(); ++it) {
      if (it->ack) {
#ifdef IIC_DEBUG
        LOG_DEBUG(Xenon_IIC, "EOI + Set PRIO: interrupt {} for thread {:#x}, new PRIO: {:#x}",
          getIntName(static_cast<u8>(it->first)), ppuID, static_cast<u8>(dataBs));
#endif
        ctrlBlock.interrupts.erase(it);
        ctrlBlock.intSignaled = false;
        break;
      }
    }

    // Set new Interrupt priority
    ctrlBlock.REG_CPU_CURRENT_TSK_PRI = dataBs;
    break;
  case Xe::XCPU::IIC::INT_MCACK:
    ctrlBlock.REG_INT_MCACK = dataBs;
    break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown CPU Interrupt Ctrl Blck Reg being written: {:#x}", reg);
    break;
  }
  mutex.unlock();
}

void Xe::XCPU::IIC::XenonIIC::readInterrupt(u64 intAddress, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::IIC]", "ReadInterrupt", MP_AUTO);

  constexpr u32 mask = 0xF000;
  u8 ppuID = static_cast<u8>((intAddress & mask) >> 12);
  u8 reg = intAddress & 0xFF;

  auto &ctrlBlock = iicState.crtlBlck[ppuID];
  switch (reg) {
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI: {
    u64 intData = byteswap_be<u64>(ctrlBlock.REG_CPU_CURRENT_TSK_PRI);
    memcpy(data, &intData, size <= sizeof(intData) ? size : sizeof(intData));
  } break;
  case Xe::XCPU::IIC::ACK: {
    // Check if the queue isn't empty
    if (!ctrlBlock.interrupts.empty()) {
      // Return the top priority pending interrupt

      // Current Interrupt position in the container
      u16 currPos = 0;
      // Highest interrupt priority found
      u8 highestPrio = 0;
      // Highest priority interrupt position
      u16 highestPrioPos = 0;

      for (auto& interrupt : ctrlBlock.interrupts) {
        // Signal first the top priority interrupt
        // If two of the same interrupt type exist, then signal the first that we find
        if (interrupt.interrupt > highestPrio) {
          highestPrio = interrupt.interrupt;
          highestPrioPos = currPos;
        }
        currPos++;
      }
      
      ctrlBlock.interrupts[highestPrioPos].ack = true;

      u64 intData = byteswap_be<u64>(highestPrio);
      memcpy(data, &intData, size <= sizeof(intData) ? size : sizeof(intData));
    } else {
      constexpr u64 intData = byteswap_be<u64>(PRIO_NONE);
      memcpy(data, &intData, size <= sizeof(intData) ? size : sizeof(intData));
    }
  } break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown interrupt being read {:#x}", reg);
    break;
  }
}

bool Xe::XCPU::IIC::XenonIIC::checkExtInterrupt(u8 ppuID) {
  MICROPROFILE_SCOPEI("[Xe::IIC]", "CheckExternalInterrupt", MP_AUTO);

  // Ensure there is a lock
  std::lock_guard lock(mutex);
  auto &ctrlBlock = iicState.crtlBlck[ppuID];

  // Check for some interrupt that was already signaled and if there's interrupts
  if (ctrlBlock.intSignaled || ctrlBlock.interrupts.empty()) {
    return false;
  }

  // Determine if any interrupt is present thats higher or equal than current priority
  bool priorityOk = false;
  for (auto& interrupt : ctrlBlock.interrupts) {
    if (interrupt.interrupt >= ctrlBlock.REG_CPU_CURRENT_TSK_PRI) {
      priorityOk = true;
      break;
    }
  }

#ifdef IIC_DEBUG
  if (priorityOk)
    LOG_DEBUG(Xenon_IIC, "Signaling interrupt for thread {:#x} ", ppuID);
#endif // IIC_DEBUG

  ctrlBlock.intSignaled = priorityOk;
  return priorityOk;
}

void Xe::XCPU::IIC::XenonIIC::genInterrupt(u8 interruptType, u8 cpusToInterrupt) {
  MICROPROFILE_SCOPEI("[Xe::IIC]", "GenInterrupt", MP_AUTO);

  Xe_Int newInt{ false, interruptType };

  for (u8 ppuID = 0; ppuID < 6; ppuID++) {
    if (cpusToInterrupt & 0x1) {
#ifdef IIC_DEBUG
      LOG_DEBUG(Xenon_IIC, "Generating interrupt: Thread {}, intType: {}", ppuID, getIntName(interruptType));
#endif // IIC_DEBUG
      auto& ctrlBlock = iicState.crtlBlck[ppuID];
      // Store the interrupt in the interrupt queue
      ctrlBlock.interrupts.push_back(newInt);
    }
    cpusToInterrupt = cpusToInterrupt >> 1;
  }
}

void Xe::XCPU::IIC::XenonIIC::cancelInterrupt(u8 interruptType, u8 cpusInterrupted) {
  for (u8 ppuID = 0; ppuID < 6; ppuID++) {
    if (cpusInterrupted & 0x1) {
      auto &ctrlBlock = iicState.crtlBlck[ppuID];
#ifdef IIC_DEBUG
      LOG_DEBUG(Xenon_IIC, "Cancelling interrupt: Thread {}, intType: {}", ppuID, getIntName(interruptType));
#endif // IIC_DEBUG
      // Delete the interrupt from the interrupt queue
      bool found = false;
      if (!ctrlBlock.interrupts.empty()) {
        u16 intIdx = 0;
        for (auto& interrupt : ctrlBlock.interrupts) {
          if (interrupt.interrupt == interruptType && !interrupt.ack) {
            found = true;
            break;
          }
          intIdx++;
        }
        if (found) {
          ctrlBlock.interrupts.erase(ctrlBlock.interrupts.begin() + intIdx);
        }
      }
      cpusInterrupted >>= 1;
    }
  }
}

struct IRQ_DATA {
  u8 irq = 0;
  std::string_view name = {};
};

static constexpr IRQ_DATA interruptMap[]{
  { 0x08, "PRIO_IPI_4" },
  { 0x10, "PRIO_IPI_3" },
  { 0x14, "PRIO_SMM" },
  { 0x18, "PRIO_SFCX" },
  { 0x20, "PRIO_SATA_HDD" },
  { 0x24, "PRIO_SATA_CDROM" },
  { 0x2C, "PRIO_OHCI_0" },
  { 0x30, "PRIO_EHCI_0" },
  { 0x34, "PRIO_OHCI_1" },
  { 0x38, "PRIO_EHCI_1" },
  { 0x40, "PRIO_XMA" },
  { 0x44, "PRIO_AUDIO" },
  { 0x4C, "PRIO_ENET" },
  { 0x54, "PRIO_XPS" },
  { 0x58, "PRIO_GRAPHICS" },
  { 0x60, "PRIO_PROFILER" },
  { 0x64, "PRIO_BIU" },
  { 0x68, "PRIO_IOC" },
  { 0x6C, "PRIO_FSB" },
  { 0x70, "PRIO_IPI_2" },
  { 0x74, "PRIO_CLOCK" },
  { 0x78, "PRIO_IPI_1" },
  { 0x7C, "PRIO_NONE" }
};

std::string Xe::XCPU::IIC::XenonIIC::getIntName(u8 intID) {
  for (auto &irq : interruptMap) {
    if (irq.irq == intID) {
      return irq.name.data();
    }
  }
  UNREACHABLE_MSG("Unknown PRIO {}", intID);
}
