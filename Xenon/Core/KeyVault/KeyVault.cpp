/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "KeyVault.h"

#include "Base/Crypto/Rc4.h"
#include "Base/Crypto/Sha1.h"
#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/StringUtil.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>

PARAM(dump_keyvault, "Writes the decrypted keyvault (0x4000 bytes) of the booted NAND to <path>");
PARAM(keyvault_info, "Writes the keyvault fields of the booted NAND, keys included, as text to <path>");

namespace Xe {

  // The keyvault layout, in offset order.
  // clang-format off
  constexpr KeyVault::Field kFields[] = {
    {XeKey::ManufacturingMode,            "ManufacturingMode",            0x18,  0x01,   false},
    {XeKey::AlternateKeyVault,            "AlternateKeyVault",            0x19,  0x01,   false},
    {XeKey::RestrictedPrivilegesFlags,    "RestrictedPrivilegesFlags",    0x1A,  0x01,   false},
    {XeKey::ReservedByte3,                "ReservedByte3",                0x1B,  0x01,   false},
    {XeKey::OddFeatures,                  "OddFeatures",                  0x1C,  0x02,   false},
    {XeKey::OddAuthType,                  "OddAuthType",                  0x1E,  0x02,   false},
    {XeKey::RestrictedHvExtLoader,        "RestrictedHvExtLoader",        0x20,  0x04,   false},
    {XeKey::PolicyFlashSize,              "PolicyFlashSize",              0x24,  0x04,   false},
    {XeKey::PolicyBuiltinUsbMuSize,       "PolicyBuiltinUsbMuSize",       0x28,  0x04,   false},
    {XeKey::ReservedDword4,               "ReservedDword4",               0x2C,  0x04,   false},
    {XeKey::RestrictedPrivileges,         "RestrictedPrivileges",         0x30,  0x08,   false},
    {XeKey::ReservedQword2,               "ReservedQword2",               0x38,  0x08,   false},
    {XeKey::ReservedQword3,               "ReservedQword3",               0x40,  0x08,   false},
    {XeKey::ReservedQword4,               "ReservedQword4",               0x48,  0x08,   false},
    {XeKey::ReservedKey1,                 "ReservedKey1",                 0x50,  0x10,   true},
    {XeKey::ReservedKey2,                 "ReservedKey2",                 0x60,  0x10,   true},
    {XeKey::ReservedKey3,                 "ReservedKey3",                 0x70,  0x10,   true},
    {XeKey::ReservedKey4,                 "ReservedKey4",                 0x80,  0x10,   true},
    {XeKey::ReservedRandomKey1,           "ReservedRandomKey1",           0x90,  0x10,   true},
    {XeKey::ReservedRandomKey2,           "ReservedRandomKey2",           0xA0,  0x10,   true},
    {XeKey::ConsoleSerialNumber,          "ConsoleSerialNumber",          0xB0,  0x0C,   false},
    {XeKey::MoboSerialNumber,             "MoboSerialNumber",             0xBC,  0x0C,   false},
    {XeKey::GameRegion,                   "GameRegion",                   0xC8,  0x02,   false},
    {XeKey::ConsoleObfuscationKey,        "ConsoleObfuscationKey",        0xD0,  0x10,   true},
    {XeKey::KeyObfuscationKey,            "KeyObfuscationKey",            0xE0,  0x10,   true},
    {XeKey::RoamableObfuscationKey,       "RoamableObfuscationKey",       0xF0,  0x10,   true},
    {XeKey::DvdKey,                       "DvdKey",                       0x100, 0x10,   true},
    {XeKey::PrimaryActivationKey,         "PrimaryActivationKey",         0x110, 0x18,   true},
    {XeKey::SecondaryActivationKey,       "SecondaryActivationKey",       0x128, 0x10,   true},
    {XeKey::GlobalDevice2DesKey1,         "GlobalDevice2DesKey1",         0x138, 0x10,   true},
    {XeKey::GlobalDevice2DesKey2,         "GlobalDevice2DesKey2",         0x148, 0x10,   true},
    {XeKey::WirelessControllerMs2DesKey1, "WirelessControllerMs2DesKey1", 0x158, 0x10,   true},
    {XeKey::WirelessControllerMs2DesKey2, "WirelessControllerMs2DesKey2", 0x168, 0x10,   true},
    {XeKey::WiredWebcamMs2DesKey1,        "WiredWebcamMs2DesKey1",        0x178, 0x10,   true},
    {XeKey::WiredWebcamMs2DesKey2,        "WiredWebcamMs2DesKey2",        0x188, 0x10,   true},
    {XeKey::WiredControllerMs2DesKey1,    "WiredControllerMs2DesKey1",    0x198, 0x10,   true},
    {XeKey::WiredControllerMs2DesKey2,    "WiredControllerMs2DesKey2",    0x1A8, 0x10,   true},
    {XeKey::MemoryUnitMs2DesKey1,         "MemoryUnitMs2DesKey1",         0x1B8, 0x10,   true},
    {XeKey::MemoryUnitMs2DesKey2,         "MemoryUnitMs2DesKey2",         0x1C8, 0x10,   true},
    {XeKey::OtherXsm3DeviceMs2DesKey1,    "OtherXsm3DeviceMs2DesKey1",    0x1D8, 0x10,   true},
    {XeKey::OtherXsm3DeviceMs2DesKey2,    "OtherXsm3DeviceMs2DesKey2",    0x1E8, 0x10,   true},
    {XeKey::WirelessController3p2DesKey1, "WirelessController3p2DesKey1", 0x1F8, 0x10,   true},
    {XeKey::WirelessController3p2DesKey2, "WirelessController3p2DesKey2", 0x208, 0x10,   true},
    {XeKey::WiredWebcam3p2DesKey1,        "WiredWebcam3p2DesKey1",        0x218, 0x10,   true},
    {XeKey::WiredWebcam3p2DesKey2,        "WiredWebcam3p2DesKey2",        0x228, 0x10,   true},
    {XeKey::WiredController3p2DesKey1,    "WiredController3p2DesKey1",    0x238, 0x10,   true},
    {XeKey::WiredController3p2DesKey2,    "WiredController3p2DesKey2",    0x248, 0x10,   true},
    {XeKey::MemoryUnit3p2DesKey1,         "MemoryUnit3p2DesKey1",         0x258, 0x10,   true},
    {XeKey::MemoryUnit3p2DesKey2,         "MemoryUnit3p2DesKey2",         0x268, 0x10,   true},
    {XeKey::OtherXsm3Device3p2DesKey1,    "OtherXsm3Device3p2DesKey1",    0x278, 0x10,   true},
    {XeKey::OtherXsm3Device3p2DesKey2,    "OtherXsm3Device3p2DesKey2",    0x288, 0x10,   true},
    {XeKey::ConsolePrivateKey,            "ConsolePrivateKey",            0x298, 0x1D0,  true},
    {XeKey::XeikaPrivateKey,              "XeikaPrivateKey",              0x468, 0x390,  true},
    {XeKey::CardeaPrivateKey,             "CardeaPrivateKey",             0x7F8, 0x1D0,  true},
    {XeKey::ConsoleCertificate,           "ConsoleCertificate",           0x9C8, 0x1A8,  false},
    // Key 0x37 as the 6717 kernel reads it starts two bytes into the 0x1288 byte slot at 0xB70 (the first two bytes are
    // not part of it): its OSIG record, magic at +0x110, is at keyvault 0xC82, and the slot still ends at 0x1DF8.
    {XeKey::XeikaCertificate,             "XeikaCertificate",             0xB72, 0x1286, false},
  };
  // clang-format on
  // Not reachable through the kernel's key query: keyvault signature 0x1DF8 (0x100), Cardea certificate 0x1EF8
  // (0x2108).

