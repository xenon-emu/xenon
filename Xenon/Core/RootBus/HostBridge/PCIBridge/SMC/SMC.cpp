// Copyright 2025 Xenon Emulator Project

#include "SMC.h"

#include "Base/Logging/Log.h"
#include "Base/Error.h"
#include "Base/Thread.h"

#include "HANA_State.h"
#include "SMC_Config.h"

#include "Core/Xe_Main.h"

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
Xe::PCIDev::SMC::SMCCore::SMCCore(const std::string &deviceName, u64 size,
  PCIBridge *parentPCIBridge, SMC_CORE_STATE *newSMCCoreState) :
  PCIDevice(deviceName, size) {
  LOG_INFO(SMC, "Core: Initializing...");

  // Assign our parent PCI Bus pointer
  pciBridge = parentPCIBridge;

  // Assign our core sate, this is already filled with config data regarding
  // AVPACK, PWRON Reason and TrayState
  smcCoreState = newSMCCoreState;

  // Create a new SMC PCI State
  memset(&smcPCIState, 0, sizeof(smcPCIState));

  // Set UART Status to Empty
  smcPCIState.uartStatusReg = UART_STATUS_EMPTY;

  // Set PCI Config Space registers
  memcpy(pciConfigSpace.data, smcConfigSpaceMap, sizeof(smcConfigSpaceMap));

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x100; // BAR0

  // Create UART handle
  bool socket = smcCoreState->currentUARTSytem == "vcom" ? false : true;
  if (socket) {
    smcCoreState->uartHandle = std::make_unique<HW_UART_SOCK>();
  }
  else {
#ifdef _WIN32
    smcCoreState->uartHandle = std::make_unique<HW_UART_VCOM>();
#else
    LOG_CRITICAL(UART, "Invalid UART type! Defaulting to print");
    smcCoreState->uartHandle = std::make_unique<HW_UART_SOCK>();
#endif
  }
  smcCoreState->uartHandle->uartPresent = true;

  // Enter main execution thread.
  smcThread = std::thread(&SMCCore::smcMainThread, this);
}

// Class Destructor.
Xe::PCIDev::SMC::SMCCore::~SMCCore() {
  LOG_INFO(SMC, "Core: Exiting.");
  smcThreadRunning = false;
  if (smcThread.joinable())
    smcThread.join();
  smcCoreState->uartHandle->Shutdown();
  smcCoreState->uartHandle.reset();
}

// PCI Read
void Xe::PCIDev::SMC::SMCCore::Read(u64 readAddress, u8 *data, u64 size) {
  const u8 regOffset = static_cast<u8>(readAddress);

  mutex.lock();
  switch (regOffset) {
  case UART_CONFIG_REG: // UART Config Register
    memcpy(data, &smcPCIState.uartConfigReg, size);
    break;
  case UART_BYTE_OUT_REG: // UART Data Out Register
    smcPCIState.uartOutReg = smcCoreState->uartHandle->Read();
    if (smcCoreState->uartHandle->retVal) {
      memcpy(data, &smcPCIState.uartOutReg, size);
    }
    break;
  case UART_STATUS_REG: // UART Status Register
    // First lets check if the UART has already been setup, if so, proceed to do
    // the TX/RX.
    smcPCIState.uartStatusReg = smcCoreState->uartHandle->ReadStatus();
    // Check if UART is already initialized.
    if (!smcCoreState->uartHandle.get()) {
      // XeLL doesn't initialize UART before sending data trough it. Initialize
      // it first then.
      setupUART(0x1E6); // 115200,8,N,1.
    }
    memcpy(data, &smcPCIState.uartStatusReg, size);
    break;
  case SMI_INT_STATUS_REG: // SMI INT Status Register
    memcpy(data, &smcPCIState.smiIntPendingReg, size);
    break;
  case SMI_INT_ACK_REG: // SMI INT ACK Register
    memcpy(data, &smcPCIState.smiIntAckReg, size);
    break;
  case SMI_INT_ENABLED_REG: // SMI INT Enabled Register
    memcpy(data, &smcPCIState.smiIntEnabledReg, size);
    break;
  case FIFO_IN_STATUS_REG: // FIFO In Status Register
    memcpy(data, &smcPCIState.fifoInStatusReg, size);
    break;
  case FIFO_OUT_STATUS_REG: // FIFO Out Status Register
    memcpy(data, &smcPCIState.fifoOutStatusReg, size);
    break;
  case FIFO_OUT_DATA_REG: // FIFO Data Out Register
    // Copy the data to our input buffer.
    memcpy(data, &smcCoreState->fifoDataBuffer[smcCoreState->fifoBufferPos], size);
    smcCoreState->fifoBufferPos += 4;
    break;
  default:
    LOG_ERROR(SMC, "Unknown register being read, offset {:#x}", static_cast<u16>(regOffset));
    break;
  }
  mutex.unlock();
}

