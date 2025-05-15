#include <iostream>
#include <stdint.h>

#define HEX(x) "0x" << std::hex << std::uppercase << (x) << std::dec

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    std::cout << "Invalid args" << std::endl;
    return -1;
  }
  const char *value = argv[1];
  u32 packetData = strtoul(value, 0, 16);
  const u32 packetType = packetData >> 30;
  std::cout << "Packet: " << HEX(packetData) << std::endl;
  switch (packetType) {
  case 0: {
    std::cout << std::endl << "Type0: " << std::endl;
    // Base register to start from.
    const u32 baseIndex = (packetData & 0x7FFF);
    // Tells wheter the write is to one or multiple regs starting at specified register at base index.
    const u32 singleRegWrite = (packetData >> 15) & 0x1;
    const u32 regCount = ((packetData >> 16) & 0x3FFF) + 1;
    std::cout << "Register Count: " << HEX(regCount) << std::endl;
    std::cout << "Simple Register Write: " << HEX(singleRegWrite) << std::endl;
    std::cout << "Base Index: " << HEX(baseIndex) << std::endl;
    for (u64 idx = 0; idx < regCount; idx++) {
      u32 targetRegIndex = singleRegWrite ? baseIndex : baseIndex + idx;
      std::cout << " Register " << idx << ": " << HEX(targetRegIndex) << std::endl;
    }
  } break;
  case 1: {
    std::cout << std::endl << "Type1: " << std::endl;
    const u32 regIndex0 = packetData & 0x7FF;
    const u32 regIndex1 = (packetData >> 11) & 0x7FF;
    std::cout << "Register 0: " << HEX(regIndex0) << std::endl;
    std::cout << "Register 1: " << HEX(regIndex1) << std::endl;
  } break;
  case 2: {
    std::cout << std::endl << "Type2: " << std::endl;
    std::cout << "No-operation" << std::endl;
  } break;
  case 3: {
    std::cout << std::endl << "Type3: " << std::endl;
    u32 currentOpcode = (packetData >> 8) & 0x7F;
    const u32 dataCount = ((packetData >> 16) & 0x3FFF) + 1;
    std::cout << "Opcode: " << HEX(currentOpcode) << std::endl;
    std::cout << "Data Count: " << HEX(dataCount) << std::endl;
  } break;
  }
  return 0;
}