/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#ifdef _WIN32
  #include <Windows.h>
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

namespace Base {

  //
  // Read/Write Storage.
  //

#ifdef _WIN32
  class ReadWriteStorage {
  public:
    ReadWriteStorage(const std::string Filename) {
      hFile = CreateFileA(Filename.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    ~ReadWriteStorage(void) {
      if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
      hFile = INVALID_HANDLE_VALUE;
    }

    u64 Size() {
      LARGE_INTEGER fileSize;
      if (!GetFileSizeEx(hFile, &fileSize)) return 0;
      return static_cast<u64>(fileSize.QuadPart);
    }

    bool Read(u64 Offset, u8* Destination, u32 cu8s) {
      DWORD cbRead;
      OVERLAPPED Over;

      memset(&Over, 0, sizeof Over);
      Over.Offset = static_cast<u32>(Offset);
      Over.OffsetHigh = (static_cast<u64>(Offset) >> 32);
      return (ReadFile(hFile, Destination, cu8s, &cbRead, &Over) && (cbRead == cu8s));
    }

    bool Write(u64 Offset, u8* Source, u32 cu8s) {
      DWORD cbWritten;
      OVERLAPPED Over;

      memset(&Over, 0, sizeof Over);
      Over.Offset = static_cast<u32>(Offset);
      Over.OffsetHigh = (static_cast<u64>(Offset) >> 32);
      return (WriteFile(hFile, Source, cu8s, &cbWritten, &Over) && (cbWritten == cu8s));
    }

    bool isHandleValid() { return (hFile != INVALID_HANDLE_VALUE); }

  private:
    HANDLE hFile;
  };
#else
  class ReadWriteStorage {
  public:
    ReadWriteStorage(const std::string Filename) { fd = open(Filename.c_str(), O_RDWR); }
    ~ReadWriteStorage() {
      if (fd != -1) close(fd);
      fd = -1;
    }

    u64 Size() {
      struct stat st;
      if (fstat(fd, &st) != 0) return 0;
      return static_cast<u64>(st.st_size);
    }

    bool Read(u64 Offset, u8* Destination, u32 cu8s) {
      if (lseek(fd, Offset, SEEK_SET) == (off_t)-1) return false;
      ssize_t bytesRead = read(fd, Destination, cu8s);
      return bytesRead == static_cast<ssize_t>(cu8s);
    }

    bool Write(u64 Offset, u8* Source, u32 cu8s) {
      if (lseek(fd, Offset, SEEK_SET) == (off_t)-1) return false;
      ssize_t bytesWritten = write(fd, Source, cu8s);
      return bytesWritten == static_cast<ssize_t>(cu8s);
    }

    bool isHandleValid() { return (fd != -1); }

  private:
    int fd;
  };
#endif // ifdef _WIN32
} // namespace Base
