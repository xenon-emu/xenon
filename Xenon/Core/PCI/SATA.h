/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

//
// ATAPI (ODD) Registers Offsets
//

// Communication with disk drive controllers is achieved via I/O registers.
// Registers and their offsets relative to the base
// address of command block registers and the base address of control block
// registers

// Registers Offsets from Command Block

// Data Reg (Read/Write)
#define ATAPI_REG_DATA 0x0
// Error Reg (Read)
#define ATAPI_REG_ERROR 0x1
// Features Reg (Write)
#define ATAPI_REG_FEATURES 0x1
// Interrupt Reason Reg (Read)
#define ATAPI_REG_INT_REAS 0x2
// Sector Count Reg (Write)
#define ATAPI_REG_SECTOR_COUNT 0x2
// LBA Low Reg (Read/Write)
#define ATAPI_REG_LBA_LOW 0x3
// Byte Count Low Reg (Read/Write)
#define ATAPI_REG_BYTE_COUNT_LOW 0x4
// Byte Count High Reg (Read/Write)
#define ATAPI_REG_BYTE_COUNT_HIGH 0x5
// Device Reg (Read/Write)
#define ATAPI_REG_DEVICE 0x6
// Status Reg (Read)
#define ATAPI_REG_STATUS 0x7
// Command Reg (Write)
#define ATAPI_REG_COMMAND 0x7

// In the Xenon ATA Controller ALTStatus/DevControl is at 0xA
// Alternate Status Reg (Read)
#define ATAPI_REG_ALTERNATE_STATUS 0xA
// Device Control Reg (Write)
#define ATAPI_REG_DEVICE_CONTROL 0xA

// Registers Offsets from Control Block

// Direct Memory Access Command
#define ATAPI_DMA_REG_COMMAND 0x0
// Direct Memory Access Status
#define ATAPI_DMA_REG_STATUS 0x2
// Direct Memory Access Table Offset
#define ATAPI_DMA_REG_TABLE_OFFSET 0x4

//
//  DMA Registers Bitmasks
//

#define XE_ATAPI_DMA_ACTIVE 0x1
#define XE_ATAPI_DMA_ERR 0x2
#define XE_ATAPI_DMA_INTR 0x4
#define XE_ATAPI_DMA_WR 0x8

//
// ATA (HDD) Registers Offsets
//

// Command block Registers

// Data Reg (Read/Write)
#define ATA_REG_DATA          0x0
// Error Reg (Read)
#define ATA_REG_ERROR         0x1
// Features Reg (Write)
#define ATA_REG_FEATURES      0x1
// Sector Count Reg (Read/Write)
#define ATA_REG_SECTORCOUNT   0x2
// LBA Low Reg (Read/Write)
#define ATA_REG_LBA_LOW       0x3
// LBA Med Reg (Read/Write)
#define ATA_REG_LBA_MED       0x4
// LBA High Reg (Read/Write)
#define ATA_REG_LBA_HI        0x5
// Device Reg (Read/Write)
#define ATA_REG_DEV_SEL       0x6
// Status Reg (Read)
#define ATA_REG_STATUS        0x7
// Command Reg (Write)
#define ATA_REG_CMD           0x7

// Control Block registers

// Data Reg (Read/Write)
#define ATA_REG_ALT_STATUS    0xA
// Data Reg (Read/Write)
#define ATA_REG_DEV_CTRL      0xA

// SStatus, SError, SControl and SActive are also accessible thru bridge registers, this means
// software does not need to access config space for reading them.

#define ATA_REG_SSTATUS   0x10
#define ATA_REG_SERROR    0x14
#define ATA_REG_SCONTROL  0x18
#define ATA_REG_SACTIVE   0x1C // TODO: Verify this.

// Direct Memory Access Command
#define ATA_REG_DMA_COMMAND 0x0
// Direct Memory Access Status
#define ATA_REG_DMA_STATUS 0x2
// Direct Memory Access Table Offset
#define ATA_REG_DMA_TABLE_OFFSET 0x4

// DMA Status
#define XE_ATA_DMA_ACTIVE 0x1
#define XE_ATA_DMA_ERR 0x2
#define XE_ATA_DMA_INTR 0x4
#define XE_ATA_DMA_WR 0x8

