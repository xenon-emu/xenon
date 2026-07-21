/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/
#include "Core/RAM/RAM.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/HLE/Input/XInputBackend/XInputBackend.h"

#ifdef _WIN32
#include <Windows.h>
#include <xinput.h>
#endif // _WIN32

namespace Xe::Core::HLE {

template <typename T, std::endian E>
struct EndianStore {
  EndianStore() = default;
  EndianStore(const T &src) { set(src); }
  EndianStore(const EndianStore &other) { set(other); }
  operator T() const { return get(); }

  void set(const T &src) {
    if constexpr (std::endian::native == E) {
      value = src;
    }
    else {
      value = byteswap_be(src);
    }
  }
  void set(const EndianStore &other) { value = other.value; }
  T get() const {
    if constexpr (std::endian::native == E) {
      return value;
    }
    return byteswap_be(value);
  }

  EndianStore<T, E> &operator+=(int a) {
    *this = *this + a;
    return *this;
  }
  EndianStore<T, E> &operator-=(int a) {
    *this = *this - a;
    return *this;
  }
  EndianStore<T, E> &operator++() {
    *this += 1;
    return *this;
  }  // ++a
  EndianStore<T, E> operator++(int) {
    *this += 1;
    return (*this - 1);
  }  // a++
  EndianStore<T, E> &operator--() {
    *this -= 1;
    return *this;
  }  // --a
  EndianStore<T, E> operator--(int) {
    *this -= 1;
    return (*this + 1);
  }  // a--

