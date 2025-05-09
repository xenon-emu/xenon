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
  u32 currentOpcode = (packetData >> 8) & 0x7F;
  const u32 dataCount = ((packetData >> 16) & 0x3FFF) + 1;
  std::cout << "Packet: " << HEX(packetData) << std::endl;
  std::cout << "Opcode: " << HEX(currentOpcode) << std::endl;
  std::cout << "Data Count: " << HEX(dataCount) << std::endl;
  return 0;
}