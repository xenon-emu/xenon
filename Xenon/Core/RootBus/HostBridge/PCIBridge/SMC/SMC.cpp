// Copyright 2025 Xenon Emulator Project

#include "SMC.h"

#include "Base/Logging/Log.h"
#include "Base/Error.h"

#include "HANA_State.h"
#include "SMC_Config.h"

//
// Registers Offsets
//

// UART Region
#define UART_BYTE_OUT_REG 0x10
#define UART_BYTE_IN_REG 0x14
#define UART_STATUS_REG 0x18
#define UART_CONFIG_REG 0x1C

// SMI Region
#define SMI_INT_STATUS_REG 0x50
#define SMI_INT_ACK_REG 0x58
#define SMI_INT_ENABLED_REG 0x5C

// Clock Region
#define CLCK_INT_ENABLED_REG 0x64
#define CLCK_INT_STATUS_REG 0x6C

// FIFO Region
#define FIFO_IN_DATA_REG 0x80
#define FIFO_IN_STATUS_REG 0x84
#define FIFO_OUT_DATA_REG 0x90
#define FIFO_OUT_STATUS_REG 0x94

//
// UART Definitions
//
#define UART_STATUS_EMPTY 0x2
#define UART_STATUS_DATA_PRES 0x1

//
// FIFO Definitions
//
#define FIFO_STATUS_READY 0x4
#define FIFO_STATUS_BUSY 0x0

//
// SMI Definitions
//
#define SMI_INT_ENABLED 0xC
#define SMI_INT_NONE 0x0
#define SMI_INT_PENDING 0x10000000

//
// Clock Definitions
//
#define CLCK_INT_ENABLED 0x10000000
#define CLCK_INT_READY 0x1
#define CLCK_INT_TAKEN 0x3

// Class Constructor.
Xe::PCIDev::SMC::SMCCore::SMCCore(const char *deviceName, u64 size,
  PCIBridge *parentPCIBridge, SMC_CORE_STATE *newSMCCoreState) :
  PCIDevice(deviceName, size) {
  LOG_INFO(SMC, "Core: Initializing...");

  // Assign our parent PCI Bus Ptr.
  pciBridge = parentPCIBridge;

  // Assign our core sate, this is already filled with config data regarding
  // AVPACK, PWRON Reason and TrayState.
  smcCoreState = newSMCCoreState;

  // Create a new SMC PCI State
  memset(&smcPCIState, 0, sizeof(smcPCIState));

  // Set UART Status to Empty.
  smcPCIState.uartStatusReg = UART_STATUS_EMPTY;

  // Set PCI Config Space registers.
  memcpy(pciConfigSpace.data, smcConfigSpaceMap, sizeof(smcConfigSpaceMap));

  // Set our PCI Dev Sizes.
  pciDevSizes[0] = 0x100; // BAR0

  // Set UART Presence.
  smcCoreState->uartPresent = true;

  // Enter main execution thread.
  smcThread = std::thread(&SMCCore::smcMainThread, this);
}

// Class Destructor.
Xe::PCIDev::SMC::SMCCore::~SMCCore() {
  LOG_INFO(SMC, "Core: Exiting.");
  smcThreadRunning = false;
  if (smcThread.joinable())
    smcThread.join();
  // If we are on Linux, or the backup system is running, 
  // kill it.
  smcCoreState->uartThreadRunning = false;
#ifdef SOCKET_UART
  // Shutdown receive thread
  if (uartSecondaryThread.joinable())
    uartSecondaryThread.join();
  // Shutdown socket
  if (smcCoreState->socketCreated) {
#ifdef _WIN32
    shutdown(smcCoreState->sockHandle, SD_BOTH);
#endif // _WIN32
    socketclose(smcCoreState->sockHandle);
  }
#endif // SOCKET_UART
}

