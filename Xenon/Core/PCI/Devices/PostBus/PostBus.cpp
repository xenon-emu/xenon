/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Logging/Log.h"

#include "PostBus.h"

void Xe::XCPU::POSTBUS::POST(u64 postCode) {
  /* 1BL */
  if (postCode >= 0x10 && postCode <= 0x1E) {
    switch (postCode) {
    case 0x10:
      LOG_XBOX(Xenon_PostBus, "1BL started.");
      break;
    case 0x11:
      LOG_XBOX(Xenon_PostBus, "FSB_CONFIG_PHY_CONTROL - Execute FSB function1.");
      break;
    case 0x12:
      LOG_XBOX(Xenon_PostBus, "FSB_CONFIG_RX_STATE - Execute FSB function2");
      break;
    case 0x13:
      LOG_XBOX(Xenon_PostBus, "FSB_CONFIG_TX_STATE - Execute FSB function3");
      break;
    case 0x14:
      LOG_XBOX(Xenon_PostBus, "FSB_CONFIG_TX_CREDITS - Execute FSB function4");
      break;
    case 0x15:
      LOG_XBOX(Xenon_PostBus, "FETCH_OFFSET - Verify CB offset");
      break;
    case 0x16:
      LOG_XBOX(Xenon_PostBus, "FETCH_HEADER - Copy CB header from NAND");
      break;
    case 0x17:
      LOG_XBOX(Xenon_PostBus, "VERIFY_HEADER - Verify CB header");
      break;
    case 0x18:
      LOG_XBOX(Xenon_PostBus, "FETCH_CONTENTS - Copy CB into protected SRAM");
      break;
    case 0x19:
      LOG_XBOX(Xenon_PostBus, "HMACSHA_COMPUTE - Generate CB HMAC key");
      break;
    case 0x1A:
      LOG_XBOX(Xenon_PostBus, "RC4_INITIALIZE - Initialize CB RC4 decryption key");
      break;
    case 0x1B:
      LOG_XBOX(Xenon_PostBus, "RC4_DECRYPT - RC4 decrypt CB");
      break;
    case 0x1C:
      LOG_XBOX(Xenon_PostBus, "SHA_COMPUTE - Generate hash of CB for verification");
      break;
    case 0x1D:
      LOG_XBOX(Xenon_PostBus, "SIG_VERIFY - RSA signature check of CB hash");
      break;
    case 0x1E:
      LOG_XBOX(Xenon_PostBus, "BRANCH - Jump to CB");
      break;
    }
  }
  /* 1BL PANICS */
  else if (postCode >= 0x81 && postCode <= 0x98) {
    switch (postCode) {
      /* 1BL PANIC*/
    case 0x81:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - MACHINE_CHECK");
      break;
    case 0x82:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - DATA_STORAGE");
      break;
    case 0x83:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - DATA_SEGMENT");
      break;
    case 0x84:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - INSTRUCTION_STORAGE");
      break;
    case 0x85:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - INSTRUCTION_SEGMENT");
      break;
    case 0x86:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - EXTERNAL");
      break;
    case 0x87:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - ALIGNMENT");
      break;
    case 0x88:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - PROGRAM");
      break;
    case 0x89:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - FPU_UNAVAILABLE");
      break;
    case 0x8a:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - DECREMENTER");
      break;
    case 0x8b:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - HYPERVISOR_DECREMENTER");
      break;
    case 0x8c:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - SYSTEM_CALL");
      break;
    case 0x8d:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - TRACE");
      break;
    case 0x8e:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - VPU_UNAVAILABLE");
      break;
    case 0x8f:
      LOG_XBOX(Xenon_PostBus, "1BL > PANIC - MAINTENANCE");
      break;
    case 0x90:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - VMX_ASSIST");
      break;
    case 0x91:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - THERMAL_MANAGEMENT");
      break;
    case 0x92:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - 1BL is executed on wrong CPU thread.");
      break;
    case 0x93:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - TOO_MANY_CORES - 1BL is executed on wrong CPU core.");
      break;
    case 0x94:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - VERIFY_OFFSET - CB offset verification failed.");
      break;
    case 0x95:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - VERIFY_HEADER - CB header verification failed.");
      break;
    case 0x96:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - SIG_VERIFY - CB RSA signature verification failed.");
      break;
    case 0x97:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - NONHOST_RESUME_STATUS");
      break;
    case 0x98:
      LOG_ERROR(Xenon_PostBus, "1BL > PANIC - NEXT_STAGE_SIZE - Size of next stage is out-of-bounds.");
      break;
    }
    // Pause the system.
    Base::SystemPause();
  }
  /* CB */
  else if (postCode >= 0x20 && postCode <= 0x3B) {
    switch (postCode) {
    case 0x20:
      LOG_XBOX(Xenon_PostBus, "CB > CB entry point. initialize SoC.");
      break;
    case 0x21:
      LOG_XBOX(Xenon_PostBus, "CB > INIT_SECOTP - Initialize secotp, verify lockdown fuses.");
      break;
    case 0x22:
      LOG_XBOX(Xenon_PostBus, "CB > INIT_SECENG - Initialize security engine.");
      break;
    case 0x23:
      LOG_XBOX(Xenon_PostBus, "CB > INIT_SYSRAM - Initialize EDRAM.");
      break;
    case 0x24:
      LOG_XBOX(Xenon_PostBus, "CB > VERIFY_OFFSET_3BL_CC");
      break;
    case 0x25:
      LOG_XBOX(Xenon_PostBus, "CB > LOCATE_3BL_CC");
      break;
    case 0x26:
      LOG_XBOX(Xenon_PostBus, "CB > FETCH_HEADER_3BL_CC");
      break;
    case 0x27:
      LOG_XBOX(Xenon_PostBus, "CB > VERIFY_HEADER_3BL_CC");
      break;
    case 0x28:
      LOG_XBOX(Xenon_PostBus, "CB > FETCH_CONTENTS_3BL_CC");
      break;
    case 0x29:
      LOG_XBOX(Xenon_PostBus, "CB > HMACSHA_COMPUTE_3BL_CC");
      break;
    case 0x2A:
      LOG_XBOX(Xenon_PostBus, "CB > RC4_INITIALIZE_3BL_CC");
      break;
    case 0x2B:
      LOG_XBOX(Xenon_PostBus, "CB > RC4_DECRYPT_3BL_CC");
      break;
    case 0x2C:
      LOG_XBOX(Xenon_PostBus, "CB > SHA_COMPUTE_3BL_CC");
      break;
    case 0x2D:
      LOG_XBOX(Xenon_PostBus, "CB > SIG_VERIFY_3BL_CC");
      break;
    case 0x2E:
      LOG_XBOX(Xenon_PostBus, "CB > HWINIT - Hardware initialization.");
      break;
    case 0x2F:
      LOG_XBOX(Xenon_PostBus, "CB > RELOCATE - Setup TLB entries, relocate to RAM.");
      break;
    case 0x30:
      LOG_XBOX(Xenon_PostBus, "CB > VERIFY_OFFSET_4BL_CD - Verify CD offset.");
      break;
    case 0x31:
      LOG_XBOX(Xenon_PostBus, "CB > FETCH_HEADER_4BL_CD - Verify CD header.");
      break;
    case 0x32:
      LOG_XBOX(Xenon_PostBus, "CB > VERIFY_HEADER_4BL_CD - Verify CD header.");
      break;
    case 0x33:
      LOG_XBOX(Xenon_PostBus, "CB > FETCH_CONTENTS_4BL_CD - Copy CD from NAND.");
      break;
    case 0x34:
      LOG_XBOX(Xenon_PostBus, "CB > HMACSHA_COMPUTE_4BL_CD - Create HMAC key for CD decryptio.n");
      break;
    case 0x35:
      LOG_XBOX(Xenon_PostBus, "CB > RC4_INITIALIZE_4BL_CD - Initialize CD RC4 key using HMAC key.");
      break;
    case 0x36:
      LOG_XBOX(Xenon_PostBus, "CB > RC4_DECRYPT_4BL_CD - RC4 decrypt CD with key.");
      break;
    case 0x37:
      LOG_XBOX(Xenon_PostBus, "CB > SHA_COMPUTE_4BL_CD - Compute hash of CD for verification.");
      break;
    case 0x38:
      LOG_XBOX(Xenon_PostBus, "CB > SIG_VERIFY_4BL_CD - RSA signature check of CD hash.");
      break;
    case 0x39:
      LOG_XBOX(Xenon_PostBus, "CB > SHA_VERIFY_4BL_CD - MemCmp cumputed hash with expected one.");
      break;
    case 0x3A:
      LOG_XBOX(Xenon_PostBus, "CB > BRANCH - Setup memory encryption and jump to CD.");
      break;
    case 0x3B:
      LOG_XBOX(Xenon_PostBus, "CB > PCI_INIT - Initialize PCI.");
      break;
    }
  }
  /* CB PANICS */
  else if (postCode >= 0x9B && postCode <= 0xB0) {
    switch (postCode) {
    case 0x9B:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_1 - Secopt fuse verification failed");
      break;
    case 0x9C:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_2 - Secopt fuse verification failed");
      break;
    case 0x9D:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_3 - Secopt fuse verification console type failed.");
      break;
    case 0x9E:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_4 - Secopt fuse verification console type failed.");
      break;
    case 0x9F:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_5 - Secopt fuse verification console type failed.");
      break;
    case 0xA0:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_6 - CB revocation check failed.");
      break;
    case 0xA1:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_7");
      break;
    case 0xA2:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_8");
      break;
    case 0xA3:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_9");
      break;
    case 0xA4:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_SECOTP_10 - Failed SMC HMAC.");
      break;
    case 0xA5:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_OFFSET_3BL_CC");
      break;
    case 0xA6:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - LOCATE_3BL_CC");
      break;
    case 0xA7:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_HEADER_3BL_CC");
      break;
    case 0xA8:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - SIG_VERIFY_3BL_CC");
      break;
    case 0xA9:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - HWINIT - Hardware Initialization failed.");
      break;
    case 0xAA:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_OFFSET_4BL_CC");
      break;
    case 0xAB:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - VERIFY_HEADER_4BL_CC");
      break;
    case 0xAC:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - SIG_VERIFY_4BL_CC");
      break;
    case 0xAD:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - SHA_VERIFY_4BL_CC");
      break;
    case 0xAE:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - UNEXPECTED_INTERRUPT");
      break;
    case 0xAF:
      LOG_ERROR(Xenon_PostBus, "CB > PANIC - UNSUPPORTED_RAM_SIZE");
      break;
    default:
      LOG_ERROR(Xenon_PostBus, "CB > Unrecognized PANIC code 0x{:X}", postCode);
      break;
    }
    Base::SystemPause();
  }
  /* CB_A */
  else if (postCode >= 0xD0 && postCode <= 0xDB) {
    switch (postCode) {
    case 0xD0:
      LOG_XBOX(Xenon_PostBus, "CB_A > CB_A_ENTRY - CB_A entry point, copy self to "
               "0x8000.0200.0001.C000 and continue from there.");
      break;
    case 0xD1:
      LOG_XBOX(Xenon_PostBus, "CB_A > READ_FUSES - Copy fuses from SoC for CB_B decryption.");
      break;
    case 0xD2:
      LOG_XBOX(Xenon_PostBus, "CB_A > VERIFY_OFFSET_CB_B - Verify CB_B offset.");
      break;
    case 0xD3:
      LOG_XBOX(Xenon_PostBus, "CB_A > FETCH_HEADER_CB_B - Copy CB_B header from NAND for verification.");
      break;
    case 0xD4:
      LOG_XBOX(Xenon_PostBus, "CB_A > VERIFY_HEADER_CB_B - Verify CB_B header.");
      break;
    case 0xD5:
      LOG_XBOX(Xenon_PostBus, "CB_A > FETCH_CONTENTS_CB_B - Copy CBB into memory at "
                              "0x8000.0200.0001.0000 (Old location of CB_A).");
      break;
    case 0xD6:
      LOG_XBOX(Xenon_PostBus, "CB_A > HMACSHA_COMPUTE_CB_B - Create HMAC key for CD decryption.");
      break;
    case 0xD7:
      LOG_XBOX(Xenon_PostBus, "CB_A > RC4_INITIALIZE_CB_B - Initialize CD RC4 key using HMAC key.");
      break;
    case 0xD8:
      LOG_XBOX(Xenon_PostBus, "CB_A > RC4_DECRYPT_CB_B - RC4 decrypt CD.");
      break;
    case 0xD9:
      LOG_XBOX(Xenon_PostBus, "CB_A > SHA_COMPUTE_CB_B - Compute hash of CD for verification.");
      break;
    case 0xDA:
      LOG_XBOX(Xenon_PostBus, "CB_A > SHA_VERIFY_CB_B - MemCmp computed hash with expected one "
                              "(where RGH2 glitches).");
      break;
    case 0xDB:
      LOG_XBOX(Xenon_PostBus, "CB_A > BRANCH_CB_B - Verify CB_B offset.");
      break;
    }
  }
  /* CB_A PANICS */
  else if (postCode >= 0xF0 && postCode <= 0xF3) {
    switch (postCode) {
    case 0xF0:
      LOG_ERROR(Xenon_PostBus, "CB_A > PANIC - VERIFY_OFFSET_CB_B - CB_B offset verification fail.");
      break;
    case 0xF1:
      LOG_ERROR(Xenon_PostBus, "CB_A > PANIC - VERIFY_HEADER_CB_B - CB_B header verification fail");
      break;
    case 0xF2:
      LOG_ERROR(Xenon_PostBus, "CB_A > PANIC - SHA_VERIFY_CB_B - CB_B security hash comparison fail.");
      break;
    case 0xF3:
      LOG_ERROR(Xenon_PostBus, "CB_A > PANIC - ENTRY_SIZE_INVALID_CB_B - CB_B size check fail "
                                "(must be less than 0xC000).");
      break;
    }
    Base::SystemPause();
  }
  /* CD */
  else if (postCode >= 0x40 && postCode <= 0x53) {
    switch (postCode) {
    case 0x40:
      LOG_XBOX(Xenon_PostBus, "CD > Entrypoint of CD, setup memory paging.");
      break;
    case 0x41:
      LOG_XBOX(Xenon_PostBus, "CD > VERIFY_OFFSET - Verify offset to CE.");
      break;
    case 0x42:
      LOG_XBOX(Xenon_PostBus, "CD > FETCH_HEADER - Copy CE header from NAND for verification.");
      break;
    case 0x43:
      LOG_XBOX(Xenon_PostBus, "CD > VERIFY_HEADER - Verify CE header.");
      break;
    case 0x44:
      LOG_XBOX(Xenon_PostBus, "CD > FETCH_CONTENTS - Read CE from NAND into memory.");
      break;
    case 0x45:
      LOG_XBOX(Xenon_PostBus, "CD > HMACSHA_COMPUTE - Create HMAC key for CE decryption.");
      break;
    case 0x46:
      LOG_XBOX(Xenon_PostBus, "CD > RC4_INITIALIZE - Initialize CE RC4 key using HMAC key.");
      break;
    case 0x47:
      LOG_XBOX(Xenon_PostBus, "CD > RC4_DECRYPT - RC4 decrypt CE.");
      break;
    case 0x48:
      LOG_XBOX(Xenon_PostBus, "CD > SHA_COMPUTE - Compute hash of CE for verification.");
      break;
    case 0x49:
      LOG_XBOX(Xenon_PostBus, "CD > SHA_VERIFY - MemCmp computed hash with expected one. (RGH1 "
                              "Glitches here)");
      break;
    case 0x4A:
      LOG_XBOX(Xenon_PostBus, "LOAD_6BL_CF");
      break;
    case 0x4B:
      LOG_XBOX(Xenon_PostBus, "LZX_EXPAND - LZX Decompress CE.");
      break;
    case 0x4C:
      LOG_XBOX(Xenon_PostBus, "SWEEP_CACHES");
      break;
    case 0x4D:
      LOG_XBOX(Xenon_PostBus, "DECODE_FUSES");
      break;
    case 0x4E:
      LOG_XBOX(Xenon_PostBus, "FETCH_OFFSET_6BL_CF - Load CD (kernel patches) offset.");
      break;
    case 0x4F:
      LOG_XBOX(Xenon_PostBus, "VERIFY_OFFSET_6BL_CF - Verify CF offset.");
      break;
    case 0x50:
      LOG_XBOX(Xenon_PostBus, "LOAD_UPDATE_1 - Load CF1/CG1 (patch slot 1) if version & "
                              "header check pass.");
      break;
    case 0x51:
      LOG_XBOX(Xenon_PostBus, "LOAD_UPDATE_2 - Load CF2/CG2 (patch slot 2) if version & "
                              "header check pass.");
      break;
    case 0x52:
      LOG_XBOX(Xenon_PostBus, "BRANCH - Startup kernel/hypervisor.");
      break;
    case 0x53:
      LOG_XBOX(Xenon_PostBus, "DECRYPT_VERIFY_HV_CERT - Decrypt and verify hypervisor "
                              "certificate.");
      break;
    }
  }
  /* CD PANICS */
  else if (postCode >= 0xB1 && postCode <= 0xB8) {
    switch (postCode) {
    case 0xB1:
      LOG_ERROR(Xenon_PostBus, "CD > PANIC - VERIFY_OFFSET - CE decryption failed.");
      break;
    case 0xB2:
      LOG_ERROR(Xenon_PostBus, "PANIC - VERIFY_HEADER - Failed to verify CE header.");
      break;
    case 0xB3:
      LOG_ERROR(Xenon_PostBus, "PANIC - SHA_VERIFY - CE hash comparison failed.");
      break;
    case 0xB4:
      LOG_ERROR(Xenon_PostBus, "PANIC - LZX_EXPAND - CE LZX decompression failed.");
      break;
    case 0xB5:
      LOG_ERROR(Xenon_PostBus, "PANIC - VERIFY_OFFSET_6BL - CF verification failed.");
      break;
    case 0xB6:
      LOG_ERROR(Xenon_PostBus, "PANIC - DECODE_FUSES - Fuse decryption/check failed.");
      break;
    case 0xB7:
      LOG_ERROR(Xenon_PostBus, "PANIC - UPDATE_MISSING - CF decryption failed, patches missing.");
      break;
    case 0xB8:
      LOG_ERROR(Xenon_PostBus, "PANIC - CF_HASH_AUTH - CF hash auth failed.");
      break;
    }
    Base::SystemPause();
  }
  /* CE/CF PANICS */
  else if (postCode >= 0xC1 && postCode <= 0xC8) {
    switch (postCode) {
    case 0xC1:
      LOG_ERROR(Xenon_PostBus, "CE/F PANIC - LZX_EXPAND_1 - LDICreateDecompression.");
      break;
    case 0xC2:
      LOG_ERROR(Xenon_PostBus, "PANIC - LZX_EXPAND_2 - 7BL Size Verification.");
      break;
    case 0xC3:
      LOG_ERROR(Xenon_PostBus, "PANIC - LZX_EXPAND_3 - Header/Patch Fragment Info.");
      break;
    case 0xC4:
      LOG_ERROR(Xenon_PostBus, "PANIC - LZX_EXPAND_4 - Unexpected LDI Fragment.");
      break;
    case 0xC5:
      LOG_ERROR(Xenon_PostBus, "PANIC - LZX_EXPAND_5 - LDISetWindowData.");
      break;
    case 0xC6:
      LOG_ERROR(Xenon_PostBus, "PANIC - LZX_EXPAND_6 - LDIDecompress.");
      break;
    case 0xC7:
      LOG_ERROR(Xenon_PostBus, "PANIC - LZX_EXPAND_7 - LDIResetDecompression.");
      break;
    case 0xC8:
      LOG_ERROR(Xenon_PostBus, "PANIC - SHA_VERIFY - 7BL Signature Verify.");
      break;
    }
    Base::SystemPause();
  }
  /* HYPERVISOR */
  else if (postCode >= 0x58 && postCode <= 0x5F) {
    switch (postCode) {
    case 0x58:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_HYPERVISOR - Hypervisor Initialization begin.");
      break;
    case 0x59:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_SOC_MMIO - Initialize SoC MMIO.");
      break;
    case 0x5A:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_XEX_TRAINING - Initialize XEX training.");
      break;
    case 0x5B:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_KEYRING - Initialize key ring.");
      break;
    case 0x5C:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_KEYS - Initialize keys.");
      break;
    case 0x5D:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_SOC_INT - Initialize SoC Interrupts.");
      break;
    case 0x5E:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_SOC_INT_COMPLETE - Initialization complete.");
      break;
    case 0x5F:
      LOG_XBOX(Xenon_PostBus, "HV > INIT_HYPERVISOR_COMPLETE - Hypervisor Initialization end.");
      break;
    }
  }
  /* HYPERVISOR PANICS */
  else if (postCode == 0xFF) {
      LOG_ERROR(Xenon_PostBus, "HV > PANIC - FATAL!");
  }
  /* KERNEL */ // Kernel post codes vary according to each system version. Lets rely
               // on the DebugPrints for now.
  else if (postCode >= 0x60 && postCode <= 0x79) {
      /*
    switch (postCode) {
    case 0x60:
      LOG_XBOX(Xenon_PostBus, "INIT_KERNEL - Initialize kernel");
      break;
    case 0x61:
      LOG_XBOX(Xenon_PostBus, "INITIAL_HAL_PHASE_0 - Initialize HAL phase 0");
      break;
    case 0x62:
      LOG_XBOX(Xenon_PostBus, "INIT_PROCESS_OBJECTS - Initialize process objects");
      break;
    case 0x63:
      LOG_XBOX(Xenon_PostBus, "INIT_KERNEL_DEBUGGER - Initialize kernel debugger");
      break;
    case 0x64:
      LOG_XBOX(Xenon_PostBus, "INIT_MEMORY_MANAGER - Initialize memory manager");
      break;
    case 0x65:
      LOG_XBOX(Xenon_PostBus, "INIT_STACKS - Initialize stacks");
      break;
    case 0x66:
      LOG_XBOX(Xenon_PostBus, "INIT_OBJECT_SYSTEM - Initialize object system");
      break;
    case 0x67:
      LOG_XBOX(Xenon_PostBus, "INIT_PHASE1_THREAD - Initialize phase 1 thread");
      break;
    case 0x68:
      LOG_XBOX(Xenon_PostBus, "INIT_PROCESSORS - Initialize processors");
      break;
    case 0x69:
      LOG_XBOX(Xenon_PostBus, "INIT_KEYVAULT - Initialize keyvault");
      break;
    case 0x6A:
      LOG_XBOX(Xenon_PostBus, "INIT_HAL_PHASE_1 - Initialize HAL phase 1");
      break;
    case 0x6B:
      LOG_XBOX(Xenon_PostBus, "INIT_SFC_DRIVER - Initialize flash controller");
      break;
    case 0x6C:
      LOG_XBOX(Xenon_PostBus, "INIT_SECURITY - Initialize security");
      break;
    case 0x6D:
      LOG_XBOX(Xenon_PostBus, "INIT_KEY_EX_VAULT - Initialize extended keyvault");
      break;
    case 0x6E:
      LOG_XBOX(Xenon_PostBus, "INIT_SETTINGS - Initialize settings");
      break;
    case 0x6F:
      LOG_XBOX(Xenon_PostBus, "INIT_POWER_MODE - Initialize power mode");
      break;
    case 0x70:
      LOG_XBOX(Xenon_PostBus, "INIT_VIDEO_DRIVER - Initialize video driver");
      break;
    case 0x71:
      LOG_XBOX(Xenon_PostBus, "INIT_AUDIO_DRIVER - Initialize audio driver");
      break;
    case 0x72:
      LOG_XBOX(Xenon_PostBus, "INIT_BOOT_ANIMATION - Initialize bootanim.xex, XMADecoder, and XAudioRender");
      break;
    case 0x73:
      LOG_XBOX(Xenon_PostBus, "INIT_SATA_DRIVER - Initialize SATA driver");
      break;
    case 0x74:
      LOG_XBOX(Xenon_PostBus, "INIT_SHADOWBOOT - Initialize shadowboot");
      break;
    case 0x75:
      LOG_XBOX(Xenon_PostBus, "INIT_DUMP_SYSTEM - Initialize dump system");
      break;
    case 0x76:
      LOG_XBOX(Xenon_PostBus, "INIT_SYSTEM_ROOT - Initialize system root");
      break;
    case 0x77:
      LOG_XBOX(Xenon_PostBus, "INIT_OTHER_DRIVERS - Initialize other drivers");
      break;
    case 0x78:
      LOG_XBOX(Xenon_PostBus, "INIT_STFS_DRIVER - Initialize STFS driver");
      break;
    case 0x79:
      LOG_XBOX(Xenon_PostBus, "LOAD_XAM - Initialize xam.xex");
      break;
    }*/
  }
  else {
    LOG_ERROR(Xenon_PostBus, "POST: Unrecognized post code: 0x{:X}", postCode);
  }
}

