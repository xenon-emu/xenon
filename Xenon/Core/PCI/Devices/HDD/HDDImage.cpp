/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "HDDImage.h"

#include "Base/Crypto/Sha1.h"
#include "Base/Logging/Log.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#ifdef _WIN32
  #include <windows.h>
  #include <winioctl.h>
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

namespace Xe::HDDImage {

  constexpr u32 kSectorSize = 0x200;
  // Every retail partition has 16 KiB clusters.
  constexpr u32 kClusterSectors = 0x20;
  constexpr u32 kClusterSize = kClusterSectors * kSectorSize;
  // Where the content partition starts: the kernel's fixed layout when sector 0 holds no devkit partition table.
  constexpr u64 kPartition1Start = 0x130EB0000;
  // Raw images (of a whole drive) are far bigger than any dump of the sector.
  constexpr u64 kDumpLimit = 0x100000;

  // The partitions the kernel lays out before the content partition.
  struct Partition {
    const char *name;
    u64 offset, size;
  };
  constexpr Partition kFixedPartitions[] = {
    {"Cache0", 0x00080000, 0x80000000},          {"Cache1", 0x80080000, 0x80000000},
    {"SystemURLCache", 0x100080000, 0x06000000}, {"TitleURLCache", 0x106080000, 0x02000000},
    {"SystemExt", 0x10C080000, 0x0CE30000},      {"SystemAux", 0x118EB0000, 0x08000000},
    {"System", 0x120EB0000, 0x10000000},
  };

  // Trims the padding of an ATA string.
  static std::string Trim(const u8 *data, size_t size) {
    std::string text(reinterpret_cast<const char *>(data), size);
    const size_t first = text.find_first_not_of(' ');
    if (first == std::string::npos) { return {}; }
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
  }

  // Fills in everything a sector's data tells: the identity, the signed count, the logo check, and what the kernel
  // rejects.
  static void Analyze(SecuritySector &s) {
    const u8 *d = s.data.data();
    s.serial = Trim(d, 0x14);
    s.firmware = Trim(d + 0x14, 8);
    s.model = Trim(d + 0x1C, 0x28);
    s.sectors = static_cast<u32>(d[0x58] | (d[0x59] << 8) | (d[0x5A] << 16) | (static_cast<u32>(d[0x5B]) << 24));
    // The kernel hashes the 0xC00 bytes at 0x2200 and compares them with the digest at +0x44.
    s.logoOk =
      s.hasLogo && std::equal(d + 0x44, d + 0x58, Base::Crypto::Sha1::Hash({d + kSecuritySectorSize, 0xC00}).begin());

    s.problem.clear();
    if (s.serial.empty() || s.model.empty()) {
      s.problem = "This is not a security sector, it has no drive serial or model.";
    } else if (!s.hasLogo) {
      s.problem = "The logo behind the sector is missing (0xE00 bytes), the kernel would reject the drive.";
    } else if (!s.logoOk) {
      s.problem = "The logo does not match the digest in the sector, the kernel would reject the drive.";
    } else if (s.sectors == 348 || static_cast<u64>(s.sectors) * kSectorSize <= kPartition1Start + 0x10000) {
      s.problem = "The sector count signed into this sector is too small for a retail drive.";
    }
  }

  bool ReadSecuritySector(const std::string &path, SecuritySector &out, std::string &error) {
    out = {};
    std::error_code ec;
    const u64 size = std::filesystem::file_size(path, ec);
    if (ec) {
      error = "Cannot read the file.";
      return false;
    }
    if (size < kSecuritySectorSize) {
      error = "The file is shorter than a security sector (512 bytes).";
      return false;
    }
    // A dump of the sector starts at its serial. An image of a drive holds it at 0x2000.
    const u64 at = size > kDumpLimit ? kSecuritySectorOffset : 0;
    std::ifstream file(path, std::ios::binary);
    file.seekg(static_cast<std::streamoff>(at));
    file.read(reinterpret_cast<char *>(out.data.data()),
              static_cast<std::streamsize>(std::min<u64>(kSecurityRegionSize, size - at)));
    const u64 got = static_cast<u64>(file.gcount());
    if (!file || got < kSecuritySectorSize) {
      error = "Cannot read the file.";
      return false;
    }
    out.hasLogo = got >= kSecurityRegionSize;
    Analyze(out);
    return true;
  }

