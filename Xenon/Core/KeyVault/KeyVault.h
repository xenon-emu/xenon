/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>

// Keyvault of a console, decrypted from the copy NandInfo extracted out of its NAND.
//
// The NAND keeps the keyvault (0x4000 bytes at the logical address in the NAND header, +0x6C) encrypted with the
// console's CPU key (fuse lines 3 and 5). Its first 16 bytes are a nonce that doubles as the digest. The RC4 key is the
// first 16 bytes of HMAC-SHA1(CPU key, nonce), the rest of the keyvault is RC4 encrypted with it, and it checks out
// when the first 16 bytes of HMAC-SHA1(CPU key, plain | 07 12) equal the nonce.
//
// The layout below is the kernel's keyvault structure and the XeKey numbers are the ones its key query takes. They
// were checked against this kernel: key 0x36, the console certificate, is 0x1A8 bytes, the console ID sits at
// certificate +2, and the USB security category tables pick 0x1D/0x1E and 0x1F..0x32 as the 2DES key pairs.
//
// NandInfo reads the encrypted keyvault out of the NAND and owns the decrypted KeyVault as a member. The devices that
// need keys (ODD, the USB pads) read it from the console's NandInfo when they are constructed.

namespace Xe {

  // The keyvault fields, numbered the way the kernel's key query takes them.
  enum class XeKey : u8 {
    ManufacturingMode = 0x00,
    AlternateKeyVault = 0x01,
    RestrictedPrivilegesFlags = 0x02,
    ReservedByte3 = 0x03,
    OddFeatures = 0x04,
    OddAuthType = 0x05,
    RestrictedHvExtLoader = 0x06,
    PolicyFlashSize = 0x07,
    PolicyBuiltinUsbMuSize = 0x08,
    ReservedDword4 = 0x09,
    RestrictedPrivileges = 0x0A,
    ReservedQword2 = 0x0B,
    ReservedQword3 = 0x0C,
    ReservedQword4 = 0x0D,
    ReservedKey1 = 0x0E,
    ReservedKey2 = 0x0F,
    ReservedKey3 = 0x10,
    ReservedKey4 = 0x11,
    ReservedRandomKey1 = 0x12,
    ReservedRandomKey2 = 0x13,
    ConsoleSerialNumber = 0x14,
    MoboSerialNumber = 0x15,
    GameRegion = 0x16,
    ConsoleObfuscationKey = 0x17,
    KeyObfuscationKey = 0x18,
    RoamableObfuscationKey = 0x19,
    DvdKey = 0x1A,
    PrimaryActivationKey = 0x1B,
    SecondaryActivationKey = 0x1C,
    GlobalDevice2DesKey1 = 0x1D,
    GlobalDevice2DesKey2 = 0x1E,
    WirelessControllerMs2DesKey1 = 0x1F,
    WirelessControllerMs2DesKey2 = 0x20,
    WiredWebcamMs2DesKey1 = 0x21,
    WiredWebcamMs2DesKey2 = 0x22,
    WiredControllerMs2DesKey1 = 0x23,
    WiredControllerMs2DesKey2 = 0x24,
    MemoryUnitMs2DesKey1 = 0x25,
    MemoryUnitMs2DesKey2 = 0x26,
    OtherXsm3DeviceMs2DesKey1 = 0x27,
    OtherXsm3DeviceMs2DesKey2 = 0x28,
    WirelessController3p2DesKey1 = 0x29,
    WirelessController3p2DesKey2 = 0x2A,
    WiredWebcam3p2DesKey1 = 0x2B,
    WiredWebcam3p2DesKey2 = 0x2C,
    WiredController3p2DesKey1 = 0x2D,
    WiredController3p2DesKey2 = 0x2E,
    MemoryUnit3p2DesKey1 = 0x2F,
    MemoryUnit3p2DesKey2 = 0x30,
    OtherXsm3Device3p2DesKey1 = 0x31,
    OtherXsm3Device3p2DesKey2 = 0x32,
    ConsolePrivateKey = 0x33,
    XeikaPrivateKey = 0x34,
    CardeaPrivateKey = 0x35,
    ConsoleCertificate = 0x36,
    XeikaCertificate = 0x37,
  };