// Sector size for ATA disks
#define ATA_SECTOR_SIZE 512

//
// ATA Status Register
//

/*
   This register contains the current status of the drive. If the BSY bit is 0,
   the other bits of the register contain valid information; otherwise the other
   bits do not contain valid information. If this register is read by the host
   computer during a pending interrupt, the interrupt condition is cleared

   Bits 1 & 2 are undefined
   Bit 4 is Command Specific
*/

/*
   Bit 0 (ERR / CHK - Error / Check) is defined as ERR for all commands except
   for the Packet and Service commands, for which this bit is defined as CHK
*/
#define ATA_STATUS_ERR_CHK 0x1
/*
   Bit 3 (DRQ - Data Request) indicates by value 1 that the disk drive is ready
   to transfer data between the host computer and the drive. After the computer
   writes a commmand code to the Command register, the drive sets the BSY bit or
   the DRQ bit to 1 until command completion
*/
#define ATA_STATUS_DRQ 0x08
/*
   Bit 5 (DF - Device Fault) indicates by value 1 that a device fault has been
   detected
*/
#define ATA_STATUS_DF 0x10
/*
   Bit 6 (DRDY - Device Ready) is set to 1 to indicate that the disk drive
   accepts commands. If the DRDY bit is 0, the drive will accept and attempt to
   execute the Device Reset and Execute Device Diagnostic commands. Other
   commands will not be accepted, and the drive will set the ABRT bit in the
   Error register and the ERR/CHK bit in the Status register, before resetting
   the BSY bit to indicate completion of the command
*/
#define ATA_STATUS_DRDY 0x40
/*
   Bit 7 (BSY - Busy) is set to 1 whenever the disk drive has control of the
   Command Block registers. If the BSY bit is 1, a write to any Command Block
   register by the host computer will be ignored by the drive. The BSY bit is
   cleared to 0 by the drive at command completion and after setting the DRQ
   status bit to 1 to indicate the device is ready to transfer data
*/
#define ATA_STATUS_BSY 0x80

//
// ATA Data Register
//

/*
   This is a 32-bit register and is used for reading or writing the data during
   data transfers. This register shall be accessed for data transfers in PIO
   mode only when the DRQ bit of the Status register is set to 1
*/

//
// ATA Error Register
//

/*
   This register contains the status of the last command executed by the disk
   drive or a diagnostic code. At completion of any command except the Execute
   Device Diagnostic and Device Reset commands, the contents of this register
   are valid when the BSY and DRQ bits of the Status register are cleared to 0
   and the ERR/CHK bit in the same register is set to 1. At completion of an
   Execute Device Diagnostic or Device Reset command and after a hardware or
   software reset, this register contains a diagnostic code. Except for bit 2
   (ABRT), the meaning of other bits of the Error register varies depending on
   the command that has been executed
*/

/*
   Bit 2 (ABRT - Command Aborted) indicates by value 1 that the requested
   command has been aborted because the command code or a command parameter is
   invalid, the command is not implemented, or some other error has occurred
*/
#define ATA_ERROR_ABRT 0x4

//
// ATA Device Register
//

/*
   This register is used for selecting the disk drive. The register shall be
   written only when the BSY and DRQ bits of the Status register are both 0. The
   contents of this register are valid only when the BSY bit of the Status
   register is 0. Except the DEV bit, all other bits of this register become a
   command parameter when the command code is written into the Command register

   Bits 3..0 are command specific.
   Bit 7 and bit 5 are undefined.
*/

/*
   Bit 4 (DEV - Device Select) selects by value 0 the drive 0, and by value 1
   the drive 1
*/
#define ATA_DEV_DEV 0x8
/*
   Bit 6 (LBA) selects the sector addressing mode. Some commands require to set
   this bit to 1 to select LBA addressing. If this bit is cleared to 0, the CHS
   addressing is selected
*/
#define ATA_DEV_LBA 0x20

//
// ATA Command Register
//

/*
   This register contains the command code to be sent to the disk drive.
   Command execution begins immediately after the command code is written into
   the Command register. The contents of the Command Block registers become
   parameters of the command when this register is written. Writing this
   register clears any pending interrupt condition
*/

//
// ATA Alternate Status Register
//

