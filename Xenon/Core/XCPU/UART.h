// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <Windows.h>
#define socketclose closesocket
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
#define socketclose close
#endif // _WIN32

#include <condition_variable>
#include <thread>
#include <queue>

#include "Base/Assert.h"

//
// UART Definitions
//
#define UART_STATUS_EMPTY 0x2
#define UART_STATUS_DATA_PRES 0x1

// HW UART base class
class HW_UART {
public:
  virtual ~HW_UART() = default;
  virtual void Init(void *uartConfig) = 0;
  virtual void Shutdown() = 0;
  virtual void Write(const u8 data) = 0;
  virtual u8 Read() = 0;
  virtual u32 ReadStatus() = 0;
  bool SetupNeeded() {
    return uartPresent && !uartInitialized;
  }
  bool Valid() {
    return uartPresent && uartInitialized;
  }
  // UART Initialized
  bool uartInitialized = false;
  // UART Present. Checks if it has a vCOM driver,
  // and if a socket is listening
  bool uartPresent = false;
  // Read/Write Return Status Values
  bool retVal = false;
};

// HW UART, used in Socket and printf
struct HW_UART_SOCK_CONFIG {
  //255.255.255.255 = 16 + 1 (nul-term)
  char ip[17] = {};
  u16 port = 0;
  bool usePrint = false;
};
class HW_UART_SOCK : public HW_UART {
public:
  void Init(void *uartConfig) override;
  void Shutdown() override;
  void Write(const u8 data) override;
  u8 Read() override;
  u32 ReadStatus() override;
  // UART Thread
  void uartMainThread();
  // UART Receive Thread
  void uartReceiveThread();
  // Print mode
  bool printMode = false;
  // Socket created
  bool socketCreated = false;
  // UART Thread object
  std::thread uartThread;
  // UART Receive Thread object
  std::thread uartSecondaryThread;
  // Thread status
  volatile bool uartThreadRunning = false;
  // Receive buffer (inverse, as we're hardware)
  std::queue<u8> uartTxBuffer = {};
  // Transfer buffer (inverse, as we're hardware)
  std::queue<u8> uartRxBuffer = {};
  // Mutex, to avoid race conitions
  std::mutex uartMutex = {};
  // Socket Address
  struct sockaddr_in sockAddr = {};
  // Socket Handles
#ifdef _WIN32
  // WSA, needed for Windows Sockets
  WSADATA wsaData = {};
  // FD Handle
  SOCKET sockHandle = {};
#else
  // FD Handle
  int sockHandle = 0;
#endif // _WIN32
};

// HW UART, vCOM. Used only on Windows for kern debugging
struct HW_UART_VCOM_CONFIG {
  //COM00 = 8 + 3 + 1 (\\.\\COM, num, nul-term)
  char selectedComPort[12] = {};
  u32 config = 0;
};
class HW_UART_VCOM : public HW_UART {
public:
  void Init(void *uartConfig) override;
  void Shutdown() override;
  void Write(const u8 data) override;
  u8 Read() override;
  u32 ReadStatus() override;
#ifdef DEBUG_BUILD
  std::ofstream f{};
  bool reading = false;
  // Buffer, used to detect when direction changes
  std::vector<u8> uartBuffer = {};
#endif
#ifdef _WIN32
  // Current COM Port Device Control Block
  // See
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-dcb
  DCB comPortDCB = {};
  // Current COM Port Handle
  void *comPortHandle = nullptr;
  // Current COM Port Status
  COMSTAT comPortStat = {};
  // Current COM Port error register used by ClearCommErr()
  ul32 comPortError = 0;
  // Bytes Written to the COM Port
  ul32 currentBytesWrittenCount = 0;
  // Bytes Read from the COM Port
  ul32 currentBytesReadCount = 0;
#endif // _WIN32
};