// PCI Read
void Xe::PCIDev::SMC::SMCCore::Read(u64 readAddress, u64 *data, u8 byteCount) {
  const u8 regOffset = static_cast<u8>(readAddress);

  mutex.lock();
  switch (regOffset) {
  case UART_CONFIG_REG: // UART Config Register
    memcpy(data, &smcPCIState.uartConfigReg, byteCount);
    break;
  case UART_BYTE_OUT_REG: // UART Data Out Register  
#if defined(_WIN32) && !defined(SOCKET_UART)
    smcCoreState->retVal =
        ReadFile(smcCoreState->comPortHandle, &smcPCIState.uartOutReg, 1,
                 &smcCoreState->currentBytesReadCount, nullptr);
#endif // _WIN32 && !SOCKET_UART
    if (smcCoreState->uartBackup) {
      smcCoreState->retVal = false;
      {
        // We love mutexes
        std::lock_guard<std::mutex> lock(smcCoreState->uartMutex);
        if (!smcCoreState->uartRxBuffer.empty()) {
          smcPCIState.uartOutReg = smcCoreState->uartRxBuffer.front();
          smcCoreState->uartRxBuffer.pop();
          smcCoreState->retVal = true;
        }
      }     
    }
    if (smcCoreState->retVal) {
      memcpy(data, &smcPCIState.uartOutReg, byteCount);
    }
    break;
  case UART_STATUS_REG: // UART Status Register
    // First lets check if the UART has already been setup, if so, proceed to do
    // the TX/RX.                   
#if defined(_WIN32) && !defined(SOCKET_UART)
    if (smcCoreState->uartInitialized) {
      // Get current COM Port Status
      ClearCommError(smcCoreState->comPortHandle, &smcCoreState->comPortError,
                     &smcCoreState->comPortStat);
      if (smcCoreState->comPortStat.cbInQue >
          0) // The queue has any bytes remaining?
      {
        // Got something to read in the input queue.
        smcPCIState.uartStatusReg = UART_STATUS_DATA_PRES;
      } else {
        // The input queue is empty.
        smcPCIState.uartStatusReg = UART_STATUS_EMPTY;
      }
    }
#endif // _WIN32 && !SOCKET_UART
    if (smcCoreState->uartBackup) {
      if (smcCoreState->uartInitialized) {
        smcPCIState.uartStatusReg = smcCoreState->uartRxBuffer.empty() ? UART_STATUS_EMPTY : UART_STATUS_DATA_PRES;
      }
    }
    // Check if UART is already initialized.
    if (!smcCoreState->uartInitialized && smcCoreState->uartPresent) {
      // XeLL doesn't initialize UART before sending data trough it. Initialize
      // it first then.
      setupUART(0x1e6); // 115200,8,N,1.
    }
    memcpy(data, &smcPCIState.uartStatusReg, byteCount);
    break;
  case SMI_INT_STATUS_REG: // SMI INT Status Register
    memcpy(data, &smcPCIState.smiIntPendingReg, byteCount);
    break;
  case SMI_INT_ACK_REG: // SMI INT ACK Register
    memcpy(data, &smcPCIState.smiIntAckReg, byteCount);
    break;
  case SMI_INT_ENABLED_REG: // SMI INT Enabled Register
    memcpy(data, &smcPCIState.smiIntEnabledReg, byteCount);
    break;
  case FIFO_IN_STATUS_REG: // FIFO In Status Register
    memcpy(data, &smcPCIState.fifoInStatusReg, byteCount);
    break;
  case FIFO_OUT_STATUS_REG: // FIFO Out Status Register
    memcpy(data, &smcPCIState.fifoOutStatusReg, byteCount);
    break;
  case FIFO_OUT_DATA_REG: // FIFO Data Out Register
    // Copy the data to our input buffer.
    memcpy(data, &smcCoreState->fifoDataBuffer[smcCoreState->fifoBufferPos],
           byteCount);
    smcCoreState->fifoBufferPos += 4;
    break;
  default:
    LOG_ERROR(SMC, "Unknown register being read, offset {:#x}", static_cast<u16>(regOffset));
    break;
  }
  mutex.unlock();
}

// PCI Config Read
void Xe::PCIDev::SMC::SMCCore::ConfigRead(u64 readAddress, u64 *data, u8 byteCount) {
  LOG_INFO(SMC, "ConfigRead: Address = {:#x}, ByteCount = {:#x}.", readAddress, byteCount);
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], byteCount);
}