/*
   This register contains the same information as the Status register. The only
   difference is that reading the Alternate Status register does not imply an
   interrupt acknowledgement or clearing of the interrupt condition
*/

//
// ATA Device Control Register Register
//

/*
   This register allows the host computer to perform a software reset of the
   disk drives and to enable or disable the assertion of the INTRQ interrupt
   signal by the selected drive. When the Device Control register is written,
   both drives respond to the write regardless of which drive is selected.

   Bit 0 shall be cleared to 0
   Bits 6..3 are reserved
*/

/*
   Bit 1 (nIEN - INTRQ Enable) enables by value 0 the assertion of the INTRQ
   interrupt request signal by the disk drive
*/
#define ATA_DEVICE_CONTROL_NIEN 0x02
/*
   Bit 2 (SRST - Software Reset) is the software reset bit of the disk drives.
   If there are two daisy-chained drives, by setting this bit to 1 both drives
   are reset
*/
#define ATA_DEVICE_CONTROL_SRST 0x04
/*
   Bit 7 (HOB - High Order Byte) is defined only when the 48-bit LBA addressing
   is implemented. If this bit is set to 1, reading of the Features register,
   the Sector Count register, and the LBA address registers is performed from
   the -previous content- location. If the HOB bit is set to 0, reading is
   performed from the -most recently written- location. Writing to any Command
   Block register has the effect of resetting the HOB bit to 0
*/
#define ATA_DEVICE_CONTROL_HOB 0x80

//
// Command Descriptor Block for SCSI Commands
//

// The CDB on the Xenon ODD is 12 bytes in size
#define XE_ATAPI_CDB_SIZE 12

//
// ATA Commands
//

#define ATA_COMMAND_DEVICE_RESET 0x08
#define ATA_COMMAND_READ_SECTORS 0x20
#define ATA_COMMAND_READ_DMA_EXT 0x25
#define ATA_COMMAND_READ_NATIVE_MAX_ADDRESS_EXT 0x27
#define ATA_COMMAND_WRITE_SECTORS 0x30
#define ATA_COMMAND_WRITE_DMA_EXT 0x35
#define ATA_COMMAND_READ_VERIFY_SECTORS 0x40
#define ATA_COMMAND_READ_VERIFY_SECTORS_EXT 0x42
#define ATA_COMMAND_READ_FPDMA_QUEUED 0x60
#define ATA_COMMAND_SET_DEVICE_PARAMETERS 0x91
#define ATA_COMMAND_PACKET 0xA0
#define ATA_COMMAND_IDENTIFY_PACKET_DEVICE 0xA1
#define ATA_COMMAND_READ_MULTIPLE 0xC4
#define ATA_COMMAND_WRITE_MULTIPLE 0xC5
#define ATA_COMMAND_SET_MULTIPLE_MODE 0xC6
#define ATA_COMMAND_READ_DMA 0xC8
#define ATA_COMMAND_WRITE_DMA 0xCA
#define ATA_COMMAND_STANDBY_IMMEDIATE 0xE0
#define ATA_COMMAND_FLUSH_CACHE 0xE7
#define ATA_COMMAND_IDENTIFY_DEVICE 0xEC
#define ATA_COMMAND_SET_FEATURES 0xEF
#define ATA_COMMAND_SECURITY_SET_PASSWORD 0xF1
#define ATA_COMMAND_SECURITY_UNLOCK 0xF2
#define ATA_COMMAND_SECURITY_DISABLE_PASSWORD 0xF6

//
// Set Features Subcommnads list
//

// Note: Adding as per needed.

#define ATA_SF_SUBCOMMAND_SET_TRANSFER_MODE 0x3

//
// IDE feature flags for an ATAPI device
//

#define IDE_FEATURE_DMA 0x01
#define IDE_FEATURE_OVL 0x02

//
// IDE interrupt reason flags for an ATAPI device
//

// The COMMAND/DATA bit shall be cleared to zero if the transfer is data. Otherwise, the COMMAND/DATA bit shall be
// set to one
#define ATA_INTERRUPT_REASON_CD 0x01
// The INPUT/OUTPUT bit shall be cleared to zero if the transfer is to the device. The INPUT/OUTPUT bit shall be set to
// one if the transfer is to the host
#define ATA_INTERRUPT_REASON_IO 0x02