// PCI Config Read
void Xe::PCIDev::SMC::SMCCore::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  LOG_INFO(SMC, "ConfigRead: Address = {:#x}, size = {:#x}.", readAddress, size);
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

// PCI Write
void Xe::PCIDev::SMC::SMCCore::Write(u64 writeAddress, const u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  const u8 regOffset = static_cast<u8>(writeAddress);

  mutex.lock();
  switch (regOffset) {
  case UART_CONFIG_REG: // UART Config Register
    memcpy(&smcPCIState.uartConfigReg, data, size);
    // Check if UART is already initialized.
    if (smcCoreState->uartHandle->SetupNeeded()) {
      u64 tmp = 0;
      memcpy(&tmp, data, size);
      // Initialize UART.
      setupUART(tmp);
    }
    break;
  case UART_BYTE_IN_REG: // UART Data In Register
    memcpy(&smcPCIState.uartInReg, data, size);
    smcCoreState->uartHandle->Write(*data);
    break;
  case SMI_INT_STATUS_REG: // SMI INT Status Register
    memcpy(&smcPCIState.smiIntPendingReg, data, size);
    break;
  case SMI_INT_ACK_REG: // SMI INT ACK Register
    memcpy(&smcPCIState.smiIntAckReg, data, size);
    break;
  case SMI_INT_ENABLED_REG: // SMI INT Enabled Register
    memcpy(&smcPCIState.smiIntEnabledReg, data, size);
    break;
  case CLCK_INT_ENABLED_REG: // Clock INT Enabled Register
    memcpy(&smcPCIState.clockIntEnabledReg, data, size);
    break;
  case CLCK_INT_STATUS_REG: // Clock INT Status Register
    memcpy(&smcPCIState.clockIntStatusReg, data, size);
    break;
  case FIFO_IN_STATUS_REG: // FIFO In Status Register
    memcpy(&smcPCIState.fifoInStatusReg, data, size);
    if (smcPCIState.fifoInStatusReg == FIFO_STATUS_READY) { // We're about to receive a message.
      // Reset our input buffer and buffer pointer.
      memset(smcCoreState->fifoDataBuffer, 0, sizeof(smcCoreState->fifoDataBuffer));
      smcCoreState->fifoBufferPos = 0;
    }
    break;
  case FIFO_OUT_STATUS_REG: // FIFO Out Status Register
    memcpy(&smcPCIState.fifoOutStatusReg, data, size);
    // We're about to send a reply.
    if (smcPCIState.fifoOutStatusReg == FIFO_STATUS_READY) {
      // Reset our FIFO buffer pointer.
      smcCoreState->fifoBufferPos = 0;
    }
    break;
  case FIFO_IN_DATA_REG: // FIFO Data In Register
    // Copy the data to our input buffer at current position and increse buffer
    // pointer position.
    memcpy(&smcCoreState->fifoDataBuffer[smcCoreState->fifoBufferPos], data, size);
    smcCoreState->fifoBufferPos += 4;
    break;
  default:
    u64 tmp = 0;
    memcpy(&tmp, data, size);
    LOG_ERROR(SMC, "Unknown register being written, offset {:#x}, data {:#x}", 
        static_cast<u16>(regOffset), tmp);
    break;
  }
  mutex.unlock();
}

