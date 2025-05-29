// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Serial.h"

namespace Serial {

Handle OpenPort(const char *portName) {
#ifdef _WIN32
  HANDLE serial = CreateFileA(
      portName, GENERIC_READ | GENERIC_WRITE, 0,
      nullptr, OPEN_EXISTING, 0, nullptr);
  if (serial == INVALID_HANDLE_VALUE)
    return (Handle)-1;

  DCB dcb{};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(serial, &dcb))
    return (Handle)-1;
  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8;
  dcb.StopBits = ONESTOPBIT;
  dcb.Parity   = NOPARITY;
  SetCommState(serial, &dcb);

  COMMTIMEOUTS timeouts{};
  timeouts.ReadIntervalTimeout = 50;
  timeouts.ReadTotalTimeoutConstant = 50;
  timeouts.ReadTotalTimeoutMultiplier = 10;
  SetCommTimeouts(serial, &timeouts);

  return serial;
#else
  s32 fd = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0)
    return -1;
  struct termios tty{};
  memset(&tty, 0, sizeof tty);
  if (tcgetattr(fd, &tty) != 0)
    return -1;
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
  tty.c_iflag &= ~IGNBRK; // Disable break processing
  tty.c_lflag = 0; // No signaling chars, no echo
  tty.c_oflag = 0; // No remapping, no delays
  tty.c_cc[VMIN]  = 1; // Read blocks until at least 1 byte is available
  tty.c_cc[VTIME] = 1; // 0.1 second timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Shut off xon/xoff ctrl
  tty.c_cflag |= (CLOCAL | CREAD); // Ignore modem controls
  tty.c_cflag &= ~(PARENB | PARODD); // Shut off parity
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;
  tcsetattr(fd, TCSANOW, &tty);

  return fd;
#endif
}

void ClosePort(Handle handle) {
#ifdef _WIN32
    CloseHandle(handle);
#else
    close(handle);
#endif
}

bool Read(Handle handle, u8 *buffer, u64 bufSize, u32 &outBytesRead) {
#ifdef _WIN32
  DWORD read = 0;
  BOOL result = ReadFile(handle, buffer, static_cast<DWORD>(bufSize), &read, nullptr);
  outBytesRead = read;
  return result && read == bufSize;
#else
  ssize_t result = read(handle, buffer, bufSize);
  if (result < 0) {
    outBytesRead = 0;
    return false;
  }
  outBytesRead = result;
  return static_cast<u64>(result) == bufSize;
#endif
}

bool Write(Handle handle, const u8 *buffer, u64 bufSize, u32 &outBytesWritten) {
#ifdef _WIN32
  DWORD written = 0;
  BOOL result = WriteFile(handle, buffer, static_cast<DWORD>(bufSize), &written, nullptr);
  outBytesWritten = written;
  if (result && written == bufSize) {
    FlushFileBuffers(handle); // Force flush to serial port
    return true;
  }
  return false;
#else
  ssize_t result = write(handle, buffer, bufSize);
  if (result < 0) {
    outBytesWritten = 0;
    return false;
  }
  outBytesWritten = result;
  return static_cast<u64>(result) == bufSize;
#endif
}

} // namespace Serial