#define ATA_INTERRUPT_REASON_REL 0x04

//
// Data transfer values for an ATAPI device
//

#define ATAPI_CDROM_SECTOR_SIZE 2048

//
// Control and status flags for the DMA interface
//

#define ATAPI_DMA_CONTROL_RUN 0x8000
#define ATAPI_DMA_CONTROL_PAUSE 0x4000
#define ATAPI_DMA_CONTROL_FLUSH 0x2000
#define ATAPI_DMA_CONTROL_WAKE 0x1000
#define ATAPI_DMA_CONTROL_DEAD 0x0800
#define ATAPI_DMA_CONTROL_ACTIVE 0x0400

//
// SCSI Command Descriptor Block Operation codes
//

// 6 Byte 'Standard' CDB
#define SCSIOP_TEST_UNIT_READY  0x00
#define SCSIOP_REQUEST_SENSE    0x03
#define SCSIOP_FORMAT_UNIT      0x04
#define SCSIOP_INQUIRY	        0x12
#define SCSIOP_MODE_SELECT6	    0x15
#define SCSIOP_MODE_SENSE6	    0x1A
#define SCSIOP_START_STOP	      0x1B
#define SCSIOP_TOGGLE_LOCK	    0x1E

// 10 Byte CDB
#define SCSIOP_READ_FMT_CAP	    0x23
#define SCSIOP_READ_CAPACITY	  0x25
#define SCSIOP_READ10	          0x28
#define SCSIOP_SEEK10	          0x2B
#define SCSIOP_ERASE10	        0x2C
#define SCSIOP_WRITE10	        0x2A
#define SCSIOP_VER_WRITE10	    0x2E
#define SCSIOP_VERIFY10	        0x2F
#define SCSIOP_SYNC_CACHE	      0x35
#define SCSIOP_WRITE_BUF	      0x3B
#define SCSIOP_READ_BUF	        0x3C
#define SCSIOP_READ_SUBCH	      0x42
#define SCSIOP_READ_TOC	        0x43
#define SCSIOP_READ_HEADER	    0x44
#define SCSIOP_PLAY_AUDIO10	    0x45
#define SCSIOP_GET_CONFIG	      0x46
#define SCSIOP_PLAY_AUDIOMSF	  0x47
#define SCSIOP_EVENT_INFO	      0x4A
#define SCSIOP_TOGGLE_PAUSE	    0x4B
#define SCSIOP_STOP		          0x4E
#define SCSIOP_READ_INFO	      0x51
#define SCSIOP_READ_TRK_INFO	  0x52
#define SCSIOP_RES_TRACK	      0x53
#define SCSIOP_SEND_OPC	        0x54
#define SCSIOP_MODE_SELECT10    0x55
#define SCSIOP_REPAIR_TRACK	    0x58
#define SCSIOP_MODE_SENSE10	    0x5A
#define SCSIOP_CLOSE_TRACK	    0x5B
#define SCSIOP_READ_BUF_CAP	    0x5C

// 12 Byte CDB
#define SCSIOP_BLANK		        0xA1
#define SCSIOP_SEND_KEY	        0xA3
#define SCSIOP_REPORT_KEY	      0xA4
#define SCSIOP_PLAY_AUDIO12	    0xA5
#define SCSIOP_LOAD_CD	        0xA6
#define SCSIOP_SET_RD_AHEAD	    0xA7
#define SCSIOP_READ12	          0xA8
#define SCSIOP_WRITE12	        0xAA
#define SCSIOP_GET_PERF	        0xAC
#define SCSIOP_READ_DVD_S	      0xAD
#define SCSIOP_SET_STREAM	      0xB6
#define SCSIOP_READ_CD_MSF	    0xB9
#define SCSIOP_SCAN		          0xBA
#define SCSIOP_SET_CD_SPEED	    0xBB
#define SCSIOP_PLAY_CD	        0xBC
#define SCSIOP_MECH_STATUS	    0xBD
#define SCSIOP_READ_CD	        0xBE
#define SCSIOP_SEND_DVD_S	      0xBF

#define XE_MAX_DMA_PRD 16

