// Copyright 2025 Xenon Emulator Project

#pragma once

#ifdef _WIN32
#include <windows.h>
#endif
#include <condition_variable>
#include <thread>

#include "Core/RootBus/HostBridge/PCIBridge/PCIBridge.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

//#define SOCKET_UART

#ifdef SOCKET_UART
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#define socketclose closesocket
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
#define socketclose close
#endif // _WIN32
#endif // SOCKET_UART

/*
        Xenon System Management Controller (SMC) Emulation.

        * The SMC is an Intel 8051 microcontroller inside the Southbridge, it
   handles low-level system tasks, such as the Power On Reset sequence, UART,
   Clock, DVD tray state, Tilt Status, IR Receiver, Temps, etc...

        Since emulating an 8051 core is just adding overhead to the emulator as
   of now i'm chosing just to do HLE for this.
*/

#define SMC_DEV_SIZE 0x100

namespace Xe {
namespace PCIDev {
namespace SMC {
// FIFO Queue Querys/Commands
enum SMC_FIFO_CMD {
  SMC_PWRON_TYPE = 0x1,
  SMC_QUERY_RTC = 0x4,
  SMC_QUERY_TEMP_SENS = 0x7,
  SMC_QUERY_TRAY_STATE = 0xA,
  SMC_QUERY_AVPACK = 0xF,
  SMC_I2C_READ_WRITE = 0x11,
  SMC_QUERY_VERSION = 0x12,
  SMC_FIFO_TEST = 0x13,
  SMC_QUERY_IR_ADDRESS = 0x16,
  SMC_QUERY_TILT_SENSOR = 0x17,
  SMC_READ_82_INT = 0x1E,
  SMC_READ_8E_INT = 0x20,
  SMC_SET_STANDBY = 0x82,
  SMC_SET_TIME = 0x85,
  SMC_SET_FAN_ALGORITHM = 0x88,
  SMC_SET_FAN_SPEED_CPU = 0x89,
  SMC_SET_DVD_TRAY = 0x8B,
  SMC_SET_POWER_LED = 0x8C,
  SMC_SET_AUDIO_MUTE = 0x8D,
  SMC_ARGON_RELATED = 0x90,
  SMC_SET_FAN_SPEED_GPU =
      0x94, // Not present on Slims, not used/respected on newer fat.
  SMC_SET_IR_ADDRESS = 0x95,
  SMC_SET_DVD_TRAY_SECURE = 0x98,
  SMC_SET_FP_LEDS = 0x99,
  SMC_SET_RTC_WAKE = 0x9A,
  SMC_ANA_RELATED = 0x9B,
  SMC_SET_ASYNC_OPERATION = 0x9C,
  SMC_SET_82_INT = 0x9D,
  SMC_SET_9F_INT = 0x9F
};

// SMC DVD Tray State
enum SMC_TRAY_STATE {
  SMC_TRAY_OPEN = 0x60,
  SMC_TRAY_OPEN_REQUEST = 0x61,
  SMC_TRAY_CLOSED = 0x62,
  SMC_TRAY_OPENING = 0x63,
  SMC_TRAY_CLOSING = 0x64,
  SMC_TRAY_UNKNOWN = 0x65,
  SMC_TRAY_SPINUP = 0x66
};

// SMC Power On Reason
enum SMC_PWR_REAS {
  SMC_PWR_REAS_PWRBTN = 0x11, // XSS 5 Power Button Pressed.
  SMC_PWR_REAS_EJECT = 0x12,  // XSS 6 eject button pushed
  SMC_PWR_REAS_ALARM = 0x15,  // XSS guess ~ should be the wake alarm ~
  SMC_PWR_REAS_REMOPWR =
      0x20, // XSS 2 power button on 3rd party remote/xbox universal remote
  SMC_PWR_REAS_REMOEJC = 0x21, // Eject button on xbox universal remote
  SMC_PWR_REAS_REMOX = 0x22,   // XSS 3 Xbox universal media remote X button
  SMC_PWR_REAS_WINBTN = 0x24,  // XSS 4 Windows button pushed IR remote
  SMC_PWR_REAS_RESET =
      0x30, // XSS HalReturnToFirmware(1 or 2 or 3) = hard reset by smc
  SMC_PWR_REAS_RECHARGE_RESET =
      0x31,                  // After leaving pnc charge mode via power button
  SMC_PWR_REAS_KIOSK = 0x41, // XSS 7 console powered on by kiosk pin
  SMC_PWR_REAS_WIRELESS =
      0x55, // XSS 8 wireless controller middle button/start button pushed to
            // power on controller and console
  SMC_PWR_REAS_WIRED_F1 = 0x56, // XSS 9 wired guide button; fat front top USB
                                // port, slim front left USB port
  SMC_PWR_REAS_WIRED_F2 = 0x57, // XSS A wired guide button; fat front botton
                                // USB port, slim front right USB port
  SMC_PWR_REAS_WIRED_R2 =
      0x58, // XSS B wired guide button; slim back middle USB port
  SMC_PWR_REAS_WIRED_R3 =
      0x59, // XSS C wired guide button; slim back top USB port
  SMC_PWR_REAS_WIRED_R1 = 0x5A // XSS D wired guide button; fat back USB port,
                               // slim back bottom USB port
                               //
  // Possibles/reboot reasons  0x23, 0x2A, 0x42, 0x61, 0x64.
  // slim with wired controller when horozontal, 3 back usb ports top to bottom
  // 0x59, 0x58, 0x5A front left 0x56, right 0x57. slim with wireless controller
  // w/pnc when horozontal, 3 back usb ports top to bottom 0x55, 0x58, 0x5A
  // front left 0x56, right 0x57. fat with wired controller when horozontal, 1
  // back usb port 0x5A front top 0x56, bottom 0x57 fat with wireless controller
  // w/pnc when horozontal, 1 back usb port 0x5A front top 0x56, bottom 0x57
  // Using Microsoft Wireless Controller: 0x55
  // Using Madcatz Wireless Keyboard (Rockband 3 Keyboard - Item Number 98161):
  // 0x55 Using Activision Wireless Turntable Controller (DJ Hero Turntable):
  // 0x55 Using Drums Controller from Activision Guitar Hero Warriors of Rock:
  // 0x55 Using Guitar controller from Activision Guitar Hero 5: 0x55
};

// AVPACK's Taken from libXenon.
enum SMC_AVPACK_TYPE {
  HDMI_AUDIO = 0x13,                 // HDMI_AUDIO
  HDMI_AUDIO_0x14 = 0x14,            // HDMI_AUDIO - GHETTO MOD
  HDMI_AUDIO_GHETTO_MOD = 0x1C,      // HDMI_AUDIO - GHETTO MOD
  HDMI_AUDIO_GHETTO_MOD_0x1e = 0x1E, // HDMI
  HDMI_NO_AUDIO = 0x1F,              // HDMI_NO_AUDIO
  COMPOSITE_TV_MODE = 0x43,          // COMPOSITE - TV MODE
  SCART = 0x47,                      // SCART
  COMPOSITE_S_VIDEO = 0x54,          // COMPOSITE + S-VIDEO
  COMPOSITE = 0x57,                  // NORMAL COMPOSITE
  COMPONENT = 0x0C,                  // COMPONENT
  COMPONENT_0xF = 0x0F,              // COMPONENT
  COMPOSITE_HD_MODE = 0x4F,          // COMPOSITE - HD MODE
  VGA = 0x5B,                        // VGA
  VGA_0x5B = 0x59,                   // VGA
  VGA_ADP_FIX = 0x1B                 // This fixes a generic VGA-HDMI Adapter
};

// We handle two states:
// 1. The SMC PCI State (SMC_PCI_STATE): This is what the system sees/has R/W
// access trough the PCI Bus.
// 2. The Inner State (SMC_CORE_STATE): tracking config settings like DVD Tray
// State, Current temps, Tilt status, etc...

// SMC PCI State: 255 Bytes long.
// There are some registers known, and some that aren't atm, so we'll be adding
// those later on.
struct SMC_PCI_STATE {
  u32 busControl;
  u32 reg04;
  u32 reg08;
  u32 reg0C;
  // Offset 0x10
  u32 uartInReg;
  // Offset 0x14
  u32 uartOutReg;
  // Offset 0x18
  u32 uartStatusReg;
  // Offset 0x1C
  u32 uartConfigReg;
  u32 reg20;
  u32 reg24;
  u32 reg28;
  u32 reg2C;
  u32 reg30;
  u32 reg34;
  u32 reg38;
  u32 reg3C;
  u32 reg40;
  u32 reg44;
  u32 reg48;
  u32 reg4C;
  u32 smiIntPendingReg;
  u32 reg54;
  u32 smiIntAckReg;
  u32 smiIntEnabledReg;
  u32 reg60;
  u32 clockIntEnabledReg;
  u32 reg68;
  u32 clockIntStatusReg;
  u32 reg70;
  u32 reg74;
  u32 reg78;
  u32 reg7C;
  // Offset 0x80
  u32 fifoInMsgReg;
  // Offset 0x84
  u32 fifoInStatusReg;
  u32 reg88;
  u32 reg8C;
  // Offset 0x90
  u32 fifoOutMsgReg;
  // Offset 0x94
  u32 fifoOutStatusReg;
  u32 reg98;
  u32 reg9C;
  u32 regA0;
  u32 regA4;
  u32 regA8;
  u32 regAC;
  u32 regB0;
  u32 regB4;
  u32 regB8;
  u32 regBC;
  u32 regC0;
  u32 regC4;
  u32 regC8;
  u32 regCC;
  u32 regD0;
  u32 regD4;
  u32 regD8;
  u32 regDC;
  u32 regE0;
  u32 regE4;
  u32 regE8;
  u32 regEC;
  u32 regF0;
  u32 regF4;
  u32 regF8;
  u32 regFC;
};

// SMC Core State, tracks current state of the system as per view from the SMC.
struct SMC_CORE_STATE {
  SMC_TRAY_STATE currTrayState;
  SMC_PWR_REAS currPowerOnReas;
  SMC_AVPACK_TYPE currAVPackType;

