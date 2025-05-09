#include <iostream>
#include <stdint.h>
#include <bit>
#define HEX(x) "0x" << std::hex << std::uppercase << (x) << std::dec

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    std::cout << "Invalid args" << std::endl;
    return -1;
  }
  const char *value = argv[1];
  const char *testValue = argv[2];
  u64 v = strtoull(value, 0, 16);
  u64 testV = strtoull(testValue, 0, 16);
  u64 vBS = std::byteswap<u32>(v);
  std::cout << "Value: " << HEX(v) << std::endl;
  std::cout << "Test Value: " << HEX(testV) << std::endl;
  std::cout << "Byteswapped value: " << HEX(vBS) << std::endl;
  std::cout << "Test Result: " << HEX(vBS & testV) << std::endl;
  return 0;
}