/*
* Small note: (taken from linux kernel patches for the Xbox360).
* It's completely unknown whether the Xenon Southbridge SATA is really based on SiS technology.
* SCR seem to be SiS-like in PCI Config Space, but that should be verified!
* TODO(bitsh1ft3r): Verify this
*/

#define XE_SIS_PMR_COMBINED   0x30
#define XE_SIS_GENCTL        0x54  // IDE General Control Register
#define XE_SIS_PMR          0x90   // Port Mapping Register
#define XE_SIS_SCR_BASE      0xC0  // Sata0 PHY SCR Registers Base

/*
* Serial ATA provides an additional block of registers to control the interface and to retrieve
* interface state information. There are 16 contiguous registers allocated of which the first five are
* defined and the remaining 11 are reserved for future definition. Table 76 defines the Serial ATA
* Status and Control registers
* The registers start at offset that's specified by the arch
* Registers offsets are SCR_REG_BASE + RegNum * 4
*/

/*
* SStatus register:
* The Serial ATA interface Status register - SStatus - is a 32-bit read-only register that conveys the
* current state of the interface and host adapter. The register conveys the interface state at the
* time it is read and is updated continuously and asynchronously by the host adapter. Writes to the
* register have no effect
*/
#define SCR_STATUS_REG  0       // SCR[0] SStatus register

/*
* SError register:
* The Serial ATA interface Error register - SError - is a 32-bit register that conveys supplemental
* Interface error information to complement the error information available in the Shadow Register
* Block Error register. The register represents all the detected errors accumulated since the last
* time the SError register was cleared (whether recovered by the interface of not). Set bits in the
* error register are explicitly cleared by a write operation to the SError register, or a reset operation.
* The value written to clear set error bits shall have 1's encoded in the bit positions corresponding
* to the bits that are to be cleared. Host software should clear the Interface SError register at
* appropriate checkpoints in order to best isolate error conditions and the commands they impact
*/
#define SCR_ERROR_REG 1       // SCR[1] SError register

/*
* SControl register:
* The Serial ATA interface Control register - SControl - is a 32-bit read-write register that provides
* the interface by which software controls Serial ATA interface capabilities. Writes to the SControl
* register result in an action being taken by the host adapter or interface. Reads from the register
* return the last value written to it
*/
#define SCR_CONTROL_REG 2       // SCR[2] SControl register

/*
* SActive register:
* The SActive register is a 32-bit register that conveys the information returned in the SActive field
* of the Set Device Bits FIS. If NCQ is not supported, then the SActive register does not need to
* be implemented
*/
#define SCR_ACTIVE_REG  3       // SCR[3] SActive register

/*
* SNotification register (Optional):
* The Serial ATA interface notification register - SNotification - is a 32-bit register that conveys the
* devices that have sent the host a Set Device Bits FIS with the Notification bit set, as specified in
* section 10.3.6. When the host receives a Set Device Bits FIS with the Notification bit set to one,
* the host shall set the bit in the SNotification register corresponding to the value of the PM Port
* field in the received FIS. For example, if the PM Port field is set to 7 then the host shall set bit 7
* in the SNotification register to one. After setting the bit in the SNotification register, the host shall
* generate an interrupt if the Interrupt bit is set to one in the FIS and interrupts are enabled
*/
#define SCR_NOTIFICATION_REG  4   // SCR[4] SNotification register

/*
* SCR[5-15] Reserved
*/

//
// SCR Bit definitions
//

/*
*  SStatus:
*/

// DET: The DET value indicates the interface device detection and Phy state
#define SSTATUS_DET_BITS 0xF
// Possible values:
#define SSTATUS_DET_NO_DEVICE_DETECTED  0  // No device detected and Phy communication not established
#define SSTATUS_DET_COM_NOT_ESTABLISHED 1  // Device presence detected but Phy communication not established
#define SSTATUS_DET_COM_ESTABLISHED     3  // Device presence detected and Phy communication established
#define SSTATUS_DET_DISABLED_OR_BIST    4  // Phy in offline mode as a result of the interface being disabled or running in a BIST loopback mode

// SPD: The SPD value indicates the negotiated interface communication speed established
#define SSTATUS_SPD_BITS  0xF0
#define SSTATUS_SPD_SHIFT 4
// Possible values:
#define SSTATUS_SPD_NO_SPEED        0 // No negotiated speed (device not present or communication not established)
#define SSTATUS_SPD_GEN1_COM_SPEED  1 // Generation 1 communication rate negotiated
#define SSTATUS_SPD_GEN2_COM_SPEED  2 // Generation 2 communication rate negotiated

