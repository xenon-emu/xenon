/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

/*
* Most of the information in this file was gathered from
* various sources across the internet.
*
* In particular, this work is based heavily on research by
* the Xenia developers - especially Triang3l -
* whose dedication and deep understanding of the hardware
* made accurate Xenos emulation possible.
*
* Huge thanks to everyone who contributed to uncovering and
* documenting this complex system.
*/

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>
#include <unordered_map>

#include "Base/Logging/Log.h"
#include "Base/Types.h"

#include "Core/PCI/Devices/RAM/RAM.h"
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/XGPU/Microcode/ASTBlock.h"
#include "Core/XGPU/PM4Opcodes.h"
#include "Core/XGPU/RingBuffer.h"
#include "Core/XGPU/XenosRegisters.h"
#include "Core/XGPU/Xenos.h"
#include "Core/XGPU/XenosState.h"

#include "Render/Abstractions/Buffer.h"
#include "Render/Abstractions/Shader.h"
#include "Render/Abstractions/Texture.h"

// Xenos GPU Command Processor.
// Handles all commands sent to the Xenos via the RingBuffer.
// The RingBuffer is a dedicated area of memory used as storage for CP packets.

#ifdef _DEBUG
#define XE_DEBUG
#endif

namespace Render { class Renderer; }

namespace Xe::XGPU {

// Index Buffer info for DRAW_INDX_* PM4 commands.
struct XeIndexBufferInfo {
  eIndexFormat indexFormat = eIndexFormat::xeInt16;
  eEndian endianness = eEndian::xeNone;
  u32 count = 0;
  u32 guestBase = 0;
  u8 *elements = 0;
  u64 length = 0;
};

#ifndef NO_GFX
struct XeShader {
  u32 vertexShaderHash = 0;
  Microcode::AST::Shader *vertexShader = nullptr;
  u32 pixelShaderHash = 0;
  Microcode::AST::Shader *pixelShader = nullptr;
  std::vector<std::shared_ptr<Render::Texture>> textures{};
  std::shared_ptr<Render::Shader> program = {};
};
#endif

struct XeDrawParams {
  XenosState *state = nullptr;
  XeIndexBufferInfo indexBufferInfo = {};
  VGT_DRAW_INITIATOR_REG vgtDrawInitiator = {};
  u32 maxVertexIndex = 0;
  u32 minVertexIndex = 0;
  u32 indexOffset = 0;
  u32 multiPrimitiveIndexBufferResetIndex = 0;
  u32 currentBinIdMin = 0;
};

class CommandProcessor {
public:
  CommandProcessor(RAM *ramPtr, XenosState *statePtr, Render::Renderer *renderer, PCIBridge *pciBridge);
  ~CommandProcessor();

  // Methods for R/W of the CP/PFP uCode data
  void CPSetPFPMicrocodeAddress(u32 address) { cpPFPuCodeAddress = address; }
  void CPSetMEMicrocodeWriteAddress(u32 address) { cpMEuCodeWriteAddress = address; }
  void CPSetMEMicrocodeReadAddress(u32 address) { cpMEuCodeReadAddress = address; }
  void CPWriteMicrocodeData(eCPMicrocodeType uCodeType, u32 data);
  u32 CPReadMicrocodeData(eCPMicrocodeType uCodeType);

  // Update RingBuffer base address
  void CPUpdateRBBase(u32 address);
  // Update RingBuffer size
  void CPUpdateRBSize(size_t newSize);
  // CP RB Write Ptr offset (from base in words)
  void CPUpdateRBWritePointer(u32 offset);

  void CPSetSQProgramCntl(u32 value);

private:
  // PCI Bridge pointer. Used for interrupts
  PCIBridge *parentBus{};

  // RAM Poiner, for DMA ops and RingBuffer access
  RAM *ram{};

  // Render handle
  Render::Renderer *render;

  // Xenos State, contains register data
  XenosState *state{};

