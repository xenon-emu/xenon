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
      static_cast<u32>(_byteswap_uint64(intData));
    break;
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI =
      static_cast<u32>(_byteswap_uint64(intData));
    break;
  case Xe::XCPU::IIC::CPU_IPI_DISPATCH_0:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_IPI_DISPATCH_0 =
      static_cast<u32>(_byteswap_uint64(intData));
    genInterrupt(intType, cpusToInterrupt);
    break;
  case Xe::XCPU::IIC::INT_0x30:
    // Dont know what this does, lets cause an interrupt?
    genInterrupt(intType, cpusToInterrupt);
    break;
  case Xe::XCPU::IIC::EOI:
    // If there are interrupts stored in the queue, remove the ack'd.
    if (!iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts.empty()) {
      u16 intIdx = 0;
      for (auto& interrupt : iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts) {
        if (interrupt.ack) {
          break;
        }
        intIdx++;
      }
      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts.erase(
        iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts.begin() + intIdx);

      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intSignaled = false;
    }
    break;
  case Xe::XCPU::IIC::EOI_SET_CPU_CURRENT_TSK_PRI:
    // If there are interrupts stored in the queue, remove the ack'd.
    if (!iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts.empty()) {
      u16 intIdx = 0;
      for (auto& interrupt : iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts) {
        if (interrupt.ack) {
          break;
        }
        intIdx++;
      }
      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts.erase(
        iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts.begin() + intIdx);

      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].intSignaled = false;
    }

    // Set new Interrupt priority.
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI =
      static_cast<u32>(_byteswap_uint64(intData));
    break;
  case Xe::XCPU::IIC::INT_MCACK:
    iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_INT_MCACK =
      static_cast<u32>(_byteswap_uint64(intData));
    break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown CPU Interrupt Ctrl Blck Reg being written: {:#x}", ppeIntCtrlBlckReg);
    break;
  }
}

void Xe::XCPU::IIC::XenonIIC::readInterrupt(u64 intAddress, u64* intData) {
  std::lock_guard lck(mutex);

  u32 mask = 0xF000;
  u8 ppeIntCtrlBlckID = static_cast<u8>((intAddress & mask) >> 12);
  u8 ppeIntCtrlBlckReg = intAddress & 0xFF;
  switch (ppeIntCtrlBlckReg) {
  case Xe::XCPU::IIC::CPU_CURRENT_TSK_PRI:
    *intData = _byteswap_uint64(
      (u64)iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].REG_CPU_CURRENT_TSK_PRI);
    break;
  case Xe::XCPU::IIC::ACK:
    // Check if the queue isn't empty.
    if (iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts.empty() != true) {
      // Return the top priority pending interrupt.

      // Current Interrupt position in the container.
      u16 currPos = 0;
      // Highest interrupt priority found.
      u8 highestPrio = 0;
      // Highest priority interrupt position.
      u16 highestPrioPos = 0;

      for (auto& interrupt : iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts) {
        // Signal first the top priority interrupt. 
        // If two of the same interrupt type exist, then signal the first that we find.
        if (interrupt.interrupt > highestPrio) {
          highestPrio = interrupt.interrupt;
          highestPrioPos = currPos;
        }
        currPos++;
      }

      // Set the top priority interrypt to signaled
      iicState.ppeIntCtrlBlck[ppeIntCtrlBlckID].interrupts[highestPrioPos].ack = true;

      *intData = _byteswap_uint64(highestPrio);
    }
    else {
      // If the queue is empty, return PRIO_NONE.
      *intData = _byteswap_uint64(PRIO_NONE);
    }
    break;
  default:
    LOG_ERROR(Xenon_IIC, "Unknown interupt being read {:#x}", ppeIntCtrlBlckReg);
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

      LOG_DEBUG(Xenon_IIC, "Generating interrupt: Thread {}, intType: {:#x}", ppuID, interruptType);

      // Store the interrupt in the interrupt queue.
      iicState.ppeIntCtrlBlck[ppuID].interrupts.push_back(newInt);
    }
    cpusToInterrupt = cpusToInterrupt >> 1;
  }
}
