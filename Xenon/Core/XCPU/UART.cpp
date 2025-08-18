// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "UART.h"

#include "Base/Error.h"
#include "Base/Global.h"
#include "Base/Thread.h"
#ifndef _WIN32
#include <signal.h>
#endif //ifndef _WIN32

#define COM_TEST 0

// UART Thread
void HW_UART_SOCK::uartMainThread() {
  Base::SetCurrentThreadName("[Xe::SMC::UART] Transfer");
  if (uartInitialized) {
    LOG_INFO(SMC, "UART Initialized Successfully!");
  }
  while (uartThreadRunning) {
    std::unique_lock<std::mutex> lock(uartMutex);
    if (!uartTxBuffer.empty()) {
      char c = uartTxBuffer.front();
      if (socketCreated) {
        s32 flags = 0;
#ifndef _WIN32
        flags = MSG_NOSIGNAL;
#endif // ifndef _WIN32
        if (send(sockHandle, &c, 1, flags) <= 0) {
          LOG_WARNING(UART, "Socket send failed: {}", strerror(errno));
          socketCreated = false;
          socketclose(sockHandle);
        }
      } else {
        if (c != -1 && c != '\0')
          printf("%c", c);
      }
      uartTxBuffer.pop();
    }
    lock.unlock();
  }
}

// UART Receive Thread
void HW_UART_SOCK::uartReceiveThread() {
  Base::SetCurrentThreadName("[Xe::SMC::UART] Receive");
  while (uartThreadRunning) {
    std::unique_lock<std::mutex> lock(uartMutex);
    char c = -1;
    u64 bytesReceived = 0;
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sockHandle, FIONBIO, &mode);
    bytesReceived = recv(sockHandle, &c, 1, 0);
#else
    bytesReceived = recv(sockHandle, &c, 1, MSG_DONTWAIT);
    if (bytesReceived == 0) {
      // Peer closed connection
      LOG_INFO(UART, "UART socket closed by peer.");
      socketCreated = false;
      socketclose(sockHandle);
      break;
    } else if (bytesReceived < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
      LOG_WARNING(UART, "UART recv error: {}", strerror(errno));
      socketCreated = false;
      socketclose(sockHandle);
      break;
    }
#endif //ifndef _WIN32
    if (c != -1 && bytesReceived != 0) {
      uartRxBuffer.push(c);
    }
    lock.unlock();
  }
}