  // Worker thread
  std::thread cpWorkerThread;

  // Worker thread running
  volatile bool cpWorkerThreadRunning = true;

  // Command Processor Worker Thread Loop
  // Whenever there's valid commands in the read/write Ptrs, this will process 
  // all commands and perform tasks associated with them
  void cpWorkerThreadLoop();

  // Software loads ME and PFP uCode
  // libXenon driver loads it and after it verifies it
  
  // CP PFP Write Address (offset)
  u32 cpPFPuCodeAddress = 0;
  // CP ME Write Address (offset)
  u32 cpMEuCodeWriteAddress = 0;
  // CP ME Read Address (offset)
  u32 cpMEuCodeReadAddress = 0;
  // CP Microcode Engine data & size
  u32 cpMEuCodeSize = 0;
  std::unordered_map<u32, u32> cpMEuCodeData;
  // CP PreFetch Parser data & size
  u32 cpPFPuCodeSize = 0;
  std::unordered_map<u32, u32> cpPFPuCodeData;
  // CP ME for PM4_ME_INIT data
  std::vector<u32> cpME_PM4_ME_INIT_Data;

  // Basically, the driver sets the CP write base to an address in memory where
  // the RingBuffer is located, and after it stores a Read Pointer to the location
  // in memory where it wants to read the data from the gpu.
  // We need to handle both pointers to memory, and also the size of the RingBuffer.
  // If both pointers are valid then only then we can start to process commands.
  // Since we don't have to deal with buffers, and simply read/write to memory, 
  // we can directly use pointers to real memory like hardware does.

  // CP RingBuffer Base Address in memory.
  std::atomic<u8*> cpRingBufferBasePtr = nullptr;
  
  // RingBuffer Size.
  std::atomic<size_t> cpRingBufferSize = 0;

  // Read/Write indexes.
  u32 cpReadPtrIndex = 0;
  std::atomic<u32> cpWritePtrIndex = 0;

  // Execute primary buffer from CP_RB_BASE.
  u32 cpExecutePrimaryBuffer(u32 readIndex, u32 writeIndex);

  // Execute indirect buffer from PM4_INDIRECT_BUFFER.
  void cpExecuteIndirectBuffer(u32 bufferPtr, u32 bufferSize);

  // Handles tiling type
  u64 binSelect = 0xFFFFFFFFULL;
  u64 binMask = 0xFFFFFFFFULL;

  // Execute a packet based on the Ringbuffer data.
  bool ExecutePacket(RingBuffer *ringBuffer);

  // Execute the different packet types.
  bool ExecutePacketType0(RingBuffer *ringBuffer, u32 packetData);
  bool ExecutePacketType1(RingBuffer *ringBuffer, u32 packetData);
  bool ExecutePacketType2(RingBuffer *ringBuffer, u32 packetData);
  bool ExecutePacketType3(RingBuffer *ringBuffer, u32 packetData);

  // Packet type 3 OpCodes definitions.
  bool ExecutePacketType3_NOP(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_INVALIDATE_STATE(RingBuffer* ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_REG_RMW(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_COND_WRITE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_EVENT_WRITE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_ME_INIT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_INTERRUPT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_CONSTANT2(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_SHADER_CONSTANTS(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_EVENT_WRITE_SHD(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_IM_LOAD(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_IM_LOAD_IMMEDIATE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_CONSTANT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_INDIRECT_BUFFER(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_WAIT_REG_MEM(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_BIN_MASK(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_BIN_SELECT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_BIN_MASK_LO(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_BIN_MASK_HI(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_BIN_SELECT_LO(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_SET_BIN_SELECT_HI(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_DRAW(RingBuffer *ringBuffer, u32 packetData, u32 dataCount, u32 vizQueryCondition, const char *opCodeName);
  bool ExecutePacketType3_DRAW_INDX(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_DRAW_INDX_2(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
  bool ExecutePacketType3_LOAD_ALU_CONSTANT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount);
};
}
