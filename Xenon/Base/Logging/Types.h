// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"

namespace Base::Log {

/// Specifies the severity or level of detail of the log message.
enum class Level : u8 {
  Trace,      ///< Extremely detailed and repetitive debugging information that is likely to pollute logs.
  Debug,      ///< Less detailed debugging information.
  Info,       ///< Status information from important points during execution.
  Warning,    ///< Minor or potential problems found during execution of a task.
  Error,      ///< Major problems found during execution of a task that prevent it from being completed.
  Critical,   ///< Major problems during execution that threaten the stability of the entire application.
  Guest,      ///< Output from the guest system.
  Count       ///< Total number of logging levels.
};

/*
 * Specifies the sub-system that generated the log message.
 *
 * @note If you add a new entry here, also add a corresponding one to `ALL_LOG_CLASSES` in
 * filter.cpp.
 */
enum class Class : u8 {
  Log,                    // Messages about the log system itself.
  Base,                   // System base routines: FS, logging, etc.
  Base_Filesystem,        // Filesystem Messages.
  Config,                 // Emulator configuration (including commandline).
  Debug,                  // Debugging tools.
  System,                 // Base System messages.
  Xenon,                  // Xenon CPU messages.
  Xenon_IIC,              // Xenon Integrated Interrupt Controller.
  Xenon_MMU,              // Xenon MMU debugging messages.
  Xenon_PostBus,          // Xenon Post Bus output messages.
  Xenos,                  // Xenos GPU messages.
  RootBus,                // RootBus messages. Missing/unmapped memory, etc...
  HostBridge,             // Hostbridge messages.
  PCIBridge,              // PCI Bridge messages: Connected devices, failed lookups, etc..
  AudioController,        // Several devices on the system:
  EHCI,
  OHCI,
  ETH,
  HDD,
  ODD,
  SFCX,
  XMA,
  SMC,                    // System Management Controller messages.
  UART,                   // UART output.
  DebugPrint,             // Guest hardware output. Xbox Kernel Debug Prints.
  Count                   // Total number of logging classes.
};

} // namespace Base::Log