// PCI Write
void Xe::PCIDev::SMC::SMCCore::Write(u64 writeAddress, u64 data, u8 byteCount) {
  std::lock_guard lck(mutex);
  const u8 regOffset = static_cast<u8>(writeAddress);

  mutex.lock();
  switch (regOffset) {
  case UART_CONFIG_REG: // UART Config Register
    memcpy(&smcPCIState.uartConfigReg, &data, byteCount);
    // Check if UART is already initialized.
    if (!smcCoreState->uartInitialized && smcCoreState->uartPresent) {
      // Initialize UART.
      setupUART(static_cast<u32>(data));
    }
    break;
  case UART_BYTE_IN_REG: // UART Data In Register
    memcpy(&smcPCIState.uartInReg, &data, byteCount);
#if defined(_WIN32) && !defined(SOCKET_UART)
    // Write the data out.
    smcCoreState->retVal =
        WriteFile(smcCoreState->comPortHandle, &data, 1,
                  &smcCoreState->currentBytesWrittenCount, nullptr);
#endif // _WIN32
    if (smcCoreState->uartBackup) {
      {
        // We love mutexes
        std::lock_guard<std::mutex> lock(smcCoreState->uartMutex);
        smcCoreState->uartTxBuffer.push(data);
        smcCoreState->uartConditionVar.notify_one();
      }
      smcCoreState->retVal = true;
    }
    break;
  case SMI_INT_STATUS_REG: // SMI INT Status Register
    memcpy(&smcPCIState.smiIntPendingReg, &data, byteCount);
    break;
  case SMI_INT_ACK_REG: // SMI INT ACK Register
    memcpy(&smcPCIState.smiIntAckReg, &data, byteCount);
    break;
  case SMI_INT_ENABLED_REG: // SMI INT Enabled Register
    memcpy(&smcPCIState.smiIntEnabledReg, &data, byteCount);
    break;
  case CLCK_INT_ENABLED_REG: // Clock INT Enabled Register
    memcpy(&smcPCIState.clockIntEnabledReg, &data, byteCount);
    break;
  case CLCK_INT_STATUS_REG: // Clock INT Status Register
    memcpy(&smcPCIState.clockIntStatusReg, &data, byteCount);
    break;
  case FIFO_IN_STATUS_REG: // FIFO In Status Register
    smcPCIState.fifoInStatusReg = static_cast<u32>(data);
    if (data == FIFO_STATUS_READY) { // We're about to receive a message.
      // Reset our input buffer and buffer pointer.
      memset(&smcCoreState->fifoDataBuffer, 0, 16);
      smcCoreState->fifoBufferPos = 0;
    }
    break;
  case FIFO_OUT_STATUS_REG: // FIFO Out Status Register
    smcPCIState.fifoOutStatusReg = static_cast<u32>(data);
    // We're about to send a reply.
    if (data == FIFO_STATUS_READY) {
      // Reset our FIFO buffer pointer.
      smcCoreState->fifoBufferPos = 0;
    }
    break;
  case FIFO_IN_DATA_REG: // FIFO Data In Register
    // Copy the data to our input buffer at current position and increse buffer
    // pointer position.
    memcpy(&smcCoreState->fifoDataBuffer[smcCoreState->fifoBufferPos], &data,
           byteCount);
    smcCoreState->fifoBufferPos += 4;
    break;
  default:
    LOG_ERROR(SMC, "Unknown register being written, offset {:#x}, data {:#x}", 
        static_cast<u16>(regOffset), data);
    break;
  }
  mutex.unlock();
}

// PCI Config Write
void Xe::PCIDev::SMC::SMCCore::ConfigWrite(u64 writeAddress, u64 data, u8 byteCount) {
  LOG_INFO(SMC, "ConfigWrite: Address = {:#x}, Data = {:#x}, ByteCount = {:#x}.", writeAddress, data, byteCount);

  // Check if we're being scanned.
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (data == 0xFFFFFFFF) { // PCI BAR Size discovery.
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          data &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        data &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address.
      data = 0; // Register not implemented.
    }
  }
  
  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &data, byteCount);
}