// IPM: The IPM value indicates the current interface power management state
#define SSTATUS_IPM_BITS  0xF00
#define SSTATUS_IPM_SHIFT 8
// Possible values:
#define SSTATUS_IPM_NO_DEVICE                   0 // Device not present or communication not established
#define SSTATUS_IPM_INTERFACE_ACTIVE_STATE      1 // Interface in active state
#define SSTATUS_IPM_INTERFACE_PARTIAL_PM_STATE  2 // Interface in Partial power management state
#define SSTATUS_IPM_INTERFACE_SLUMBER_PM_STATE  6 // Interface in Slumber power management state

/*
*  SError:
*/

// ERR: The ERR field contains error information for use by host software in determining the
// appropriate response to the error condition
#define SERROR_ERR_BITS  0xFFFF
// Possible values:
// [R|R|R|R|E|P|C|T|R|R|R|R|R|R|M|I]

/*
* C Non-recovered persistent communication or data integrity error: A communication error
that was not recovered occurred that is expected to be persistent. Since the error
condition is expected to be persistent the operation need not be retried by host software.
Persistent communications errors may arise from faulty interconnect with the device, from
a device that has been removed or has failed, or a number of other causes

* E Internal error: The host bus adapter experienced an internal error that caused the
operation to fail and may have put the host bus adapter into an error state. Host software
should reset the interface before re-trying the operation. If the condition persists, the host
bus adapter may suffer from a design issue rendering it incompatible with the attached
device

* I Recovered data integrity error: A data integrity error occurred that was recovered by the
interface through a retry operation or other recovery action. This may arise from a noise
burst in the transmission, a voltage supply variation, or from other causes. No action is
required by host software since the operation ultimately succeeded, however, host
software may elect to track such recovered errors in order to gauge overall
communications integrity and potentially step down the negotiated communication speed

* M Recovered communications error: Communications between the device and host was
temporarily lost but was re-established. This may arise from a device temporarily being
removed, from a temporary loss of Phy synchronization, or from other causes and may
be derived from the PHYRDYn signal between the Phy and Link layers. No action is
required by the host software since the operation ultimately succeeded, however, host
software may elect to track such recovered errors in order to gauge overall
communications integrity and potentially step down the negotiated communication speed

* P Protocol error: A violation of the Serial ATA protocol was detected. This may arise from
invalid or poorly formed FISes being received, from invalid state transitions, or from other
causes. Host software should reset the interface and retry the corresponding operation. If
such an error persists, the attached device may have a design issue rendering it
incompatible with the host bus adapter

* R Reserved bit for future use: Shall be cleared to zero

* T Non-recovered transient data integrity error: A data integrity error occurred that was not
recovered by the interface. Since the error condition is not expected to be persistent the
operation should be retried by host software
*/

// DIAG: The DIAG field contains diagnostic error information for use by diagnostic software in
// validating correct operation or isolating failure modes.The field is bit significant as
// defined in the following figure
#define SERROR_DIAG_BITS  0xFFFF0000
#define SERROR_DIAG_SHIFT 16
// Possible values:
// [R|A|X|F|T|S|H|C|D|B|W|I|N]