// PCI MemSet
void Xe::PCIDev::SMC::SMCCore::MemSet(u64 writeAddress, s32 data, u64 size) {
  std::lock_guard lck(mutex);
  const u8 regOffset = static_cast<u8>(writeAddress);

  mutex.lock();
  switch (regOffset) {
  case UART_CONFIG_REG: // UART Config Register
    memset(&smcPCIState.uartConfigReg, data, size);
    break;
  case UART_BYTE_IN_REG: // UART Data In Register
    memset(&smcPCIState.uartInReg, data, size);
    break;
  case SMI_INT_STATUS_REG: // SMI INT Status Register
    memset(&smcPCIState.smiIntPendingReg, data, size);
    break;
  case SMI_INT_ACK_REG: // SMI INT ACK Register
    memset(&smcPCIState.smiIntAckReg, data, size);
    break;
  case SMI_INT_ENABLED_REG: // SMI INT Enabled Register
    memset(&smcPCIState.smiIntEnabledReg, data, size);
    break;
  case CLCK_INT_ENABLED_REG: // Clock INT Enabled Register
    memset(&smcPCIState.clockIntEnabledReg, data, size);
    break;
  case CLCK_INT_STATUS_REG: // Clock INT Status Register
    memset(&smcPCIState.clockIntStatusReg, data, size);
    break;
  case FIFO_IN_STATUS_REG: // FIFO In Status Register
    memset(&smcPCIState.fifoInStatusReg, data, size);
    if (smcPCIState.fifoInStatusReg == FIFO_STATUS_READY) { // We're about to receive a message.
      // Reset our input buffer and buffer pointer.
      memset(&smcCoreState->fifoDataBuffer, 0, 16);
      smcCoreState->fifoBufferPos = 0;
    }
    break;
  case FIFO_OUT_STATUS_REG: // FIFO Out Status Register
    memset(&smcPCIState.fifoOutStatusReg, data, size);
    // We're about to send a reply.
    if (smcPCIState.fifoOutStatusReg == FIFO_STATUS_READY) {
      // Reset our FIFO buffer pointer.
      smcCoreState->fifoBufferPos = 0;
    }
    break;
  case FIFO_IN_DATA_REG: // FIFO Data In Register
    // Copy the data to our input buffer at current position and increse buffer
    // pointer position.
    memset(&smcCoreState->fifoDataBuffer[smcCoreState->fifoBufferPos], data, size);
    smcCoreState->fifoBufferPos += 4;
    break;
  default:
    u64 tmp = 0;
    memset(&tmp, data, size);
    LOG_ERROR(SMC, "Unknown register being written, offset {:#x}, data {:#x}", 
        static_cast<u16>(regOffset), tmp);
    break;
  }
  mutex.unlock();
}

// PCI Config Write
void Xe::PCIDev::SMC::SMCCore::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {  
  // Check if we're being scanned
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  LOG_DEBUG(SMC, "ConfigWrite: Address = {:#x}, Data = {:#x}, size = {:#x}.", writeAddress, tmp, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          tmp &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        tmp &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address
      tmp = 0; // Register not implemented
    }
  }
  
  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
}

// Setups the UART Communication at a given configuration.
void Xe::PCIDev::SMC::SMCCore::setupUART(u32 uartConfig) {
  LOG_INFO(UART, "Initializing...");
  bool socket = smcCoreState->currentUARTSytem == "vcom" ? false : true;
  if (socket) {
    HW_UART_SOCK_CONFIG *config = new HW_UART_SOCK_CONFIG();
    strncpy(config->ip, smcCoreState->socketIp.c_str(), sizeof(config->ip));
    config->port = smcCoreState->socketPort;
    config->usePrint = smcCoreState->currentUARTSytem == "print";
    smcCoreState->uartHandle->Init(config);
  } else {
#ifdef _WIN32
    HW_UART_VCOM_CONFIG *config = new HW_UART_VCOM_CONFIG();
    strncpy(config->selectedComPort, smcCoreState->currentCOMPort.c_str(), sizeof(config->selectedComPort));
    config->config = uartConfig;
    smcCoreState->uartHandle->Init(config);
#else
    LOG_CRITICAL(UART, "Invalid UART type! Defaulting to print");
    HW_UART_SOCK_CONFIG *config = new HW_UART_SOCK_CONFIG();
    strncpy(config->ip, smcCoreState->socketIp.c_str(), sizeof(config->ip));
    config->port = smcCoreState->socketPort;
    config->usePrint = true;
    smcCoreState->uartHandle->Init(config);
#endif
  }
}