  // A file written at any offset, sparse where the platform can.
  struct ImageFile {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool Create(const std::string &path) {
      handle =
        CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (handle == INVALID_HANDLE_VALUE) { return false; }
      // Without this the file takes its full size on disk (a file system without sparse files stays that way).
      DWORD returned = 0;
      DeviceIoControl(handle, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr);
      return true;
    }
    bool WriteAt(u64 offset, const void *data, size_t size) {
      LARGE_INTEGER to;
      to.QuadPart = static_cast<LONGLONG>(offset);
      DWORD written = 0;
      return SetFilePointerEx(handle, to, nullptr, FILE_BEGIN)
             && WriteFile(handle, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
    }
    ~ImageFile() {
      if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); }
    }
#else
    int fd = -1;
    bool Create(const std::string &path) {
      fd = open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
      return fd >= 0;
    }
    bool WriteAt(u64 offset, const void *data, size_t size) {
      return pwrite(fd, data, size, static_cast<off_t>(offset)) == static_cast<ssize_t>(size);
    }
    ~ImageFile() {
      if (fd >= 0) { close(fd); }
    }
#endif
  };

  // Formats an empty FATX volume the way the kernel reads it: the "XTAF" header, the FAT at +0x1000 (16-bit entries
  // while the cluster count stays below 0xFFF0, else 32-bit, sized to 4 KiB), cluster 1 as the root directory at the
  // start of the data area, and directory clusters filled with 0xFF. Everything is big endian.
  static bool FormatFatx(ImageFile &file, u64 offset, u64 size, std::mt19937 &random) {
    const u64 clusters = size / kClusterSize;
    const u64 entries = clusters + 1;
    const bool wide = entries >= 0xFFF0;
    const u64 fatSize = (entries * (wide ? 4 : 2) + 0xFFF) & ~u64(0xFFF);
    if (fatSize >= size - 0x1000) { return false; }

    const auto be32 = [](u8 *at, u32 value) {
      at[0] = static_cast<u8>(value >> 24);
      at[1] = static_cast<u8>(value >> 16);
      at[2] = static_cast<u8>(value >> 8);
      at[3] = static_cast<u8>(value);
    };
    std::vector<u8> header(0x1000, 0xFF);
    std::copy_n("XTAF", 4, header.begin());
    be32(&header[4], random());
    be32(&header[8], kClusterSectors);
    be32(&header[12], 1);
    // FAT entry 0 is the media descriptor, entry 1 ends the root directory's chain. The rest is free.
    u8 fatHead[8] = {};
    if (wide) {
      be32(fatHead, 0xFFFFFFF8);
      be32(fatHead + 4, 0xFFFFFFFF);
    } else {
      fatHead[0] = 0xFF, fatHead[1] = 0xF8, fatHead[2] = 0xFF, fatHead[3] = 0xFF;
    }
    const std::vector<u8> root(kClusterSize, 0xFF);
    return file.WriteAt(offset, header.data(), header.size()) && file.WriteAt(offset + 0x1000, fatHead, wide ? 8 : 4)
           && file.WriteAt(offset + 0x1000 + fatSize, root.data(), root.size());
  }

  bool CreateRetailImage(const SecuritySector &security, const std::string &path, std::string &error) {
    if (!security.problem.empty()) {
      error = security.problem;
      return false;
    }
    if (std::filesystem::exists(path)) {
      error = "That file already exists, Xenon will not overwrite it.";
      return false;
    }
    bool created = false, ok = false;
    {
      ImageFile file;
      created = file.Create(path);
      if (created) {
        // Extends the file to its size by writing the last byte: a resize would zero-fill it.
        const u64 total = static_cast<u64>(security.sectors) * kSectorSize;
        const u8 zero = 0;
        ok = file.WriteAt(total - 1, &zero, 1)
             && file.WriteAt(kSecuritySectorOffset, security.data.data(), security.data.size());

        std::mt19937 random{std::random_device{}()};
        for (const Partition &partition : kFixedPartitions) {
          ok = ok && FormatFatx(file, partition.offset, partition.size, random);
        }
        // The content partition ends where the kernel restricts the drive: the signed count.
        ok = ok && FormatFatx(file, kPartition1Start, total - kPartition1Start, random);
      }
    } // The file is closed here, so that a failed image can be deleted.
    if (!created) {
      error = "Cannot create the file (is the folder there and writable?).";
      return false;
    }
    if (!ok) {
      error = "Writing the image failed (is there room on the disk?).";
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
      return false;
    }
    LOG_INFO(HDD, "Created a retail hard drive image {}: model '{}', serial '{}', {} sectors.", path, security.model,
             security.serial, security.sectors);
    return true;
  }

} // namespace Xe::HDDImage