// Setups the UART Communication at a given configuration.
void Xe::PCIDev::SMC::SMCCore::setupUART(u32 uartConfig) {
#ifdef SOCKET_UART
  smcCoreState->uartBackup = true;
  smcCoreState->socketCreated = true;
  int port = 7000;
  constexpr const char* ip = "127.0.0.1";
  //constexpr const char* ip = "10.0.0.201";

  smcCoreState->sockAddr.sin_family = AF_INET;
  smcCoreState->sockAddr.sin_port = htons(port);
  smcCoreState->sockAddr.sin_addr.s_addr = inet_addr(ip);
  smcCoreState->socketCreated = true;
#ifdef _WIN32
  int start = WSAStartup(MAKEWORD(2, 2), &smcCoreState->wsaData);
  if (start != 0) {
    LOG_CRITICAL(SMC, "SOCKET_UART failed! WSAStartup returned a non-zero value. Error: {}", start);
    smcCoreState->socketCreated = false;
    SYSTEM_PAUSE();
  }
#endif // _WIN32
  smcCoreState->sockHandle = socket(AF_INET, SOCK_STREAM, 0);
  int sockert_connect = connect(smcCoreState->sockHandle, (struct sockaddr*)&smcCoreState->sockAddr, sizeof(smcCoreState->sockAddr));
  if (sockert_connect != 0) {
    LOG_CRITICAL(SMC, "SOCKET_UART failed! Failed to connect to socket. Error: {}", Base::GetLastErrorMsg());
    smcCoreState->socketCreated = false;
    SYSTEM_PAUSE();
  }
  if (!smcCoreState->socketCreated) {
    sockert_connect = connect(smcCoreState->sockHandle, (struct sockaddr*)&smcCoreState->sockAddr, sizeof(smcCoreState->sockAddr));
    if (sockert_connect != 0) {
      LOG_CRITICAL(SMC, "SOCKET_UART failed! Failed to connect to socket. (x2) Error: {}", Base::GetLastErrorMsg());
      socketclose(smcCoreState->sockHandle);
      SYSTEM_PAUSE();
    } else {
      smcCoreState->socketCreated = true;
    }
  }
#else
#ifdef _WIN32
  // Windows Init Code.
  LOG_INFO(SMC, "Initializing UART:");

  //  Initialize the DCB structure.
  SecureZeroMemory(&smcCoreState->comPortDCB, sizeof(DCB));
  smcCoreState->comPortDCB.DCBlength = sizeof(DCB);

  switch (uartConfig) {
  case 0x1e6:
    LOG_INFO(SMC, " * BaudRate: 115200bps, DataSize: 8, Parity: N, StopBits: 1.");
    smcCoreState->comPortDCB.BaudRate = CBR_115200;
    smcCoreState->comPortDCB.ByteSize = 8;
    smcCoreState->comPortDCB.Parity = NOPARITY;
    smcCoreState->comPortDCB.StopBits = ONESTOPBIT;
    break;
  case 0x1bb2:
    LOG_INFO(SMC, " * BaudRate: 38400bps, DataSize: 8, Parity: N, StopBits: 1.");
    smcCoreState->comPortDCB.BaudRate = CBR_38400;
    smcCoreState->comPortDCB.ByteSize = 8;
    smcCoreState->comPortDCB.Parity = NOPARITY;
    smcCoreState->comPortDCB.StopBits = ONESTOPBIT;
    break;
  case 0x163:
    LOG_INFO(SMC, " * BaudRate: 19200bps, DataSize: 8, Parity: N, StopBits: 1.");
    smcCoreState->comPortDCB.BaudRate = CBR_19200;
    smcCoreState->comPortDCB.ByteSize = 8;
    smcCoreState->comPortDCB.Parity = NOPARITY;
    smcCoreState->comPortDCB.StopBits = ONESTOPBIT;
    break;
  default:
    LOG_WARNING(SMC, "SMCCore: Unknown UART config being set: ConfigValue = {:#x}", uartConfig);
    break;
  }

  // Open COM# port using the CreateFile function.
  smcCoreState->comPortHandle =
      CreateFileA(smcCoreState->currentCOMPort, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                 OPEN_EXISTING, 0, nullptr);

  bool useBackup = false;
  if (smcCoreState->comPortHandle == INVALID_HANDLE_VALUE) {
    LOG_ERROR(SMC, "CreateFile failed with error {:#x}. Make sure the Selected COM Port is avaliable "
        "in your system.", GetLastError());
    smcCoreState->uartPresent = false;
    if (!smcCoreState->uartBackup) {
      printf("[SMC] <Info> Use backup UART system? (Y/N) ");
      char opt = std::tolower(getchar());
      smcCoreState->uartBackup = opt == 'y';     
      if (!smcCoreState->uartBackup) {
        return;
      }
    }
    else {
      LOG_INFO(SMC, "Using backup UART. Logging UART to Console");
    }
    useBackup = true;
  }

  // Set The COM Port State as per config value.
  if (!useBackup) {
    if (!SetCommState(smcCoreState->comPortHandle, &smcCoreState->comPortDCB)) {
      LOG_ERROR(SMC, "UART: SetCommState failed with error {:#x}.", GetLastError());
    }
  }

  // Everything OK.
  smcCoreState->uartInitialized = true;

  if (useBackup) {
    smcCoreState->uartInitialized = false;
  } else {
    smcCoreState->uartBackup = false;
    LOG_INFO(SMC, "UART Initialized Successfully!");
  }

#else
  smcCoreState->uartBackup = true;
  LOG_ERROR(SMC, "UART Initialization is fully supported on this platform! User beware.");
#endif // _WIN32
#endif // SOCKET_UART
  if (smcCoreState->uartBackup && !smcCoreState->uartInitialized) {
#ifdef SOCKET_UART
    smcCoreState->uartThreadRunning = smcCoreState->socketCreated;
#else
    smcCoreState->uartThreadRunning = true;
#endif // SOCKET_UART
    smcCoreState->uartInitialized = true;
    smcCoreState->uartPresent = true;
    uartThread = std::thread(&SMCCore::uartMainThread, this);
    uartThread.detach();
#ifdef SOCKET_UART
    uartSecondaryThread = std::thread(&SMCCore::uartReceiveThread, this);
#endif // SOCKET_UART
  }
}