  // The console certificate's fields, as offsets into the certificate. Its first u16 is its own size.
  constexpr u32 kCertConsoleId = 0x02;    // 5 bytes
  constexpr u32 kCertPartNumber = 0x07;   // 11 chars
  constexpr u32 kCertPrivileges = 0x16;   // u16
  constexpr u32 kCertConsoleType = 0x18;  // u32
  constexpr u32 kCertManufactured = 0x1C; // 8 chars, MM-DD-YY

  constexpr u8 kDigestSalt[] = {0x07, 0x12};

  // A big-endian integer at the start of a slice, 0 for an empty one (the keyvault isn't usable).
  template <class T>
  static T BeValue(std::span<const u8> bytes) {
    return bytes.empty() ? T{} : LoadBe<T>(bytes.data());
  }

  // The printable characters up to the first NUL.
  static std::string Ascii(std::span<const u8> bytes) {
    std::string out;
    for (const u8 byte : bytes) {
      if (byte == 0) { break; }
      out += std::isprint(byte) ? static_cast<char>(byte) : '?';
    }
    return out;
  }

  // First and last two bytes only.
  static std::string MaskedHex(std::span<const u8> bytes) {
    if (bytes.size() < 4) { return std::string(bytes.size() * 2, '.'); }
    return fmt::format("{:02X}{:02X}....{:02X}{:02X}", bytes[0], bytes[1], bytes[bytes.size() - 2],
                       bytes[bytes.size() - 1]);
  }

