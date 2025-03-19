// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fstream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Base/Types.h"
#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIe.h"

/*
 *	XGPU.h Basic Xenos implementation.
 */

namespace Xe {
namespace Xenos {

#define XGPU_DEVICE_SIZE 0x10000
#define XE_FB_BASE 0x1E000000

//
// Xenos Registers
//

#define REG_GPU_CLK 0x0210
#define REG_MEM_CLK 0x0284
#define REG_EDRAM_CLK 0x0244
#define REG_FSB_CLK 0x0248
#define REG_D1CRTC_UPDATE_LOCK 0x60E8
#define REG_D1CRTC_DOUBLE_BUFFER_CONTROL 0x60EC
#define REG_D1CRTC_V_TOTAL 0x6020
#define REG_D1CRTC_H_TOTAL 0x6000
#define REG_D1CRTC_H_SYNC_B 0x6010
#define REG_D1CRTC_H_BLANK_START_END 0x6004
#define REG_D1CRTC_H_SYNC_B_CNTL 0x6014
#define REG_D1CRTC_H_SYNC_A 0x6008
#define REG_D1CRTC_V_SYNC_B 0x6030
#define REG_D1CRTC_H_SYNC_A_CNTL 0x600C
#define REG_D1CRTC_MVP_INBAND_CNTL_CAP 0x604C
#define REG_D1CRTC_MVP_INBAND_CNTL_INSERT 0x6050
#define REG_D1CRTC_MVP_FIFO_STATUS 0x6044
#define REG_D1CRTC_MVP_SLAVE_STATUS 0x6048
#define REG_AVIVO_D1CRTC_V_BLANK_START_END 0x6024
#define REG_D1CRTC_MVP_CONTROL1 0x6038
#define REG_D1CRTC_MVP_CONTROL2 0x603C
#define REG_D1CRTC_MVP_FIFO_CONTROL 0x6040
#define REG_D1CRTC_MVP_INBAND_CNTL_INSERT_TIMER 0x6054
#define REG_D1CRTC_MVP_BLACK_KEYER 0x6058
#define REG_D1CRTC_TRIGA_CNTL 0x6060
#define REG_D1CRTC_TRIGA_MANUAL_TRIG 0x6064
#define REG_D1CRTC_TRIGB_CNTL 0x6068

#define REG_D1GRPH_COLOR_MATRIX_TRANSFORMATION_CNTL 0x6380
#define REG_D1COLOR_MATRIX_COEF_1_1 0x6384
#define REG_D1COLOR_MATRIX_COEF_1_2 0x6388
#define REG_D1COLOR_MATRIX_COEF_1_3 0x638C
#define REG_D1COLOR_MATRIX_COEF_1_4 0x6390
#define REG_D1COLOR_MATRIX_COEF_2_1 0x6394
#define REG_D1COLOR_MATRIX_COEF_2_2 0x6398
#define REG_D1COLOR_MATRIX_COEF_2_3 0x639C
#define REG_D1COLOR_MATRIX_COEF_2_4 0x63A0
#define REG_D1COLOR_MATRIX_COEF_3_1 0x63A4
#define REG_D1COLOR_MATRIX_COEF_3_2 0x63A8
#define REG_D1COLOR_MATRIX_COEF_3_3 0x63AC
#define REG_D1COLOR_MATRIX_COEF_3_4 0x63B0
#define REG_DC_LUTA_CONTROL 0x64C0
#define REG_DC_LUT_RW_INDEX 0x6488
#define REG_DC_LUT_RW_MODE 0x6484
#define REG_DC_LUT_PWL_DATA 0x6490
#define REG_DC_LUT_WRITE_EN_MASK 0x649C
#define REG_DC_LUT_AUTOFILL 0x64A0
#define REG_AVIVO_D1MODE_DATA_FORMAT 0x6528
#define REG_AVIVO_D1SCL_UPDATE 0x65CC
#define REG_AVIVO_D1SCL_SCALER_ENABLE 0x6590
#define REG_AVIVO_D1MODE_VIEWPORT_START 0x6580
#define REG_AVIVO_D1MODE_VIEWPORT_SIZE 0x6584
#define REG_AVIVO_D1SCL_SCALER_TAP_CONTROL 0x6594
#define REG_AVIVO_D1MODE_DESKTOP_HEIGHT 0x652C
#define REG_DCP_LB_DATA_GAP_BETWEEN_CHUNK 0x6CBC

struct XenosState {
  std::unique_ptr<u8[]> Regs;
};

// ARGB (Console is BGRA)
#define COLOR(r, g, b, a) ((a) << 24 | (r) << 16 | (g) << 8 | (b) << 0)
#define TILE(x) ((x + 31) >> 5) << 5
class XGPU {
public:
  XGPU(RAM *ram);
  ~XGPU();

  // Memory Read/Write methods.
  bool Read(u64 readAddress, u8 *data, u8 byteCount);
  bool Write(u64 writeAddress, u8 *data, u8 byteCount);

  void ConfigRead(u64 readAddress, u8 *data, u8 byteCount);
  void ConfigWrite(u64 writeAddress, u8 *data, u8 byteCount);

  bool isAddressMappedInBAR(u32 address);

  // Dump framebuffer from RAM
  void DumpFB(const std::filesystem::path &path, int pitch);

private:
  // Mutex handle
  std::recursive_mutex mutex{};
  // XGPU Config Space Data at address 0xD0010000.
  GENRAL_PCI_DEVICE_CONFIG_SPACE xgpuConfigSpace{};
  // PCI Device Size, using when determining PCI device size of each BAR in Linux.
  u32 pciDevSizes[6]{};

  // RAM Pointer
  RAM *ramPtr{};

  // GPU State
  XenosState xenosState{};
};
} // namespace Xenos
} // namespace Xe