// SMC Main Thread
void Xe::PCIDev::SMC::SMCCore::smcMainThread() {
  Base::SetCurrentThreadName("[Xe] SMC");
  // Set FIFO_IN_STATUS_REG to FIFO_STATUS_READY to indicate we are ready to
  // receive a message.
  smcPCIState.fifoInStatusReg = FIFO_STATUS_READY;

  // Timer for measuring elapsed time since last Clock Interrupt.
  std::chrono::steady_clock::time_point timerStart =
      std::chrono::steady_clock::now();

  while (smcThreadRunning) {
    MICROPROFILE_SCOPEI("[Xe::PCI]", "SMC::Loop", MP_AUTO);
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
      if (false) {
        std::stringstream ss{};
        ss << std::endl;
        for (u64 i = 0; i != sizeof(smcCoreState->fifoDataBuffer); i += 4) {
          for (u64 j = 0; j != 4; ++j) {
            ss << fmt::format(" 0x{:02X}", static_cast<u16>(smcCoreState->fifoDataBuffer[i+j]));
          }
          if (i != (sizeof(smcCoreState->fifoDataBuffer) - 4))
            ss << std::endl;
        }
        LOG_INFO(SMC, "FIFO Data:{}", ss.str());
      }
      switch (smcCoreState->fifoDataBuffer[0]) {
      case Xe::PCIDev::SMC::SMC_PWRON_TYPE:
        // Zero out the buffer
        memset(&smcCoreState->fifoDataBuffer, 0, 16);
        smcCoreState->fifoDataBuffer[0] = SMC_PWRON_TYPE;
        smcCoreState->fifoDataBuffer[1] = smcCoreState->currPowerOnReason;
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
        case 0x3: // SMC_I2C_DDC_LOCK
          LOG_INFO(SMC, "[I2C] Requested DDC Lock.");
          smcCoreState->fifoDataBuffer[0] = SMC_I2C_READ_WRITE;
          smcCoreState->fifoDataBuffer[1] = 0; // Lock Succeeded.
          break;
        case 0x5: // SMC_I2C_DDC_UNLOCK
          LOG_INFO(SMC, "[I2C] Requested DDC Unlock.");
          smcCoreState->fifoDataBuffer[0] = SMC_I2C_READ_WRITE;
          smcCoreState->fifoDataBuffer[1] = 0; // Unlock Succeeded.
          break;
        case 0x10: // SMC_READ_ANA
          smcCoreState->fifoDataBuffer[0] = SMC_I2C_READ_WRITE;
          smcCoreState->fifoDataBuffer[1] = 0x0;
          smcCoreState->fifoDataBuffer[3] =
              (HANA_State[smcCoreState->fifoDataBuffer[6]] & 0xFF);
          smcCoreState->fifoDataBuffer[4] =
              ((HANA_State[smcCoreState->fifoDataBuffer[6]] >> 8) & 0xFF);
          smcCoreState->fifoDataBuffer[5] =
              ((HANA_State[smcCoreState->fifoDataBuffer[6]] >> 16) & 0xFF);
          smcCoreState->fifoDataBuffer[6] =
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
        smcCoreState->fifoDataBuffer[0] = SMC_SET_STANDBY;
        // TODO: Fix other HAL types
        if (smcCoreState->fifoDataBuffer[1] == 0x01) {
          LOG_INFO(SMC, "[Standby] Requested shutdown");
          Xe_Main->shutdown();
        }
        else if (smcCoreState->fifoDataBuffer[1] == 0x04) {
          LOG_INFO(SMC, "[Standby] Requested reboot");
          // Note: Real hardware only respects 0x30, but for automated testing, we will allow anything
          mutex.unlock();
          Xe_Main->reboot(static_cast<Xe::PCIDev::SMC::SMC_PWR_REASON>(smcCoreState->fifoDataBuffer[2]));
          mutex.lock();
        } else {
          LOG_WARNING(SMC, "Unimplemented SMC_FIFO_CMD Subtype in SMC_SET_STANDBY: 0x{:02X}",
            static_cast<u16>(smcCoreState->fifoDataBuffer[1]));
        }
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

    // Measure elapsed time.
    std::chrono::steady_clock::time_point timerNow =
      std::chrono::steady_clock::now();

    // Check for SMC Clock interrupt register.
    // 
    // Clock Int Enabled.
    if (smcPCIState.clockIntEnabledReg == CLCK_INT_ENABLED) {
      // Clock Interrupt Not Taken.
      if (smcPCIState.clockIntStatusReg == CLCK_INT_READY) {
        // Wait X time before next clock interrupt. TODO: Find the correct
        // delay.
        if (timerNow >= timerStart + 500ms) {
          // Update internal timer.
          timerStart = std::chrono::steady_clock::now();
          mutex.lock();
          smcPCIState.clockIntStatusReg = CLCK_INT_TAKEN;
          pciBridge->RouteInterrupt(PRIO_CLOCK);
          mutex.unlock();
        }
      }
    }
  }
}