  T value;
};

template <typename T>
using be = EndianStore<T, std::endian::big>;

template <typename T>
using le = EndianStore<T, std::endian::little>;

struct X_INPUT_GAMEPAD {
  be<u16> buttons;
  u8 leftTrigger;
  u8 rightTrigger;
  be<s16> thumbLX;
  be<s16> thumbLY;
  be<s16> thumbRX;
  be<s16> thumbRY;
};

struct X_INPUT_STATE {
  be<uint32_t> packetNumber;
  X_INPUT_GAMEPAD gamepad;
};

struct X_INPUT_VIBRATION {
  be<u16> leftMotorSpeed;
  be<u16> rightMotorSpeed;
};

struct X_INPUT_CAPABILITIES {
  u8 type;
  u8 subType;
  be<u16> flags;
  X_INPUT_GAMEPAD gamepad;
  X_INPUT_VIBRATION vibration;
};

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinput_keystroke(v=vs.85).aspx
struct X_INPUT_KEYSTROKE {
  be<u16> virtualKey;
  be<u16> unicode;
  be<u16> flags;
  u8 userIndex;
  u8 hidCode;
};

#ifndef _WIN32
struct XINPUT_KEYSTROKE {
  u16 VirtualKey;
  wchar_t Unicode;
  u16 Flags;
  u8 UserIndex;
  u8 HidCode;
};
typedef XINPUT_KEYSTROKE *PXINPUT_KEYSTROKE;

struct XINPUT_GAMEPAD {
  u16 wButtons;
  u8 bLeftTrigger;
  u8 bRightTrigger;
  s16 sThumbLX;
  s16 sThumbLY;
  s16 sThumbRX;
  s16 sThumbRY;
};
typedef XINPUT_GAMEPAD *PXINPUT_GAMEPAD;

struct XINPUT_VIBRATION {
  u16 wLeftMotorSpeed;
  u16 wRightMotorSpeed;
};
typedef XINPUT_VIBRATION *PXINPUT_VIBRATION;

struct XINPUT_CAPABILITIES {
  u8 Type;
  u8 SubType;
  u16 Flags;
  XINPUT_GAMEPAD Gamepad;
  XINPUT_VIBRATION Vibration;
};
typedef XINPUT_CAPABILITIES *PXINPUT_CAPABILITIES;

struct XINPUT_STATE {
  ul32 dwPacketNumber;
  XINPUT_GAMEPAD Gamepad;
};
typedef XINPUT_STATE *PXINPUT_STATE;

// TODO: Create a compat layer for other platforms
ul32 XInputGetCapabilities(ul32 dwUserIndex, ul32 dwReserved, PXINPUT_CAPABILITIES pCapabilities);
ul32 XInputGetKeystroke(ul32 dwUserIndex, ul32 dwReserved, PXINPUT_KEYSTROKE pKeystroke);
ul32 XInputGetState(ul32 dwUserIndex, PXINPUT_STATE pState);
ul32 XInputSetState(ul32 dwUserIndex, PXINPUT_VIBRATION pVibration);
#endif // !WIN32

XInputBackend::XInputBackend(RAM *inRamPtr)
  : ramPtr(inRamPtr)
{}

XInputBackend::~XInputBackend() {
  // Unload library if loaded and clean up function pointers
#ifdef _WIN32
  if (moduleHandle) {
    FreeLibrary((HMODULE)moduleHandle);
  }
  moduleHandle = nullptr;
#endif // WIN32
  XInputGetCapabilitiesPtr = nullptr;
  XInputGetStatePtr = nullptr;
  XInputGetKeystrokePtr = nullptr;
  XInputSetStatePtr = nullptr;
  XInputEnablePtr = nullptr;
}

bool XInputBackend::Setup() {
#ifdef _WIN32
  // Try to load XInput library
  HMODULE moduleHndl = LoadLibraryW(L"xinput1_4.dll");
  if (!moduleHndl) {
    moduleHndl = LoadLibraryW(L"xinput1_3.dll");
  }
  if (!moduleHndl) {
    return false;
  }

  // Function pointers for XInput related routines
  // Required:
  auto xinputGetCapabilities = GetProcAddress(moduleHndl, "XInputGetCapabilities");
  auto xinputGetState = GetProcAddress(moduleHndl, "XInputGetState");
  auto xinputGetKeystroke = GetProcAddress(moduleHndl, "XInputGetKeystroke");
  auto xinputSetState = GetProcAddress(moduleHndl, "XInputSetState");

  // Not required
  auto xinputEnable = GetProcAddress(moduleHndl, "XInputEnable");

  // Check for the needed modules
  if (!xinputGetCapabilities || !xinputGetState || !xinputGetKeystroke || !xinputSetState) {
    // Free library handle
    FreeLibrary(moduleHndl);
    // Return false as we got either one or more missing functions that are required
    return false;
  }

  // Set pointers
  moduleHandle = moduleHndl;
  XInputGetCapabilitiesPtr = reinterpret_cast<void *>(xinputGetCapabilities);
  XInputGetStatePtr = reinterpret_cast<void *>(xinputGetState);
  XInputGetKeystrokePtr = reinterpret_cast<void *>(xinputGetKeystroke);
  XInputSetStatePtr = reinterpret_cast<void *>(xinputSetState);
  XInputEnablePtr = reinterpret_cast<void *>(xinputEnable);

  // Library was present and all the required modules are here, great!

  SetName("XInput");
  SetPresent(true);

  LOG_INFO(HLE, "[Input]: HLE XInput backend initialized successfully.");
#else
  // Not supported outside windows :/
  SetPresent(false);
#endif // _WIN32

  return true;
}

void XInputBackend::GetState(sPPEState *ppeState) {
  u64 userIndex = curThread.GPR[3];
  u64 flags = curThread.GPR[4];
  u64 statePtr = curThread.GPR[5];

  XINPUT_STATE nativeState;
  auto xigs = (decltype(&XInputGetState))XInputGetStatePtr;
  ul32 result = xigs(userIndex, &nativeState);
  if (result) {
    return;
  }

  X_INPUT_STATE *outState = nullptr;
  u64 address = statePtr;
  PPCInterpreter::MMUTranslateAddress(&address, ppeState, false);
  outState = reinterpret_cast<X_INPUT_STATE *>(ramPtr->GetPointerToAddress(address));

  outState->packetNumber = nativeState.dwPacketNumber;
  outState->gamepad.buttons = nativeState.Gamepad.wButtons;
  outState->gamepad.leftTrigger = nativeState.Gamepad.bLeftTrigger;
  outState->gamepad.rightTrigger = nativeState.Gamepad.bRightTrigger;
  outState->gamepad.thumbLX = nativeState.Gamepad.sThumbLX;
  outState->gamepad.thumbLY = nativeState.Gamepad.sThumbLY;
  outState->gamepad.thumbRX = nativeState.Gamepad.sThumbRX;
  outState->gamepad.thumbRY = nativeState.Gamepad.sThumbRY;
  curThread.GPR[3] = 0;
  return;
}

void XInputBackend::SetState(sPPEState *ppeState) {
  u64 userIndex = curThread.GPR[3];
  u64 flags = curThread.GPR[4];
  u64 statePtr = curThread.GPR[5];

  X_INPUT_VIBRATION *vibration = nullptr;
  u64 address = statePtr;
  PPCInterpreter::MMUTranslateAddress(&address, ppeState, false);
  vibration = reinterpret_cast<X_INPUT_VIBRATION *>(ramPtr->GetPointerToAddress(address));

  XINPUT_VIBRATION nativeVibration;
  nativeVibration.wLeftMotorSpeed = vibration->leftMotorSpeed;
  nativeVibration.wRightMotorSpeed = vibration->rightMotorSpeed;
  auto xiss = (decltype(&XInputSetState))XInputSetStatePtr;
  ul32 result = xiss(userIndex, &nativeVibration);

  curThread.GPR[3] = 0;
  return;
}

void XInputBackend::GetCapabilities(sPPEState *ppeState) {
  u64 userIndex = curThread.GPR[3];
  u64 flags = curThread.GPR[4];
  u64 statePtr = curThread.GPR[5];

  XINPUT_CAPABILITIES nativeCaps;
  auto xigc = (decltype(&XInputGetCapabilities))XInputGetCapabilitiesPtr;
  ul32 result = xigc(userIndex, flags, &nativeCaps);
  if (result) {
    return;
  }

  X_INPUT_CAPABILITIES *outCaps = nullptr;
  u64 address = statePtr;
  PPCInterpreter::MMUTranslateAddress(&address, ppeState, false);
  outCaps = reinterpret_cast<X_INPUT_CAPABILITIES *>(ramPtr->GetPointerToAddress(address));

  outCaps->type = nativeCaps.Type;
  outCaps->subType = nativeCaps.SubType;
  outCaps->flags = nativeCaps.Flags;
  outCaps->gamepad.buttons = nativeCaps.Gamepad.wButtons;
  outCaps->gamepad.leftTrigger = nativeCaps.Gamepad.bLeftTrigger;
  outCaps->gamepad.rightTrigger = nativeCaps.Gamepad.bRightTrigger;
  outCaps->gamepad.thumbLX = nativeCaps.Gamepad.sThumbLX;
  outCaps->gamepad.thumbLY = nativeCaps.Gamepad.sThumbLY;
  outCaps->gamepad.thumbRX = nativeCaps.Gamepad.sThumbRX;
  outCaps->gamepad.thumbRY = nativeCaps.Gamepad.sThumbRY;
  outCaps->vibration.leftMotorSpeed = nativeCaps.Vibration.wLeftMotorSpeed;
  outCaps->vibration.rightMotorSpeed = nativeCaps.Vibration.wRightMotorSpeed;

  curThread.GPR[3] = 0;
  return;
}

void XInputBackend::GetKeystroke(sPPEState *ppeState) {
  u64 userIndexPtr = curThread.GPR[3];
  u64 flags = curThread.GPR[4];
  u64 statePtr = curThread.GPR[5];

  // XInputGetKeystroke on Windows has a bug where it will return
  // ERROR_SUCCESS (0) even if the device is not connected:
  // https://stackoverflow.com/questions/23669238/xinputgetkeystroke-returning-error-success-while-controller-is-unplugged
  //
  // So we first check if the device is connected via XInputGetCapabilities, so
  // we are not passing back an uninitialized X_INPUT_KEYSTROKE structure.
  // If any user (0xFF) is polled this bug does not occur but GetCapabilities
  // would fail so we need to skip it.

  ul32 result;
  PPCInterpreter::MMUTranslateAddress(&userIndexPtr, ppeState, false);
  u32 userIndex = 0;
  memcpy(&userIndex, ramPtr->GetPointerToAddress(userIndexPtr), 4);
  userIndex = byteswap_be(userIndex);
  if (userIndex != 0xFF) {
    XINPUT_CAPABILITIES caps;
    auto xigc = (decltype(&XInputGetCapabilities))XInputGetCapabilitiesPtr;
    result = xigc(userIndex, 0, &caps);
    if (result) {
      curThread.GPR[3] = result;
      return;
    }
  }

  X_INPUT_KEYSTROKE *outKeystroke = nullptr;
  u64 address = statePtr;
  PPCInterpreter::MMUTranslateAddress(&address, ppeState, false);
  outKeystroke = reinterpret_cast<X_INPUT_KEYSTROKE *>(ramPtr->GetPointerToAddress(address));

  XINPUT_KEYSTROKE nativeKeystroke;
  auto xigk = (decltype(&XInputGetKeystroke))XInputGetKeystrokePtr;
  result = xigk(userIndex, 0, &nativeKeystroke);
  if (result) {
    curThread.GPR[3] = result;
    return;
  }

  outKeystroke->virtualKey = nativeKeystroke.VirtualKey;
  outKeystroke->unicode = nativeKeystroke.Unicode;
  outKeystroke->flags = nativeKeystroke.Flags;
  outKeystroke->userIndex = nativeKeystroke.UserIndex;
  outKeystroke->hidCode = nativeKeystroke.HidCode;

  curThread.GPR[3] = 0;
  return;
}

} // namespace Xe::Core::HLE