/*
* A Port Selector presence detected: This bit is set to one when COMWAKE is received while
the host is in state HP2: HR_AwaitCOMINIT. On power-up reset this bit is cleared to
zero. The bit is cleared to zero when the host writes a one to this bit location

* B 10b to 8b Decode error: When set to a one, this bit indicates that one or more 10b to 8b
decoding errors occurred since the bit was last cleared to zero

* C CRC Error: When set to one, this bit indicates that one or more CRC errors occurred with
the Link layer since the bit was last cleared to zero

* D Disparity Error: When set to one, this bit indicates that incorrect disparity was detected
one or more times since the last time the bit was cleared to zero

* F Unrecognized FIS type: When set to one, this bit indicates that since the bit was last
cleared one or more FISes were received by the Transport layer with good CRC, but had
a type field that was not recognized

* I Phy Internal Error: When set to one, this bit indicates that the Phy detected some internal
error since the last time this bit was cleared to zero

* N PHYRDY change: When set to one, this bit indicates that the PHYRDY signal changed
state since the last time this bit was cleared to zero

* H Handshake error: When set to one, this bit indicates that one or more R_ERRP
handshake response was received in response to frame transmission. Such errors may
be the result of a CRC error detected by the recipient, a disparity or 10b/8b decoding
error, or other error condition leading to a negative handshake on a transmitted frame

* R Reserved bit for future use: Shall be cleared to zero

* S Link Sequence Error: When set to one, this bit indicates that one or more Link state
machine error conditions was encountered since the last time this bit was cleared to zero.
The Link layer state machine defines the conditions under which the link layer detects an
erroneous transition

* T Transport state transition error: When set to one, this bit indicates that an error has
occurred in the transition from one state to another within the Transport layer since the
last time this bit was cleared to zero

* W COMWAKE Detected: When set to one this bit indicates that a COMWAKE signal was
detected by the Phy since the last time this bit was cleared to zero

* X Exchanged: When set to one this bit indicates that device presence has changed since
the last time this bit was cleared to zero. The means by which the implementation
determines that the device presence has changed is vendor specific. This bit may be set
to one anytime a Phy reset initialization sequence occurs as determined by reception of
the COMINIT signal whether in response to a new device being inserted, in response to a
COMRESET having been issued, or in response to power-up
*/

/*
*  SControl:
*/

// DET: The DET field controls the host adapter device detection and interface initialization.
#define SCONTROL_DET_BITS  0xF
// Possible values:
#define SCONTROL_DET_NO_DEV   0 // No device detection or initialization action requested
#define SCONTROL_DET_INIT     1 // Perform interface communication initialization sequence to establish
                                // communication.This is functionally equivalent to a hard reset and results in the
                                // interface being reset and communications reinitialized
#define SCONTROL_DET_DISABLE  4 // Disable the Serial ATA interface and put Phy in offline mode

// SPD: The SPD field represents the highest allowed communication speed the interface is
// allowed to negotiate when interface communication speed is established
#define SCONTROL_SPD_BITS   0xF0
#define SCONTROL_SPD_SHIFT  4
// Possible values:
#define SCONTROL_SPD_NO_LIMIT 0 // No speed negotiation restrictions
#define SCONTROL_SPD_GEN1     1 // Limit speed negotiation to a rate not greater than Gen 1 communication rate
#define SCONTROL_SPD_HEN2     2 // Limit speed negotiation to a rate not greater than Gen 2 communication rate

// IPM: The IPM field represents the enabled interface power management states that may be
// invoked via the Serial ATA interface power management capabilities.
#define SCONTROL_IPM_BITS   0xF00
#define SCONTROL_IPM_SHIFT  8
// Possible values:
#define SCONTROL_IPM_NO_RESTRICTION       0 // No interface power management state restrictions
#define SCONTROL_IPM_PARTIAL_PM_DISABLED  1 // Transitions to the Partial power management state disabled
#define SCONTROL_IPM_SLUMBER_PM_DISABLED  2 // Transitions to the Slumber power management state disabled
#define SCONTROL_IPM_ALL_PM_DISABLED      3 // Transitions to both the Partial and Slumber power management states disabled

// The rest of the bits are Power-management related. If required i'll implement them

/*
*  SActive
*/
// This may be unused. Leaving unimplemented as of now

/*
*  SNotification
*/
// This may be unused. Same as above

// SCSI Status Codes
// The Status is a single byte returned by some SCSI commands
// Only nine status codes are defined under the SCSI-2 specification. All
// others are reserved
#define SCSI_STATUS_GOOD  0x00
#define SCSI_STATUS_CHECK_CONDITION  0x01
#define SCSI_STATUS_CONDITION_MET  0x02
#define SCSI_STATUS_BUSY  0x04
#define SCSI_STATUS_INTERMEDIATE  0x08
#define SCSI_STATUS_INTERMEDIATE_CONDITION_MET  0x0A
#define SCSI_STATUS_RESERVATION_CONFLICT  0x0C
#define SCSI_STATUS_COMMAND_TERMINATED  0x11
#define SCSI_STATUS_QUEUE_FULL  0x14