  static const char *StateName(KeyVault::State state) {
    switch (state) {
      case KeyVault::State::NotLoaded: return "not loaded";
      case KeyVault::State::Verified: return "verified";
      case KeyVault::State::Unverified: return "UNVERIFIED (digest mismatch)";
      case KeyVault::State::Failed: return "failed";
    }
    return "?";
  }

  std::span<const KeyVault::Field> KeyVault::Fields() { return kFields; }

  const KeyVault::Field *KeyVault::FindField(XeKey key) {
    for (const Field &field : kFields) {
      if (field.key == key) { return &field; }
    }
    return nullptr;
  }

  bool KeyVault::ReadCpuKeyFile(const std::string &path, std::array<u8, CpuKeySize> &cpuKey) {
    if (path.empty()) { return false; }
    std::ifstream file(path);
    if (!file.is_open()) { return false; }

    std::string text;
    if (!std::getline(file, text)) { return false; }
    if (text.size() >= 3 && static_cast<u8>(text[0]) == 0xEF && static_cast<u8>(text[1]) == 0xBB
        && static_cast<u8>(text[2]) == 0xBF) {
      text.erase(0, 3);
    }
    if (const size_t colon = text.find(':'); colon != std::string::npos) { text.erase(0, colon + 1); }

    return ParseCpuKey(text, cpuKey);
  }