void HW_UART_SOCK::Init(void *uartConfig) {
#ifndef _WIN32
  // Ignore SIGPIPE, we handle it per-call anyways
  signal(SIGPIPE, SIG_IGN);
#endif // ifndef _WIN32

  HW_UART_SOCK_CONFIG *sock = reinterpret_cast<decltype(sock)>(uartConfig);
  printMode = sock->usePrint;
  socketCreated = true;

  if (!printMode) {
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(sock->port);
    sockAddr.sin_addr.s_addr = inet_addr(sock->ip);
    socketCreated = true;
#ifdef _WIN32
    int start = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (start != 0) {
      LOG_CRITICAL(UART, "UART type 'socket' failed!\nWSAStartup returned a non-zero value! See error below.\n{}", start);
      socketCreated = false;
      Base::SystemPause();
    }
#endif // ifdef _WIN32
    sockHandle = socket(AF_INET, SOCK_STREAM, 0);
    int socketConnect = connect(sockHandle, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
    if (socketConnect != 0) {
      LOG_CRITICAL(UART, "Failed to connect to socket! See error below.\n{}", Base::GetLastErrorMsg());
      socketCreated = false;
      Base::SystemPause();
    }
    if (!socketCreated) {
      socketConnect = connect(sockHandle, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
      if (socketConnect != 0) {
        LOG_CRITICAL(UART, "(x2) Failed to connect to socket! See error below. \n{}", Base::GetLastErrorMsg());
        socketclose(sockHandle);
        Base::SystemPause();
      } else {
        socketCreated = true;
      }
    }
  }
  if (printMode) {
    socketCreated = false;
    uartThreadRunning = true;
    uartInitialized = true;
    uartPresent = true;
    uartThread = std::thread(&HW_UART_SOCK::uartMainThread, this);
    uartThread.detach();
  } else {
    uartThreadRunning = socketCreated;
    uartInitialized = socketCreated;
    uartPresent = true;
    uartThread = std::thread(&HW_UART_SOCK::uartMainThread, this);
    uartThread.detach();
    if (socketCreated) {
      uartSecondaryThread = std::thread(&HW_UART_SOCK::uartReceiveThread, this);
    }
  }
}

void HW_UART_SOCK::Shutdown() {
  uartThreadRunning = false;
  // Shutdown receive thread
  if (uartSecondaryThread.joinable())
    uartSecondaryThread.join();
  // Shutdown socket
  if (socketCreated) {
#ifdef _WIN32
    shutdown(sockHandle, SD_BOTH);
#endif // _WIN32
    socketclose(sockHandle);
  }
}

void HW_UART_SOCK::Write(const u8 data) {
  std::lock_guard<std::mutex> lock(uartMutex);
  uartTxBuffer.push(data);
  retVal = true;
}

u8 HW_UART_SOCK::Read() {
  u8 data = 0;
  std::lock_guard<std::mutex> lock(uartMutex);
  if (!uartRxBuffer.empty()) {
    data = uartRxBuffer.front();
    uartRxBuffer.pop();
    retVal = true;
  } else {
    retVal = false;
  }
  return data;
}

u32 HW_UART_SOCK::ReadStatus() {
  u32 status = 0;
  status |= uartTxBuffer.size() <= 16 ? UART_STATUS_EMPTY : 0;
  status |= uartRxBuffer.empty() ? 0 : UART_STATUS_DATA_PRES;
  return status;
}

#ifdef _WIN32
void HW_UART_VCOM::Init(void *uartConfig) {
  HW_UART_VCOM_CONFIG *vcom = reinterpret_cast<decltype(vcom)>(uartConfig);
  // Initialize the DCB structure
  SecureZeroMemory(&comPortDCB, sizeof(DCB));
  comPortDCB.DCBlength = sizeof(DCB);

  switch (vcom->config) {
  case 0x1E6:
    LOG_INFO(SMC, " * BaudRate: 115200bps, DataSize: 8, Parity: N, StopBits: 1.");
    comPortDCB.BaudRate = CBR_115200;
    comPortDCB.ByteSize = 8;
    comPortDCB.Parity = NOPARITY;
    comPortDCB.StopBits = ONESTOPBIT;
    break;
  case 0x1BB2:
    LOG_INFO(SMC, " * BaudRate: 38400bps, DataSize: 8, Parity: N, StopBits: 1.");
    comPortDCB.BaudRate = CBR_38400;
    comPortDCB.ByteSize = 8;
    comPortDCB.Parity = NOPARITY;
    comPortDCB.StopBits = ONESTOPBIT;
    break;
  case 0x0163:
    LOG_INFO(SMC, " * BaudRate: 19200bps, DataSize: 8, Parity: N, StopBits: 1.");
    comPortDCB.BaudRate = CBR_19200;
    comPortDCB.ByteSize = 8;
    comPortDCB.Parity = NOPARITY;
    comPortDCB.StopBits = ONESTOPBIT;
    break;
  default:
    LOG_WARNING(SMC, "SMCCore: Unknown UART config being set: ConfigValue = 0x{:X}", uartConfig);
    break;
  }

  // Open COM# port using the CreateFile function
  comPortHandle = CreateFileA(vcom->selectedComPort, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

  if (comPortHandle == INVALID_HANDLE_VALUE) {
    LOG_ERROR(UART, "CreateFile failed! See error below.\n{}", Base::GetLastErrorMsg());
    LOG_ERROR(UART, "Make sure you have a valid COM loopback device, or a vCOM driver with an avaliable port");
    uartPresent = false;
    return;
  } else {
    uartInitialized = true;
  }

  // Set The COM Port State as per config value
  if (!SetCommState(comPortHandle, &comPortDCB)) {
    LOG_ERROR(UART, "SetCommState failed with error {}", Base::GetLastErrorMsg());
  }

#if defined(DEBUG_BUILD) && COM_TEST
  auto path = Base::FS::GetUserPath(Base::FS::PathType::RootDir);
  f.open(path / "UART.txt", std::ios::out | std::ios::binary);
#endif

  // Everything initialized
  uartInitialized = true;
}

void HW_UART_VCOM::Shutdown() {
#if defined(DEBUG_BUILD) && COM_TEST
  f.close();
#endif
}

void HW_UART_VCOM::Write(const u8 data) {
#if defined(DEBUG_BUILD) && COM_TEST
  if (reading && !uartBuffer.empty()) {
    u64 bufferSize = uartBuffer.size();
    u8 c[17] = {};
    snprintf((char*)c, sizeof(c), "W  0x%llX\xFF\0", bufferSize);
    f.write((char*)&c, strlen((char*)c) + 1);
    f.write((char*)uartBuffer.data(), uartBuffer.size());
    uartBuffer.clear();
  }
  reading = false;
  uartBuffer.push_back(data);
#endif
  if (comPortHandle)
    retVal = WriteFile(comPortHandle, &data, 1, &currentBytesWrittenCount, nullptr);
}

u8 HW_UART_VCOM::Read() {
#if defined(DEBUG_BUILD) && COM_TEST
  if (!reading && !uartBuffer.empty()) {
    u64 bufferSize = uartBuffer.size();
    u8 c[17] = {};
    snprintf((char*)c, sizeof(c), "R  0x%llX\xFF\0", bufferSize);
    f.write((char*)&c, strlen((char *)c) + 1);
    f.write((char*)uartBuffer.data(), uartBuffer.size());
    uartBuffer.clear();
  }
  reading = true;
#endif
  u8 data = 0;
  if (comPortHandle)
    retVal = ReadFile(comPortHandle, &data, 1, &currentBytesReadCount, nullptr);
#if defined(DEBUG_BUILD) && COM_TEST
  uartBuffer.push_back(data);
#endif
  return data;
}

u32 HW_UART_VCOM::ReadStatus() {
  u32 status = 0;
  if (uartInitialized) {
    // Get current COM Port Status
    ClearCommError(comPortHandle, &comPortError,
      &comPortStat);
    // The queue has any bytes remaining?
    if (comPortStat.cbInQue > 0)  {
      // Got something to read in the input queue
      status |= UART_STATUS_DATA_PRES;
    } else {
      // The input queue is empty
      status |= UART_STATUS_EMPTY;
    }
  } else {
    status |= UART_STATUS_EMPTY;
  }
  return status;
}
#else
void HW_UART_VCOM::Init(void *uartConfig) {
  UNIMPLEMENTED_MSG("Override for HW_UART_VCOM::Init failed!");
}

void HW_UART_VCOM::Shutdown() {
  UNIMPLEMENTED_MSG("Override for HW_UART_VCOM::Shutdown failed!");
}

void HW_UART_VCOM::Write(const u8 data) {
  UNIMPLEMENTED_MSG("Override for HW_UART_VCOM::Write failed!");
}

u8 HW_UART_VCOM::Read() {
  UNIMPLEMENTED_MSG("Override for HW_UART_VCOM::Read failed!");
  return 0;
}

u32 HW_UART_VCOM::ReadStatus() {
  UNIMPLEMENTED_MSG("Override for HW_UART_VCOM::ReadStatus failed!");
  return UART_STATUS_EMPTY;
}
#endif // _WIN32