  // FIFO Data Queue (16 Bytes transmitted in 4 32 Bit words)
  u8 fifoDataBuffer[16];
  u8 fifoBufferPos = 0;

  // Default COM Port for opening.
  const char* currentCOMPort;
  // UART Initialized.
  bool uartInitialized;
  // UART Present. Used to do a one time check on UART COM Port on the host
  // system.
  bool uartPresent;
  // Use backup UART. If we are on Windows, and it doesn't have the proper COM port,
  // use this instead
  bool uartBackup;
#ifdef SOCKET_UART
  bool socketCreated;
  struct sockaddr_in sockAddr;
#ifdef _WIN32
  WSADATA wsaData;
  SOCKET sockHandle;
#else
  int sockHandle;
#endif // _WIN32
#else
#ifdef _WIN32
  // Current COM Port Device Control Block.
  // See
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-dcb
  DCB comPortDCB;
  // Current COM Port Handle.
  HANDLE comPortHandle;
  // Current COM Port Status.
  COMSTAT comPortStat = {};
  // Current COM Port error register used by ClearCommErr().
  DWORD comPortError = 0;
  // Bytes Written to the COM Port.
  DWORD currentBytesWrittenCount = 0;
  // Bytes Read from the COM Port.
  DWORD currentBytesReadCount = 0;
#endif // _WIN32
#endif // SOCKET_UART
  // Backup UART system
  std::queue<u8> uartTxBuffer;
  std::queue<u8> uartRxBuffer;
  std::mutex uartMutex;
  std::condition_variable uartConditionVar;
  bool uartThreadRunning;
  // Read/Write Return Status Values
  bool retVal = false;
};

// SMC Core Object.
class SMCCore : public PCIDevice {
public:
  SMCCore(const char* deviceName, u64 size,
    PCIBridge* parentPCIBridge, SMC_CORE_STATE* newSMCCoreState);
  ~SMCCore();