  bool KeyVault::ParseCpuKey(const std::string &text, std::array<u8, CpuKeySize> &cpuKey) {
    std::string hex;
    for (const char c : text) {
      if (std::isspace(static_cast<unsigned char>(c))) { continue; }
      if (!std::isxdigit(static_cast<unsigned char>(c))) { return false; }
      hex += c;
    }
    if (hex.size() != CpuKeySize * 2) { return false; }
    for (size_t i = 0; i < cpuKey.size(); ++i) {
      cpuKey[i] = static_cast<u8>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    return true;
  }

  void KeyVault::RememberCpuKey(const std::string &path, const std::array<u8, CpuKeySize> &cpuKey) const {
    if (state == State::Verified && !path.empty()) { WriteCpuKeyFile(path, cpuKey); }
  }

  bool KeyVault::WriteCpuKeyFile(const std::string &path, const std::array<u8, CpuKeySize> &cpuKey) {
    std::array<u8, CpuKeySize> existing{};
    if (ReadCpuKeyFile(path, existing) && existing == cpuKey) { return false; }

    const std::filesystem::path outputPath(path);
    std::error_code ec;
    if (std::filesystem::exists(outputPath, ec) && !ec) {
      std::filesystem::path backupPath = outputPath;
      backupPath += ".bak";
      if (!std::filesystem::exists(backupPath, ec) && !ec) {
        std::filesystem::copy_file(outputPath, backupPath, std::filesystem::copy_options::none, ec);
        if (ec) {
          LOG_ERROR(System, "KeyVault: could not preserve the old CPU key file at '{}': {}.", backupPath.string(),
                    ec.message());
          return false;
        }
      }
    }

    std::ofstream file(outputPath, std::ios::trunc);
    if (!file.is_open()) {
      LOG_ERROR(System, "KeyVault: cannot write CPU key file '{}'.", path);
      return false;
    }
    file << Base::HexString(cpuKey) << '\n';
    if (!file) {
      LOG_ERROR(System, "KeyVault: failed while writing CPU key file '{}'.", path);
      return false;
    }
    LOG_INFO(System, "KeyVault: wrote a NAND-verified CPU key to '{}'.", path);
    return true;
  }

  bool KeyVault::Decrypt(std::span<const u8> encrypted, const std::array<u8, CpuKeySize> &cpuKey) {
    data.clear();
    state = State::NotLoaded;
    if (encrypted.size() < MinSize || encrypted.size() > Size) { return false; }

    std::vector<u8> kv(encrypted.begin(), encrypted.end());

    // Decrypt everything past the nonce with RC4(HMAC-SHA1(cpu key, nonce)[0..16)).
    const std::span<const u8> nonce(kv.data(), 0x10);
    const Base::Crypto::Sha1::Digest rc4Key = Base::Crypto::HmacSha1(cpuKey, {nonce});
    Base::Crypto::Rc4 rc4({rc4Key.data(), 0x10});
    rc4.Process({kv.data() + 0x10, kv.size() - 0x10});

    const Base::Crypto::Sha1::Digest digest =
      Base::Crypto::HmacSha1(cpuKey, {std::span<const u8>(kv.data() + 0x10, kv.size() - 0x10), kDigestSalt});
    const bool digestOk = std::equal(digest.begin(), digest.begin() + 0x10, kv.begin());

    kv.resize(Size, 0);
    data = std::move(kv);

    // Independent of the digest, the console certificate says it is its own size and the serial is all digits.
    const std::span<const u8> certificate = Bytes(XeKey::ConsoleCertificate);
    const std::span<const u8> serial = Bytes(XeKey::ConsoleSerialNumber);
    const bool layoutOk = BeValue<u16>(certificate) == certificate.size()
                          && std::all_of(serial.begin(), serial.end(), [](u8 c) { return std::isdigit(c) != 0; });

    if (digestOk) {
      state = State::Verified;
    } else if (layoutOk) {
      state = State::Unverified;
    } else {
      state = State::Failed;
      data.clear();
      return false;
    }
    return true;
  }

  void KeyVault::ApplyCommandLine() const {
    if (PARAM_dump_keyvault.Present()) {
      const std::string path = PARAM_dump_keyvault.Get();
      if (DumpDecrypted(path)) { LOG_INFO(System, "KeyVault: decrypted keyvault written to '{}'.", path); }
    }
    if (PARAM_keyvault_info.Present()) {
      const std::string path = PARAM_keyvault_info.Get();
      if (DumpInfo(path)) { LOG_INFO(System, "KeyVault: keyvault info written to '{}'.", path); }
    }
  }

  std::span<const u8> KeyVault::Bytes(XeKey key) const {
    const Field *field = FindField(key);
    if (!field || data.size() < field->offset + field->size) { return {}; }
    return {data.data() + field->offset, field->size};
  }

  std::span<const u8> KeyVault::GetKey(XeKey key) const { return IsUsable() ? Bytes(key) : std::span<const u8>(); }

  std::span<const u8> KeyVault::Certificate(u32 offset, u32 size) const {
    const std::span<const u8> certificate = GetKey(XeKey::ConsoleCertificate);
    return certificate.empty() ? certificate : certificate.subspan(offset, size);
  }

  std::string KeyVault::ConsoleSerial() const { return Ascii(GetKey(XeKey::ConsoleSerialNumber)); }
  std::string KeyVault::MoboSerial() const { return Ascii(GetKey(XeKey::MoboSerialNumber)); }
  u16 KeyVault::GameRegion() const { return BeValue<u16>(GetKey(XeKey::GameRegion)); }
  u16 KeyVault::OddFeatures() const { return BeValue<u16>(GetKey(XeKey::OddFeatures)); }
  u16 KeyVault::OddAuthType() const { return BeValue<u16>(GetKey(XeKey::OddAuthType)); }
  u8 KeyVault::ManufacturingMode() const { return BeValue<u8>(GetKey(XeKey::ManufacturingMode)); }

  std::array<u8, 5> KeyVault::ConsoleId() const {
    std::array<u8, 5> id{};
    const std::span<const u8> bytes = Certificate(kCertConsoleId, id.size());
    std::copy(bytes.begin(), bytes.end(), id.begin());
    return id;
  }

  std::string KeyVault::ConsoleIdString() const {
    // The kernel prints the 40-bit ID as 11 decimal digits of it without its low nibble, then that nibble in hex.
    u64 id = 0;
    for (const u8 b : ConsoleId()) { id = (id << 8) | b; }
    return fmt::format("{:011}{:X}", id >> 4, id & 0xF);
  }

  std::string KeyVault::ConsolePartNumber() const { return Ascii(Certificate(kCertPartNumber, 11)); }
  std::string KeyVault::ManufacturingDate() const { return Ascii(Certificate(kCertManufactured, 8)); }
  u32 KeyVault::ConsoleType() const { return BeValue<u32>(Certificate(kCertConsoleType, 4)); }
  u16 KeyVault::CertificatePrivileges() const { return BeValue<u16>(Certificate(kCertPrivileges, 2)); }

  void KeyVault::LogSummary() const {
    LOG_INFO(System, "KeyVault: {}", StateName(state));
    if (!IsUsable()) { return; }
    LOG_INFO(System, " * Console serial:     {}", ConsoleSerial());
    LOG_INFO(System, " * Motherboard serial: {}", MoboSerial());
    LOG_INFO(System, " * Console ID:         {} ({})", ConsoleIdString(), Base::HexString(ConsoleId()));
    LOG_INFO(System, " * Part number:        {}", ConsolePartNumber());
    LOG_INFO(System, " * Manufactured:       {}", ManufacturingDate());
    LOG_INFO(System, " * Console type:       0x{:08X}, privileges 0x{:04X}", ConsoleType(), CertificatePrivileges());
    LOG_INFO(System, " * Game region:        0x{:04X}", GameRegion());
    LOG_INFO(System, " * ODD features/auth:  0x{:04X} / 0x{:04X}", OddFeatures(), OddAuthType());
    LOG_INFO(System, " * Manufacturing mode: 0x{:02X}", ManufacturingMode());
    LOG_INFO(System, " * DVD key:            {}", MaskedHex(GetKey(XeKey::DvdKey)));
    LOG_INFO(System, " * XSM3 device keys:   {} {}", MaskedHex(GetKey(XeKey::GlobalDevice2DesKey1)),
             MaskedHex(GetKey(XeKey::GlobalDevice2DesKey2)));
    LOG_INFO(System, " * Wired pad keys:     {} {}", MaskedHex(GetKey(XeKey::WiredControllerMs2DesKey1)),
             MaskedHex(GetKey(XeKey::WiredControllerMs2DesKey2)));
  }

  bool KeyVault::DumpDecrypted(const std::string &path) const {
    if (!IsUsable()) {
      LOG_ERROR(System, "KeyVault: nothing to dump ({}).", StateName(state));
      return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open() || !file.write(reinterpret_cast<const char *>(data.data()), data.size())) {
      LOG_ERROR(System, "KeyVault: cannot write '{}'.", path);
      return false;
    }
    return true;
  }

  bool KeyVault::DumpInfo(const std::string &path) const {
    if (!IsUsable()) {
      LOG_ERROR(System, "KeyVault: nothing to dump ({}).", StateName(state));
      return false;
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
      LOG_ERROR(System, "KeyVault: cannot write '{}'.", path);
      return false;
    }
    file << fmt::format("Keyvault ({})\n", StateName(state));
    file << fmt::format("Console serial      {}\n", ConsoleSerial());
    file << fmt::format("Motherboard serial  {}\n", MoboSerial());
    file << fmt::format("Console ID          {} ({})\n", ConsoleIdString(), Base::HexString(ConsoleId()));
    file << fmt::format("Part number         {}\n", ConsolePartNumber());
    file << fmt::format("Manufactured        {}\n", ManufacturingDate());
    file << fmt::format("Console type        0x{:08X}\n", ConsoleType());
    file << fmt::format("Cert privileges     0x{:04X}\n", CertificatePrivileges());
    file << fmt::format("Game region         0x{:04X}\n\n", GameRegion());
    file << "XeKey offset  size  name\n";
    for (const Field &field : kFields) {
      const std::span<const u8> bytes(data.data() + field.offset, field.size);
      // Private keys and certificates are too long for a line: see the decrypted dump.
      const std::string value = field.size <= 0x18 ? Base::HexString(bytes) : fmt::format("({} bytes)", field.size);
      file << fmt::format("0x{:02X}  0x{:04X} 0x{:04X} {:<30} {}\n", static_cast<u32>(field.key), field.offset,
                          field.size, field.name, value);
    }
    return static_cast<bool>(file);
  }

} // namespace Xe
