// Copyright 2025 Xenon Emulator Project

#include "IIC.h"

#include <thread>

#include "Base/Logging/Log.h"

Xe::XCPU::IIC::XenonIIC::XenonIIC() {
  for (s8 idx = 0; idx < 6; idx++) {
    // Set pending interrupts to 0.
    iicState.ppeIntCtrlBlck[idx].REG_ACK = PRIO_NONE;
  }
}

void Xe::XCPU::IIC::XenonIIC::writeInterrupt(u64 intAddress, u64 intData) {
  std::lock_guard lck(mutex);

  u32 mask = 0xF000;
  u8 ppeIntCtrlBlckID = static_cast<u8>((intAddress & mask) >> 12);
  u8 ppeIntCtrlBlckReg = intAddress & 0xFF;
  u8 intType = (intData >> 56) & 0xFF;
  u8 cpusToInterrupt = (intData >> 40) & 0xFF;
  size_t intIndex = 0;

  switch (ppeIntCtrlBlckReg) {
  case Xe::XCPU::IIC::CPU_WHOAMI:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_WHOAMI =
        static_cast<u32>(std::byteswap<u64>(intData));
    break;
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI =
        static_cast<u32>(std::byteswap<u64>(intData));
    break;
  case Xe::XCPU::IIC::CPU_IPI_DISPATCH_0:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_IPI_DISPATCH_0 =
        static_cast<u32>(std::byteswap<u64>(intData));
    LOG_DEBUG(Xenon, "Sending CPU(s) {} interrupt {} (ppeIntCtrlBlckReg is CPU_IPI_DISPATCH_0, likely trying to bring {} online)", cpusToInterrupt, intType, cpusToInterrupt);
    //genInterrupt(intType, cpusToInterrupt);
    break;
  case Xe::XCPU::IIC::INT_0x30:
    // Dont know what this does, lets cause an interrupt?
    genInterrupt(intType, cpusToInterrupt);
    break;
  case Xe::XCPU::IIC::EOI:
    // Check if we have interrupts in the queue.
    if (!iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intQueue.empty()) {
      // Remove the first interrupt in the Interrupt queue.
      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intQueue.pop();
      // Clear the ACK flag.
      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intAck = false;
    }
    break;
  case Xe::XCPU::IIC::EOI_SET_CPU_CURRENT_TSK_PRI:
    // Check if we have interrupts in the queue.
    if (!iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intQueue.empty()) {
      // Remove the first interrupt in the Interrupt Queue.
      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intQueue.pop();
      // Clear the ACK flag.
      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intAck = false;
    }
    // Set new Interrupt priority.
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI =
        static_cast<u32>(std::byteswap<u64>(intData));
    break;
  case Xe::XCPU::IIC::INT_MCACK:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_INT_MCACK =
        static_cast<u32>(std::byteswap<u64>(intData));
    break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown CPU Interrupt Ctrl Blck Reg being written: {:#x}", ppeIntCtrlBlckReg);
    break;
  }
}

void Xe::XCPU::IIC::XenonIIC::readInterrupt(u64 intAddress, u64 *intData) {
  std::lock_guard lck(mutex);

  u32 mask = 0xF000;
  u8 ppeIntCtrlBlckID = static_cast<u8>((intAddress & mask) >> 12);
  u8 ppeIntCtrlBlckReg = intAddress & 0xFF;
  switch (ppeIntCtrlBlckReg) {
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI:
    *intData = std::byteswap<u64>(
        (u64)iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI);
    break;
  case Xe::XCPU::IIC::ACK:
    // Check if the queue isn't empty (this probably isn't necessary).
    if (iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intQueue.empty() != true) {
      // If the first interrupt is ACK'd we return PRIO_NONE.
      if (iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intAck) {
        *intData = std::byteswap<u64>(PRIO_NONE);
      } else {
        // Signal the Top Priority interrupt.
        *intData = std::byteswap<u64>(
            iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intQueue.top());
        // Set the ACK flag.
        iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intAck = true;
        return;
      }
    }
    *intData = std::byteswap<u64>(PRIO_NONE);
    break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown interupt being read {:#x}", ppeIntCtrlBlckReg);
    break;
  }
}

bool Xe::XCPU::IIC::XenonIIC::checkExtInterrupt(u8 ppuID) {
  std::lock_guard lck(mutex);
  // 1. Check if there's any interrupt already taken.
  if (iicState.ppeIntCtrlBlck[ppuID].intAck) {
    return false;
  }

  // Check to see if there are any interrupts in our queue.
  if (iicState.ppeIntCtrlBlck[ppuID].intQueue.empty() != true) {
    // Check if the top priority interrupt is higher or equal than current task
    // priority.
    if (iicState.ppeIntCtrlBlck[ppuID].intQueue.top() >=
        iicState.ppeIntCtrlBlck[ppuID].REG_CPU_CURRENT_TSK_PRI) {
      // Signal the interrupt.
      return true;
    }
  }
  return false;
}

void Xe::XCPU::IIC::XenonIIC::genInterrupt(u8 interruptType,
                                           u8 cpusToInterrupt) {
  std::lock_guard lck(mutex);
  XE_INT newInt;
  newInt.ack = false;
  newInt.pendingInt = interruptType;
  for (u8 ppuID = 0; ppuID < 6; ppuID++) {
    if ((cpusToInterrupt & 0x1) == 1) {
      // Store the interrupt.
      iicState.ppeIntCtrlBlck[ppuID].intQueue.push(
          static_cast<u64>(interruptType));
    }
    cpusToInterrupt = cpusToInterrupt >> 1;
  }
}