// UART Thread
void Xe::PCIDev::SMC::SMCCore::uartMainThread() {
#ifdef SOCKET_UART
  smcCoreState->uartInitialized = smcCoreState->socketCreated;
#else
  smcCoreState->uartInitialized = true;
#endif // SOCKET_UART
  LOG_INFO(SMC, "Backup UART Initialized Successfully!");
  while (smcCoreState->uartThreadRunning) {
    std::unique_lock<std::mutex> lock(smcCoreState->uartMutex);
    if (!smcCoreState->uartTxBuffer.empty()) {
#ifdef SOCKET_UART
      char c = smcCoreState->uartTxBuffer.front();
      send(smcCoreState->sockHandle, &c, 1, 0);
#else
      printf("%c", smcCoreState->uartTxBuffer.front());
#endif // SOCKET_UART
      smcCoreState->uartTxBuffer.pop();
    }
    lock.unlock();
  }
}

#ifdef SOCKET_UART
// UART Receive Thread
void Xe::PCIDev::SMC::SMCCore::uartReceiveThread() {
  while (smcCoreState->uartThreadRunning) {
    std::unique_lock<std::mutex> lock(smcCoreState->uartMutex);
    char c = -1;
    u64 bytesReceived = 0;
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(smcCoreState->sockHandle, FIONBIO, &mode);
    bytesReceived = recv(smcCoreState->sockHandle, &c, 1, 0);
#else
    bytesReceived = recv(smcCoreState->sockHandle, &c, 1, 0 | MSG_DONTWAIT);
#endif
    if (c != -1 && bytesReceived != 0) {
      smcCoreState->uartRxBuffer.push(c);
    }
    lock.unlock();
  }
}
#endif