  class KeyVault {
  public:
    static constexpr u32 Size = 0x4000;
    static constexpr u32 CpuKeySize = 16;
    // Smallest encrypted keyvault that still holds the console certificate (certificate at 0x9C8, 0x1A8 bytes).
    static constexpr u32 MinSize = 0x9C8 + 0x1A8;

    enum class State : u8 {
      NotLoaded,  // no attempt yet, or missing fuses / NAND
      Verified,   // digest matched: the CPU key belongs to this NAND
      Unverified, // digest mismatch but the layout decodes (console certificate and serial look right)
      Failed,     // decrypts to garbage: wrong CPU key or no keyvault
    };

    struct Field {
      XeKey key;
      const char *name;
      u16 offset;
      u16 size;
      bool secret; // key material: masked in logs, written in full only to the info dump
    };
    // Every keyvault field, in layout order.
    static std::span<const Field> Fields();
    static const Field *FindField(XeKey key);

    // Plain 32-digit CPU key file. It is only ever written with a key that verified against a NAND keyvault.
    static bool ReadCpuKeyFile(const std::string &path, std::array<u8, CpuKeySize> &cpuKey);
    // 32 hex digits, any whitespace ignored.
    static bool ParseCpuKey(const std::string &text, std::array<u8, CpuKeySize> &cpuKey);

    // Decrypts encrypted (the keyvault as the NAND stores it, NandInfo extracts it) with the CPU key and checks it.
    // True when it is usable (Verified or Unverified).
    bool Decrypt(std::span<const u8> encrypted, const std::array<u8, CpuKeySize> &cpuKey);
    // Forgets the keyvault (no key or NAND to decrypt it with).
    void Clear() {
      data.clear();
      state = State::NotLoaded;
    }
    // Saves cpuKey to the key file at path, when it is the key that verified this keyvault.
    void RememberCpuKey(const std::string &path, const std::array<u8, CpuKeySize> &cpuKey) const;
    // Handles -dump_keyvault / -keyvault_info.
    void ApplyCommandLine() const;

    State GetState() const { return state; }
    bool IsUsable() const { return state == State::Verified || state == State::Unverified; }

    // The whole decrypted keyvault (empty while it isn't usable), for records no XeKey field covers.
    std::span<const u8> Raw() const { return IsUsable() ? std::span<const u8>(data) : std::span<const u8>(); }

    // A field's bytes (empty while the keyvault isn't usable).
    std::span<const u8> GetKey(XeKey key) const;
    template <size_t N>
    bool GetKey(XeKey key, std::array<u8, N> &out) const {
      const std::span<const u8> bytes = GetKey(key);
      if (bytes.size() != N) { return false; }
      std::copy(bytes.begin(), bytes.end(), out.begin());
      return true;
    }

    std::string ConsoleSerial() const;
    std::string MoboSerial() const;
    u16 GameRegion() const;
    u16 OddFeatures() const;
    u16 OddAuthType() const;
    u8 ManufacturingMode() const;
    // Console certificate fields.
    std::array<u8, 5> ConsoleId() const;
    std::string ConsoleIdString() const; // as the kernel formats it
    std::string ConsolePartNumber() const;
    std::string ManufacturingDate() const;
    u32 ConsoleType() const;
    u16 CertificatePrivileges() const;

    void LogSummary() const;
    // Raw decrypted keyvault (Size bytes, nonce included).
    bool DumpDecrypted(const std::string &path) const;
    // Human-readable fields, key material in full.
    bool DumpInfo(const std::string &path) const;

  private:
    static bool WriteCpuKeyFile(const std::string &path, const std::array<u8, CpuKeySize> &cpuKey);

    // A field's bytes whether or not the keyvault was judged usable yet (Decrypt checks the layout with it).
    std::span<const u8> Bytes(XeKey key) const;
    // A slice of the console certificate (empty while the keyvault isn't usable).
    std::span<const u8> Certificate(u32 offset, u32 size) const;

    std::vector<u8> data; // decrypted, Size bytes
    State state = State::NotLoaded;
  };

} // namespace Xe
