// Copyright 2025 Xenon Emulator Project

#include "IIC.h"
#include "Base/Logging/Log.h"

#include <unordered_map>

#ifdef DEBUG
#define IIC_DEBUG
#endif // DEBUG

Xe::XCPU::IIC::XenonIIC::XenonIIC() {
  for (s8 idx = 0; idx < 6; idx++) {
    // Set pending interrupts to 0.
    iicState.ppeIntCtrlBlck[idx].REG_ACK = PRIO_NONE;
  }
}

void Xe::XCPU::IIC::XenonIIC::writeInterrupt(u64 intAddress, const u8 *data, u64 size) {
  std::lock_guard lck(mutex);

  if (size < 4) {
    LOG_CRITICAL(Xenon, "Invalid interruprt write! Expected a size of at least 4, got {} instead", size);
    return;
  }

  u64 intData = 0;
  memcpy(&intData, data, size);

  constexpr u32 mask = 0xF000;
  u8 ppeIntCtrlBlckID = static_cast<u8>((intAddress & mask) >> 12);
  u8 ppeIntCtrlBlckReg = intAddress & 0xFF;
  u8 intType = (intData >> 56) & 0xFF;
  u8 cpusToInterrupt = (intData >> 40) & 0xFF;
  size_t intIndex = 0;
  u32 bsIntData = static_cast<u32>(byteswap_be<u64>(intData));
  
  auto& interrupts = iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts;
  switch (ppeIntCtrlBlckReg) {
  case Xe::XCPU::IIC::CPU_WHOAMI:
#ifdef IIC_DEBUG
      LOG_DEBUG(Xenon_IIC, "Control block number {:#x} beign set to PPU {:#x}",
        ppeIntCtrlBlckID, static_cast<u8>(bsIntData));
#endif // IIC_DEBUG
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_WHOAMI = bsIntData;
    break;
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI = bsIntData;
    break;
  case Xe::XCPU::IIC::CPU_IPI_DISPATCH_0:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_IPI_DISPATCH_0 = bsIntData;
    genInterrupt(intType, cpusToInterrupt);
    break;
  case Xe::XCPU::IIC::INT_0x30:
    // Dont know what this does, lets cause an interrupt?
    genInterrupt(intType, cpusToInterrupt);
    break;
  case Xe::XCPU::IIC::EOI:
    // If there are interrupts stored in the queue, remove the ack'd.
    if (!interrupts.empty()) {
      u16 intIdx = 0;
      for (auto& interrupt : iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts) {
        if (interrupt.ack) {
          break;
        }
        intIdx++;
      }

#ifdef IIC_DEBUG
        LOG_DEBUG(Xenon_IIC, "EOI interrupt {} for thread {:#x} ", 
          getIntName(static_cast<u8>(iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts[intIdx].interrupt)), 
          ppeIntCtrlBlckID);
#endif // IIC_DEBUG

      interrupts.erase(interrupts.begin() + intIdx);

      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intSignaled = false;
    }
    break;
  case Xe::XCPU::IIC::EOI_SET_CPU_CURRENT_TSK_PRI:
    // If there are interrupts stored in the queue, remove the ack'd.
    if (!interrupts.empty()) {
      u16 intIdx = 0;
      for (auto& interrupt : iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts) {
        if (interrupt.ack) {
          break;
        }
        intIdx++;
      }

#ifdef IIC_DEBUG
        LOG_DEBUG(Xenon_IIC, "EOI + Set PRIO: interrupt {} for thread {:#x}, new PRIO: {:#x}",
          getIntName(static_cast<u8>(iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts[intIdx].interrupt)),
          ppeIntCtrlBlckID, static_cast<u8>(bsIntData));
#endif // IIC_DEBUG

      interrupts.erase(interrupts.begin() + intIdx);

      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intSignaled = false;
    }

    // Set new Interrupt priority.
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI = bsIntData;
    break;
  case Xe::XCPU::IIC::INT_MCACK:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_INT_MCACK = bsIntData;
    break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown CPU Interrupt Ctrl Blck Reg being written: {:#x}", ppeIntCtrlBlckReg);
    break;
  }
}

void Xe::XCPU::IIC::XenonIIC::readInterrupt(u64 intAddress, u8 *data, u64 size) {
  std::lock_guard lck(mutex);

  constexpr u32 mask = 0xF000;
  u8 ppeIntCtrlBlckID = static_cast<u8>((intAddress & mask) >> 12);
  u8 ppeIntCtrlBlckReg = intAddress & 0xFF;

  auto &interrupts = iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts;
  switch (ppeIntCtrlBlckReg) {
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI: {
    u64 intData = byteswap_be<u64>(iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI);
    memcpy(data, &intData, size <= sizeof(intData) ? size : sizeof(intData));
  } break;
  case Xe::XCPU::IIC::ACK: {
    // Check if the queue isn't empty.
    if (!interrupts.empty()) {
      // Return the top priority pending interrupt.

      // Current Interrupt position in the container.
      u16 currPos = 0;
      // Highest interrupt priority found.
      u8 highestPrio = 0;
      // Highest priority interrupt position.
      u16 highestPrioPos = 0;

      for (auto& interrupt : interrupts) {
        // Signal first the top priority interrupt. 
        // If two of the same interrupt type exist, then signal the first that we find.
        if (interrupt.interrupt > highestPrio) {
          highestPrio = interrupt.interrupt;
          highestPrioPos = currPos;
        }
        currPos++;
      }

      // Set the top priority interrypt to signaled
      interrupts[highestPrioPos].ack = true;

      u64 intData = byteswap_be<u64>(highestPrio);
      memcpy(data, &intData, size <= sizeof(intData) ? size : sizeof(intData));
    }
    else {
      // If the queue is empty, return PRIO_NONE.
      constexpr u64 intData = byteswap_be<u64>(PRIO_NONE);
      memcpy(data, &intData, size <= sizeof(intData) ? size : sizeof(intData));
    }
  } break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown interrupt being read {:#x}", ppeIntCtrlBlckReg);
    break;
  }
}

bool Xe::XCPU::IIC::XenonIIC::checkExtInterrupt(u8 ppuID) {
  // Set the global lock.
  std::lock_guard lck(mutex);

  // Check for some interrupt that was already signaled.
  if (iicState.ppeIntCtrlBlck[ppuID].intSignaled) {
    return false;
  }

  // Check if there's interrupt present.
  if (iicState.ppeIntCtrlBlck[ppuID].interrupts.empty()) {
    return false;
  }

  // Determine if any interrupt is present thats higher or equal than current priority.
  bool priorityOk = false;
  for (auto& interrupt : iicState.ppeIntCtrlBlck[ppuID].interrupts) {
    if (interrupt.interrupt >= iicState.ppeIntCtrlBlck[ppuID].REG_CPU_CURRENT_TSK_PRI) {
      priorityOk = true;
      break;
    }
  }

  if (priorityOk) {
#ifdef IIC_DEBUG
      LOG_DEBUG(Xenon_IIC, "Signaling interrupt for thread {:#x} ", ppuID);
#endif // IIC_DEBUG
    iicState.ppeIntCtrlBlck[ppuID].intSignaled = true;
    return true;
  }
  else {
    return false;
  }

}

void Xe::XCPU::IIC::XenonIIC::genInterrupt(u8 interruptType,
  u8 cpusToInterrupt) {
  // Set a global lock.
  std::lock_guard lck(mutex);

  // Create our interrupt packet.
  Xe_Int newInt;
  newInt.ack = false;
  newInt.interrupt = interruptType;

  for (u8 ppuID = 0; ppuID < 6; ppuID++) {
    if ((cpusToInterrupt & 0x1) == 1) {

#ifdef IIC_DEBUG
        LOG_DEBUG(Xenon_IIC, "Generating interrupt: Thread {}, intType: {}", ppuID, getIntName(interruptType));
#endif // IIC_DEBUG

      // Store the interrupt in the interrupt queue.
      iicState.ppeIntCtrlBlck[ppuID].interrupts.push_back(newInt);
    }
    cpusToInterrupt = cpusToInterrupt >> 1;
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
  return "PRIO_UNK";
}