// SMC Main Thread
void Xe::PCIDev::SMC::SMCCore::smcMainThread() {
  LOG_INFO(SMC, "Entered main thread.");
  // Set FIFO_IN_STATUS_REG to FIFO_STATUS_READY to indicate we are ready to
  // receive a message.
  smcPCIState.fifoInStatusReg = FIFO_STATUS_READY;

  // Timer for measuring elapsed time since last Clock Interrupt.
  std::chrono::steady_clock::time_point timerStart =
      std::chrono::steady_clock::now();

  while (smcThreadRunning) {
    // The System Management Controller (SMC) does the following:
    // * Communicates over a FIFO Queue with the kernel to execute commands and
    // provide system info.
    // * Does the UART/Serial communication between the console and remote
    // Serial Device/PC.
    // * Ticks the clock and sends an interrupt (PRIO_CLOCK) every x
    // milliseconds.

    // Core State (PowerOn Cause, SMC Ver, FAN Speed, Temps, etc...) should be
    // already set.

    /*
            1. FIFO communication.
    */

    // This is done in simple steps:

    /* Message Write (System -> SMC) */

    // 1. System reads FIFO_IN_STATUS_REG to check wheter the SMC is ready to
    // receive a command.
    // 2. If the status is FIFO_STATUS_READY (0x4), the System proceeds, else it
    // loops until the SMC Input Status Register is set to FIFO_STATUS_READY.
    // 3. System then does a write to FIFO_IN_STATUS_REG setting it to
    // FIFO_STATUS_READY. This signals the SMC that a new message/command is
    // about to receive.
    // 4. System does 4 32 Bit writes to FIFO_IN_DATA_REG, this is our 16 Bytes
    // message.
    // 5. System then does a write to FIFO_IN_STATUS_REG setting it to
    // FIFO_STATUS_BUSY. This Signals the SMC that the message is transmitted
    // and that it should start message processing.
    // 6. If SMM (System Management Mode) interrupts are enabled, the SMC
    // changes the SMI_INT_PENDING_REG to SMI_INT_PENDING and issues one
    // signaling the System it should read the message. It also sets the
    // FIFO_OUT_STATUS_REG to FIFO_STATUS_READY.

    /* Message Read (SMC -> System) */

    // Reads Proceed as following:
    // A. Asynchronous Mode (Interrupts Enabled):
    // 1. If an interrupt was issued (Asynchronous Mode), System reads
    // SMI_INT_STATUS_REG to check wheter an interrupt is
    // pending(SMI_INT_PENDING).
    // 2. If SMI_INT_STATUS_REG == SMI_INT_PENDING, then a DPC routine is
    // invoked in order to read the response and the SMI_INT_ACK_REG is set to
    // 0. Else it just continues normal kernel execution.

    // B. Synchronous Mode (Interrupts Disabled):

    // 1. System reads FIFO_OUT_STATUS_REG to check wheter the SMC has finished
    // processing the command. If the status is FIFO_STATUS_READY (0x4), the
    // System proceeds, else it loops until the FIFO_OUT_STATUS_REG is set to
    // FIFO_STATUS_READY.

    // The process afterwards in both cases is the same as when the system does
    // a command write. The diffrence resides on the Registers being used, using
    // FIFO_OUT_STATUS_REG instead of FIFO_IN_STATUS_REG and FIFO_OUT_DATA_REG
    // instead of FIFO_IN_DATA_REG.

    // Check wheter we've received a command. If so, process it.
    // Software sets FIFO_IN_STATUS_REG to FIFO_STATUS_BUSY after it has
    // finished sending a command.
    if (smcPCIState.fifoInStatusReg == FIFO_STATUS_BUSY) {
      // This is set first as software waits for this register to become Ready
      // in order to read a reply. Set FIFO_OUT_STATUS_REG to FIFO_STATUS_BUSY
      smcPCIState.fifoOutStatusReg = FIFO_STATUS_BUSY;

      // Set FIFO_IN_STATUS_REG to FIFO_STATUS_READY
      smcPCIState.fifoInStatusReg = FIFO_STATUS_READY;

      // Some commands does'nt have responses/interrupts.
      bool noResponse = false;

      // Note that the first byte in the response is always Command ID.
      // 
      // Data Buffer[0] is our message ID.
      mutex.lock();
      switch (smcCoreState->fifoDataBuffer[0]) {
      case Xe::PCIDev::SMC::SMC_PWRON_TYPE:
        // Zero out the buffer
        memset(&smcCoreState->fifoDataBuffer, 0, 16);
        smcCoreState->fifoDataBuffer[0] = SMC_PWRON_TYPE;
        smcCoreState->fifoDataBuffer[1] = smcCoreState->currPowerOnReas;
        break;
      case Xe::PCIDev::SMC::SMC_QUERY_RTC:
        // Zero out the buffer
        memset(&smcCoreState->fifoDataBuffer, 0, 16);
        smcCoreState->fifoDataBuffer[0] = SMC_QUERY_RTC;
        smcCoreState->fifoDataBuffer[1] = 0;
        break;
      case Xe::PCIDev::SMC::SMC_QUERY_TEMP_SENS:
        smcCoreState->fifoDataBuffer[0] = SMC_QUERY_TEMP_SENS;
        smcCoreState->fifoDataBuffer[1] = 0x3C;
        LOG_WARNING(SMC, "SMC_FIFO_CMD: SMC_QUERY_TEMP_SENS, returning 3C");
        break;
      case Xe::PCIDev::SMC::SMC_QUERY_TRAY_STATE:
        smcCoreState->fifoDataBuffer[0] = SMC_QUERY_TRAY_STATE;
        smcCoreState->fifoDataBuffer[1] = smcCoreState->currTrayState;
        break;
      case Xe::PCIDev::SMC::SMC_QUERY_AVPACK:
        smcCoreState->fifoDataBuffer[0] = SMC_QUERY_AVPACK;
        smcCoreState->fifoDataBuffer[1] = smcCoreState->currAVPackType;
        break;
      case Xe::PCIDev::SMC::SMC_I2C_READ_WRITE:
        switch (smcCoreState->fifoDataBuffer[1]) {
        case 0x10: // SMC_READ_ANA
          smcCoreState->fifoDataBuffer[0] = SMC_I2C_READ_WRITE;
          smcCoreState->fifoDataBuffer[1] = 0x0;
          smcCoreState->fifoDataBuffer[4] =
              (HANA_State[smcCoreState->fifoDataBuffer[6]] & 0xFF);
          smcCoreState->fifoDataBuffer[5] =
              ((HANA_State[smcCoreState->fifoDataBuffer[6]] >> 8) & 0xFF);
          smcCoreState->fifoDataBuffer[6] =
              ((HANA_State[smcCoreState->fifoDataBuffer[6]] >> 16) & 0xFF);
          smcCoreState->fifoDataBuffer[7] =
              ((HANA_State[smcCoreState->fifoDataBuffer[6]] >> 24) & 0xFF);
          break;
        case 0x60: // SMC_WRITE_ANA
          smcCoreState->fifoDataBuffer[0] = SMC_I2C_READ_WRITE;
          smcCoreState->fifoDataBuffer[1] = 0x0;
          HANA_State[smcCoreState->fifoDataBuffer[6]] =
              smcCoreState->fifoDataBuffer[4] |
              (smcCoreState->fifoDataBuffer[5] << 8) |
              (smcCoreState->fifoDataBuffer[6] << 16) |
              (smcCoreState->fifoDataBuffer[7] << 24);
          break;
        default:
          LOG_WARNING(SMC, "SMC_I2C_READ_WRITE: Unimplemented command {:#x}", smcCoreState->fifoDataBuffer[1]);
          smcCoreState->fifoDataBuffer[0] = SMC_I2C_READ_WRITE;
          smcCoreState->fifoDataBuffer[1] = 0x1; // Set R/W Failed.
        }
        break;
      case Xe::PCIDev::SMC::SMC_QUERY_VERSION:
        smcCoreState->fifoDataBuffer[0] = SMC_QUERY_VERSION;
        smcCoreState->fifoDataBuffer[1] = 0x41;
        smcCoreState->fifoDataBuffer[2] = 0x02;
        smcCoreState->fifoDataBuffer[3] = 0x03;
        break;
      case Xe::PCIDev::SMC::SMC_FIFO_TEST:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_FIFO_TEST");
        break;
      case Xe::PCIDev::SMC::SMC_QUERY_IR_ADDRESS:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_QUERY_IR_ADDRESS");
        break;
      case Xe::PCIDev::SMC::SMC_QUERY_TILT_SENSOR:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_QUERY_TILT_SENSOR");
        break;
      case Xe::PCIDev::SMC::SMC_READ_82_INT:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_READ_82_INT");
        break;
      case Xe::PCIDev::SMC::SMC_READ_8E_INT:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_READ_8E_INT");
        break;
      case Xe::PCIDev::SMC::SMC_SET_STANDBY:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_STANDBY");
        break;
      case Xe::PCIDev::SMC::SMC_SET_TIME:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_TIME");
        break;
      case Xe::PCIDev::SMC::SMC_SET_FAN_ALGORITHM:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_FAN_ALGORITHM");
        break;
      case Xe::PCIDev::SMC::SMC_SET_FAN_SPEED_CPU:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_FAN_SPEED_CPU");
        break;
      case Xe::PCIDev::SMC::SMC_SET_DVD_TRAY:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_DVD_TRAY");
        break;
      case Xe::PCIDev::SMC::SMC_SET_POWER_LED:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_POWER_LED");
        break;
      case Xe::PCIDev::SMC::SMC_SET_AUDIO_MUTE:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_AUDIO_MUTE");
        break;
      case Xe::PCIDev::SMC::SMC_ARGON_RELATED:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_ARGON_RELATED");
        break;
      case Xe::PCIDev::SMC::SMC_SET_FAN_SPEED_GPU:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_FAN_SPEED_GPU");
        break;
      case Xe::PCIDev::SMC::SMC_SET_IR_ADDRESS:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_IR_ADDRESS");
        break;
      case Xe::PCIDev::SMC::SMC_SET_DVD_TRAY_SECURE:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_DVD_TRAY_SECURE");
        break;
      case Xe::PCIDev::SMC::SMC_SET_FP_LEDS:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_FP_LEDS");
        noResponse = true;
        break;
      case Xe::PCIDev::SMC::SMC_SET_RTC_WAKE:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_RTC_WAKE");
        break;
      case Xe::PCIDev::SMC::SMC_ANA_RELATED:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_ANA_RELATED");
        break;
      case Xe::PCIDev::SMC::SMC_SET_ASYNC_OPERATION:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_ASYNC_OPERATION");
        break;
      case Xe::PCIDev::SMC::SMC_SET_82_INT:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_82_INT");
        break;
      case Xe::PCIDev::SMC::SMC_SET_9F_INT:
        LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD: SMC_SET_9F_INT");
        break;
      default:
        LOG_WARNING(SMC, "Unknown SMC_FIFO_CMD: ID = {:#x}", 
            static_cast<u16>(smcCoreState->fifoDataBuffer[0]));
        break;
      }
      mutex.unlock();

      // Set FIFO_OUT_STATUS_REG to FIFO_STATUS_READY, signaling we're ready to
      // transmit a response.
      smcPCIState.fifoOutStatusReg = FIFO_STATUS_READY;

      // If interrupts are active set Int status and issue one.
      if (smcPCIState.smiIntEnabledReg & SMI_INT_ENABLED && noResponse == false) {
        // Wait a small delay to mimic hardware. This allows code in xboxkrnl.exe such as
        // KeWaitForSingleObject to correctly setup waiting code.
        // This is no longer needed due to mutexes
        mutex.lock();
        smcPCIState.smiIntPendingReg = SMI_INT_PENDING;
        pciBridge->RouteInterrupt(PRIO_SMM);
        mutex.unlock();
      }
    }

    // Check for SMC Clock interrupt register.
    // 
    // Clock Int Enabled.
    if (smcPCIState.clockIntEnabledReg == CLCK_INT_ENABLED) {
      // Clock Interrupt Not Taken.
      if (smcPCIState.clockIntStatusReg == CLCK_INT_READY) {
        mutex.lock();
        smcPCIState.clockIntStatusReg = CLCK_INT_TAKEN;
        pciBridge->RouteInterrupt(PRIO_CLOCK);
        mutex.unlock();
      }
    }
  }
}