std::string Xe::XCPU::POSTBUS::GET_POST(u64 postCode) {
  /* 1BL */
  if (postCode >= 0x10 && postCode <= 0x1E) {
    switch (postCode) {
    case 0x10:
      return "1BL started.";
    case 0x11:
      return "FSB_CONFIG_PHY_CONTROL";
    case 0x12:
      return "FSB_CONFIG_RX_STATE";
    case 0x13:
      return "FSB_CONFIG_TX_STATE";
    case 0x14:
      return "FSB_CONFIG_TX_CREDITS";
    case 0x15:
      return "FETCH_OFFSET";
    case 0x16:
      return "FETCH_HEADER";
    case 0x17:
      return "VERIFY_HEADER";
    case 0x18:
      return "FETCH_CONTENTS";
    case 0x19:
      return "HMACSHA_COMPUTE";
    case 0x1A:
      return "RC4_INITIALIZE";
    case 0x1B:
      return "RC4_DECRYPT";
    case 0x1C:
      return "SHA_COMPUTE";
    case 0x1D:
      return "SIG_VERIFY";
    case 0x1E:
      return "BRANCH";
    }
  }
  /* 1BL PANICS */
  else if (postCode >= 0x81 && postCode <= 0x98) {
    switch (postCode) {
    /* 1BL PANIC*/
    case 0x81:
      return "MACHINE_CHECK-FAIL";
    case 0x82:
      return "DATA_STORAGE-FAIL";
    case 0x83:
      return "DATA_SEGMENT-FAIL";
    case 0x84:
      return "INSTRUCTION_STORAGE-FAIL";
    case 0x85:
      return "INSTRUCTION_SEGMENT-FAIL";
    case 0x86:
      return "EXTERNAL-FAIL";
    case 0x87:
      return "ALIGNMENT-FAIL";
    case 0x88:
      return "PROGRAM-FAIL";
    case 0x89:
      return "FPU_UNAVAILABLE-FAIL";
    case 0x8a:
      return "DECREMENTER-FAIL";
    case 0x8b:
      return "HYPERVISOR_DECREMENTER-FAIL";
    case 0x8c:
      return "SYSTEM_CALL-FAIL";
    case 0x8d:
      return "TRACE-FAIL";
    case 0x8e:
      return "VPU_UNAVAILABLE-FAIL";
    case 0x8f:
      return "MAINTENANCE-FAIL";
    case 0x90:
      return "VMX_ASSIST-FAIL";
    case 0x91:
      return "THERMAL_MANAGEMENT-FAIL";
    case 0x92:
      return "INVALID_THREAD-FAIL";
    case 0x93:
      return "TOO_MANY_CORES-FAIL";
    case 0x94:
      return "VERIFY_OFFSET-FAIL";
    case 0x95:
      return "VERIFY_HEADER-FAIL";
    case 0x96:
      return "SIG_VERIFY-FAIL";
    case 0x97:
      return "NONHOST_RESUME_STATUS-FAIL";
    case 0x98:
      return "NEXT_STAGE_SIZE-FAIL";
    }
  }
  /* CB */
  else if (postCode >= 0x20 && postCode <= 0x3B) {
    switch (postCode) {
    case 0x20:
      return "CB_ENTRY";
    case 0x21:
      return "CB_INIT_SECOTP";
    case 0x22:
      return "CB_INIT_SECENG";
    case 0x23:
      return "CB_INIT_SYSRAM";
    case 0x24:
      return "CB_VERIFY_OFFSET_3BL_CC";
    case 0x25:
      return "CB_LOCATE_3BL_CC";
    case 0x26:
      return "CB_FETCH_HEADER_3BL_CC";
    case 0x27:
      return "CB_VERIFY_HEADER_3BL_CC";
    case 0x28:
      return "CB_FETCH_CONTENTS_3BL_CC";
    case 0x29:
      return "CB_HMACSHA_COMPUTE_3BL_CC";
    case 0x2A:
      return "CB_RC4_INITIALIZE_3BL_CC";
    case 0x2B:
      return "CB_RC4_DECRYPT_3BL_CC";
    case 0x2C:
      return "CB_SHA_COMPUTE_3BL_CC";
    case 0x2D:
      return "CB_SIG_VERIFY_3BL_CC";
    case 0x2E:
      return "CB_HWINIT";
    case 0x2F:
      return "CB_RELOCATE";
    case 0x30:
      return "CB_VERIFY_OFFSET_4BL_CD";
    case 0x31:
      return "CB_FETCH_HEADER_4BL_CD";
    case 0x32:
      return "CB_VERIFY_HEADER_4BL_CD";
    case 0x33:
      return "CB_FETCH_CONTENTS_4BL_CD";
    case 0x34:
      return "CB_HMACSHA_COMPUTE_4BL_CD";
    case 0x35:
      return "CB_RC4_INITIALIZE_4BL_CD";
    case 0x36:
      return "CB_RC4_DECRYPT_4BL_CD";
    case 0x37:
      return "CB_SHA_COMPUTE_4BL_CD";
    case 0x38:
      return "CB_SIG_VERIFY_4BL_CD";
    case 0x39:
      return "CB_SHA_VERIFY_4BL_CD";
    case 0x3A:
      return "CB_BRANCH";
    case 0x3B:
      return "CB_PCI_INIT";
    }
  }
  /* CB PANICS */
  else if (postCode >= 0x9B && postCode <= 0xB0) {
    switch (postCode) {
    case 0x9B:
      return "VERIFY_SECOTP_1-FAIL";
    case 0x9C:
      return "VERIFY_SECOTP_2-FAIL";
    case 0x9D:
      return "VERIFY_SECOTP_3-FAIL";
    case 0x9E:
      return "VERIFY_SECOTP_4-FAIL";
    case 0x9F:
      return "VERIFY_SECOTP_5-FAIL";
    case 0xA0:
      return "VERIFY_SECOTP_6-FAIL";
    case 0xA1:
      return "VERIFY_SECOTP_7-FAIL";
    case 0xA2:
      return "VERIFY_SECOTP_8-FAIL";
    case 0xA3:
      return "VERIFY_SECOTP_9-FAIL";
    case 0xA4:
      return "VERIFY_SECOTP_10-FAIL";
    case 0xA5:
      return "VERIFY_OFFSET_3BL_CC-FAIL";
    case 0xA6:
      return "LOCATE_3BL_CC-FAIL";
    case 0xA7:
      return "VERIFY_HEADER_3BL_CC-FAIL";
    case 0xA8:
      return "SIG_VERIFY_3BL_CC-FAIL";
    case 0xA9:
      return "HWINIT_FAIL-FAIL";
    case 0xAA:
      return "VERIFY_OFFSET_4BL_CC-FAIL";
    case 0xAB:
      return "VERIFY_HEADER_4BL_CC-FAIL";
    case 0xAC:
      return "SIG_VERIFY_4BL_CC-FAIL";
    case 0xAD:
      return "SHA_VERIFY_4BL_CC-FAIL";
    case 0xAE:
      return "UNEXPECTED_INTERRUPT-FAIL";
    case 0xAF:
      return "UNSUPPORTED_RAM_SIZE-FAIL";
    default:
      return FMT("CB_UNREC_PANIC_0x{:X}", postCode);
    }
  }
  /* CB_A */
  else if (postCode >= 0xD0 && postCode <= 0xDB) {
    switch (postCode) {
    case 0xD0:
      return "CB_A_ENTRY";
    case 0xD1:
      return "READ_FUSES";
    case 0xD2:
      return "VERIFY_OFFSET_CB_B";
    case 0xD3:
      return "FETCH_HEADER_CB_B";
    case 0xD4:
      return "VERIFY_HEADER_CB_B";
    case 0xD5:
      return "FETCH_CONTENTS_CB_B";
    case 0xD6:
      return "HMACSHA_COMPUTE_CB_B";
    case 0xD7:
      return "RC4_INITIALIZE_CB_B";
    case 0xD8:
      return "RC4_DECRYPT_CB_B";
    case 0xD9:
      return "SHA_COMPUTE_CB_B";
    case 0xDA:
      return "SHA_VERIFY_CB_B_EXPECTED-RGH";
    case 0xDB:
      return "BRANCH_CB_B";
    }
  }
  /* CB_A PANICS */
  else if (postCode >= 0xF0 && postCode <= 0xF3) {
    switch (postCode) {
    case 0xF0:
      return "VERIFY_OFFSET_CB_B";
    case 0xF1:
      return "VERIFY_HEADER_CB_B";
    case 0xF2:
      return "SHA_VERIFY_CB_B";
    case 0xF3:
      return "ENTRY_SIZE_INVALID_CB_B";
    }
  }
  /* CD */
  else if (postCode >= 0x40 && postCode <= 0x53) {
    switch (postCode) {
    case 0x40:
      return "CD_ENTRY";
    case 0x41:
      return "VERIFY_OFFSET_CE";
    case 0x42:
      return "FETCH_HEADER_CE";
    case 0x43:
      return "VERIFY_HEADER_CE";
    case 0x44:
      return "FETCH_CONTENTS_CE";
    case 0x45:
      return "HMACSHA_COMPUTE_CE";
    case 0x46:
      return "RC4_INITIALIZE_CE";
    case 0x47:
      return "RC4_DECRYPT_CE";
    case 0x48:
      return "SHA_COMPUTE_CE";
    case 0x49:
      return "SHA_VERIFY-RGH";
    case 0x4A:
      return "LOAD_6BL_CF";
    case 0x4B:
      return "LZX_EXPAND_CE";
    case 0x4C:
      return "SWEEP_CACHES";
    case 0x4D:
      return "DECODE_FUSES";
    case 0x4E:
      return "FETCH_OFFSET_6BL_CF";
    case 0x4F:
      return "VERIFY_OFFSET_6BL_CF";
    case 0x50:
      return "LOAD_UPDATE_1";
    case 0x51:
      return "LOAD_UPDATE_2";
    case 0x52:
      return "BRANCH";
    case 0x53:
      return "DECRYPT_VERIFY_HV_CERT";
    }
  }
  /* CD PANICS */
  else if (postCode >= 0xB1 && postCode <= 0xB8) {
    switch (postCode) {
    case 0xB1:
      return "VERIFY_OFFSET_CE-FAIL";
    case 0xB2:
      return "VERIFY_HEADER_CE-FAIL";
    case 0xB3:
      return "SHA_VERIFY_CE-FAIL";
    case 0xB4:
      return "LZX_EXPAND_CE-FAIL";
    case 0xB5:
      return "VERIFY_OFFSET_6BL-FAIL";
    case 0xB6:
      return "DECODE_FUSES-FAIL";
    case 0xB7:
      return "UPDATE_MISSING-FAIL";
    case 0xB8:
      return "CF_HASH_AUTH-FAIL";
    }
  }
  /* CE/CF PANICS */
  else if (postCode >= 0xC1 && postCode <= 0xC8) {
    switch (postCode) {
    case 0xC1:
      return "LZX_EXPAND_1";
    case 0xC2:
      return "LZX_EXPAND_2";
    case 0xC3:
      return "LZX_EXPAND_3";
    case 0xC4:
      return "LZX_EXPAND_4";
    case 0xC5:
      return "LZX_EXPAND_5";
    case 0xC6:
      return "LZX_EXPAND_6";
    case 0xC7:
      return "LZX_EXPAND_7";
    case 0xC8:
      return "SHA_VERIFY";
    }
  }
  /* HYPERVISOR */
  else if (postCode >= 0x58 && postCode <= 0x5F) {
    switch (postCode) {
    case 0x58:
      return "INIT_HYPERVISOR";
    case 0x59:
      return "INIT_SOC_MMIO";
    case 0x5A:
      return "INIT_XEX_TRAINING";
    case 0x5B:
      return "INIT_KEYRING";
    case 0x5C:
      return "INIT_KEYS";
    case 0x5D:
      return "INIT_SOC_INT";
    case 0x5E:
      return "INIT_SOC_INT_COMPLETE";
    case 0x5F:
      return "INIT_HYPERVISOR_COMPLETE";
    }
  }
  /* HYPERVISOR PANICS */
  else if (postCode == 0xFF) {
    return "FATAL";
  }
  /* KERNEL */
  else if (postCode >= 0x60 && postCode <= 0x79) {
    switch (postCode) {
    case 0x60:
      return "INIT_KERNEL";
    case 0x61:
      return "INITIAL_HAL_PHASE_0";
    case 0x62:
      return "INIT_PROCESS_OBJECTS";
    case 0x63:
      return "INIT_KERNEL_DEBUGGER";
    case 0x64:
      return "INIT_MEMORY_MANAGER";
    case 0x65:
      return "INIT_STACKS";
    case 0x66:
      return "INIT_OBJECT_SYSTEM";
    case 0x67:
      return "INIT_PHASE1_THREAD";
    case 0x68:
      return "INIT_PROCESSORS";
    case 0x69:
      return "INIT_KEYVAULT";
    case 0x6A:
      return "INIT_HAL_PHASE_1";
    case 0x6B:
      return "INIT_SFC_DRIVER";
    case 0x6C:
      return "INIT_SECURITY";
    case 0x6D:
      return "INIT_KEY_EX_VAULT";
    case 0x6E:
      return "INIT_SETTINGS";
    case 0x6F:
      return "INIT_POWER_MODE";
    case 0x70:
      return "INIT_VIDEO_DRIVER";
    case 0x71:
      return "INIT_AUDIO_DRIVER";
    case 0x72:
      return "INIT_BOOT_ANIMATION";
    case 0x73:
      return "INIT_SATA_DRIVER";
    case 0x74:
      return "INIT_SHADOWBOOT";
    case 0x75:
      return "INIT_DUMP_SYSTEM";
    case 0x76:
      return "INIT_SYSTEM_ROOT";
    case 0x77:
      return "INIT_OTHER_DRIVERS";
    case 0x78:
      return "INIT_STFS_DRIVER";
    case 0x79:
      return "LOAD_XAM";
    }
  }
  return FMT("UNREC_POST_0x{:X}", postCode);
}