  // Read/Write functions.
  void Read(u64 readAddress, u64 *data, u8 byteCount) override;
  void ConfigRead(u64 readAddress, u64 *data, u8 byteCount) override;
  void Write(u64 writeAddress, u64 data, u8 byteCount) override;
  void ConfigWrite(u64 writeAddress, u64 data, u8 byteCount) override;

private:
  // Mutex, stops other threads from writing to values without the previous one finishing
  std::recursive_mutex mutex;

  // Parent PCI Bridge (Used for interrupts/communication):
  PCIBridge *pciBridge;

  // SMC PCI State, tracking all communcation with the system.
  SMC_PCI_STATE smcPCIState;

  // SMC Core State, tracking all general system status.
  SMC_CORE_STATE *smcCoreState;

  // SMC Thread object
  std::thread smcThread;

  // Is SMC Thread Running?
  bool smcThreadRunning = true;

  // UART Thread object
  std::thread uartThread;

#ifdef SOCKET_UART
  // UART Receive Thread object
  std::thread uartSecondaryThread;
#endif

  // SMC Main Thread
  void smcMainThread();

  // UART Thread
  void uartMainThread();

#ifdef SOCKET_UART
  // UART Receive Thread
  void uartReceiveThread();
#endif

  // UART/COM Port Setup
  void setupUART(u32 uartConfig);
};
} // namespace SMC
} // namespace PCIDev
} // namespace Xe
