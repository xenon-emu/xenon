/*
* Copyright 2025 Xenon Emulator Project. All rights reserved.
*
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

#include "CommandProcessor.h"
#include "ShaderConstants.h"
#include "Microcode/ASTBlock.h"
#include "Microcode/ASTNodeWriter.h"

#include "Base/CRCHash.h"
#include "Base/Thread.h"

#include "Render/Abstractions/Renderer.h"

namespace Xe::XGPU {

CommandProcessor::CommandProcessor(RAM *ramPtr, XenosState *statePtr, Render::Renderer *renderer, PCIBridge *pciBridge) :
  ram(ramPtr),
  state(statePtr),
#ifndef NO_GFX
  render(renderer),
#endif
  parentBus(pciBridge) {
  cpWorkerThread = std::thread(&CommandProcessor::cpWorkerThreadLoop, this);

  // According to free60/libxenon, these are the correct uCode sizes
  cpMEuCodeSize = 0x900;
  cpPFPuCodeSize = 0x120;
}

CommandProcessor::~CommandProcessor() {
  cpWorkerThreadRunning = false;
  if (cpWorkerThread.joinable()) {
    cpWorkerThread.join();
  }
}

void CommandProcessor::CPWriteMicrocodeData(eCPMicrocodeType uCodeType, u32 data) {
  switch (uCodeType) {
  case Xe::XGPU::uCodeTypeME:
    // Sanity check
    if (cpMEuCodeWriteAddress <= cpMEuCodeSize) {
      cpMEuCodeData[cpMEuCodeWriteAddress] = data;
      cpMEuCodeWriteAddress++;
    } else {
      LOG_ERROR(Xenos, "[CP] ME uCode Address '0x{:X}' is bigger than the uCode buffer", cpMEuCodeWriteAddress);
    }
    break;
  case Xe::XGPU::uCodeTypePFP:
    // Sanity check
    if (cpPFPuCodeAddress <= cpPFPuCodeSize) {
      cpPFPuCodeData[cpPFPuCodeAddress] = data;
      cpPFPuCodeAddress++;
    } else {
      LOG_ERROR(Xenos, "[CP] PFP uCode Address '0x{:X}' is bigger than the uCode buffer", cpPFPuCodeAddress);
    }
    break;
  }
}

u32 CommandProcessor::CPReadMicrocodeData(eCPMicrocodeType uCodeType) {
  u32 tmp = 0;
  switch (uCodeType) {
  case Xe::XGPU::uCodeTypeME:
    // Sanity check
    if (cpMEuCodeReadAddress < cpMEuCodeSize) {
      tmp = byteswap_be(cpMEuCodeData[cpMEuCodeReadAddress]); // Data was byteswapped, so we need to reverse that
      cpMEuCodeReadAddress++;
    }
    break;
  case Xe::XGPU::uCodeTypePFP:
    // Sanity check
    if (cpPFPuCodeAddress < cpPFPuCodeSize) {
      tmp = byteswap_be(cpPFPuCodeData[cpPFPuCodeAddress]); // Data was byteswapped, so we need to reverse that
      cpPFPuCodeAddress++;
    }
    break;
  }
  return tmp;
}

void CommandProcessor::CPUpdateRBBase(u32 address) {
  if (!address)
    return;

  cpRingBufferBasePtr = ram->GetPointerToAddress(address);
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Base Address: 0x{:X}", address);
  
  // Reset CP Read pointer index
  cpReadPtrIndex = 0;
}

void CommandProcessor::CPUpdateRBSize(size_t newSize) {
  if (!(newSize & CP_RB_CNTL_RB_BUFSZ_MASK))
    return;

  cpRingBufferSize = static_cast<size_t>(1u << (newSize + 3));
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Size: 0x{:X}", cpRingBufferSize.load());
}

void CommandProcessor::CPUpdateRBWritePointer(u32 offset) {
  cpWritePtrIndex = offset;
}

void CommandProcessor::cpWorkerThreadLoop() {
  Base::SetCurrentThreadName("[Xe] Command Processor");
  while (cpWorkerThreadRunning) {
    u32 writePtrIndex = cpWritePtrIndex.load();
    while (cpWorkerThreadRunning && (cpRingBufferBasePtr == nullptr || cpReadPtrIndex == writePtrIndex)) {
      // Stall until we're told otherwise
      std::this_thread::sleep_for(10ns);
      writePtrIndex = cpWritePtrIndex.load();
    }

    // Shutdown if we were told to
    cpWorkerThreadRunning = XeRunning;
    if (!cpWorkerThreadRunning)
      break;

    cpReadPtrIndex = cpExecutePrimaryBuffer(cpReadPtrIndex, writePtrIndex);
  }
}

u32 CommandProcessor::cpExecutePrimaryBuffer(u32 readIndex, u32 writeIndex) {
  // Create the ring buffer instance for the primary buffer.
  RingBuffer cpRingBufer(cpRingBufferBasePtr, cpRingBufferSize);

  // Set read and write offsets.
  cpRingBufer.setReadOffset(readIndex * sizeof(u32));
  cpRingBufer.setWriteOffset(writeIndex * sizeof(u32));

  do {
    if (!ExecutePacket(&cpRingBufer) && cpWorkerThreadRunning) {
      // TODO(bitsh1ft3r): Check whether this should be a fatal crash.
      LOG_ERROR(Xenos, "CP[PrimaryBuffer]: Failed to execute a packet.");
      assert(true);
      break;
    }
    // Shutdown if we were told to
    cpWorkerThreadRunning = XeRunning;
    if (!cpWorkerThreadRunning)
      break;
  } while (cpRingBufer.readCount() && cpWorkerThreadRunning);

  return writeIndex; // Set Read and Write index equal, signaling buffer processed.
}

void CommandProcessor::cpExecuteIndirectBuffer(u32 bufferPtr, u32 bufferSize) {
  // Create the ring buffer instance for the indirect buffer.
  RingBuffer ringBufer(ram->GetPointerToAddress(bufferPtr), bufferSize * sizeof(u32));

  // Set write offset.
  ringBufer.setWriteOffset(bufferSize * sizeof(u32));

  do {
    if (!ExecutePacket(&ringBufer) && cpWorkerThreadRunning) {
      // TODO(bitsh1ft3r): Check whether this should be a fatal crash.
      LOG_ERROR(Xenos, "CP[IndirectRingBuffer]: Failed to execute a packet.");
      assert(true);
      break;
    }
    // Shutdown if we were told to
    cpWorkerThreadRunning = XeRunning;
    if (!cpWorkerThreadRunning)
      break;
  } while (ringBufer.readCount() && cpWorkerThreadRunning);
  return;
}

void CommandProcessor::CPSetSQProgramCntl(u32 value) {
  state->programCntl = value;

#ifndef NO_GFX
  // Update shader hashes
  u32 vsHash = render->currentVertexShader.load();
  u32 psHash = render->currentPixelShader.load();
  
  if (vsHash && psHash) {
    render->readyToLink.store(true);
    render->pendingVertexShader = vsHash;
    render->pendingPixelShader = psHash;
  }
#endif
}

// Executes a single packet from the ringbuffer.
bool CommandProcessor::ExecutePacket(RingBuffer *ringBuffer) {
  // Get packet data.
  const u32 packetData = ringBuffer->ReadAndSwap<u32>();
  // Get the packet type.
  const CPPacketType packetType = static_cast<CPPacketType>(packetData >> 30);
  
  // TODO: Check if we should actually return if it's a nul-packet
  if (!packetData) {
    LOG_WARNING(Xenos, "CP[PrimaryBuffer]: found packet with zero data!");
    return true;
  }
  
  if (packetData == 0xCDCDCDCD) {
    LOG_WARNING(Xenos, "CP[PrimaryBuffer]: found packet with uninitialized data!");
    return true;
  }

#ifdef XE_DEBUG
  LOG_DEBUG(Xenos, "Executing packet type {} (0x{:X})", static_cast<u32>(packetType), packetData);
#endif

  // Execute packet based on type.
  switch (packetType) {
  case Xe::XGPU::CPPacketType0: return ExecutePacketType0(ringBuffer, packetData);
  case Xe::XGPU::CPPacketType1: return ExecutePacketType1(ringBuffer, packetData);
  case Xe::XGPU::CPPacketType2: return ExecutePacketType2(ringBuffer, packetData);
  case Xe::XGPU::CPPacketType3: return ExecutePacketType3(ringBuffer, packetData);
  default:
    // This should never happen.
    LOG_ERROR(Xenos, "CP[PrimaryBuffer]: found packet with unknown type!");
    return false;
    break;
  }
}

// Executes a packet type 0. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType0(RingBuffer *ringBuffer, u32 packetData) {
  const u32 regCount = ((packetData >> 16) & 0x3FFF) + 1;
  
  if (ringBuffer->readCount() < (regCount * sizeof(u32))) {
    LOG_ERROR(Xenos, "CP[ExecutePacketType0]: Data overflow, read count 0x{:X}, registers count 0x{:X})",
      ringBuffer->readCount(), regCount * sizeof(u32));
    return false;
  }

  // Base register to start from.
  const u32 baseIndex = (packetData & 0x7FFF); 
  // Tells wheter the write is to one or multiple regs starting at specified register at base index.
  const u32 singleRegWrite = (packetData >> 15) & 0x1;

  for (u64 idx = 0; cpWorkerThreadRunning && idx < regCount; idx++) {
    // Shutdown if we were told to
    cpWorkerThreadRunning = XeRunning;
    if (!cpWorkerThreadRunning)
      break;
    // Get the data to be written to the (internal) Register.
    u32 registerData = ringBuffer->Read<u32>();
    // Target register index.
    u32 targetRegIndex = singleRegWrite ? baseIndex : baseIndex + idx;
    //LOG_DEBUG(Xenos, "CP[ExecutePacketType0]: Writing to {} (0x{:X}), data 0x{:X}", Xe::XGPU::GetRegisterNameById(targetRegIndex), targetRegIndex, registerData);
    state->WriteRegister(static_cast<XeRegister>(targetRegIndex), registerData);
  }

  return true;
}

// Executes a packet type 1. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType1(Xe::XGPU::RingBuffer *ringBuffer, u32 packetData) {
  // Get both registers index.
  const u32 regIndex0 = packetData & 0x7FF;
  const u32 regIndex1 = (packetData >> 11) & 0x7FF;
  // Get both registers data.
  const u32 reg0Data = ringBuffer->Read<u32>();
  const u32 reg1Data = ringBuffer->Read<u32>();
  // Do the write.
  LOG_TRACE(Xenos, "CP[ExecutePacketType1]: Writing register at index 0x{:X}, data 0x{:X}", regIndex0, reg0Data);
  LOG_TRACE(Xenos, "CP[ExecutePacketType1]: Writing register at index 0x{:X}, data 0x{:X}", regIndex1, reg1Data);
  // Write registers.
  state->WriteRegister(static_cast<XeRegister>(regIndex0), reg0Data);
  state->WriteRegister(static_cast<XeRegister>(regIndex1), reg1Data);
  return true;
}

// Executes a packet type 2. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType2(RingBuffer *ringBuffer, u32 packetData) {
  // Type 2 is a No-op. :)
  return true;
}

// Executes a packet type 3. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType3(RingBuffer *ringBuffer, u32 packetData) {
  // Current opcode we're executing.
  const CPPacketType3Opcode currentOpCode = static_cast<CPPacketType3Opcode>((packetData >> 8) & 0x7F);
  // Amount of data we're reading.
  const u32 dataCount = ((packetData >> 16) & 0x3FFF) + 1;
  // Get the read offset.
  auto data_start_offset = ringBuffer->readOffset();
  
  if (ringBuffer->readCount() < dataCount * sizeof(u32)) {
    LOG_ERROR(Xenos, "CP[ExecutePacketType3]: Data overflow, read count 0x{:X}, registers count 0x{:X})",
      ringBuffer->readCount(), dataCount * sizeof(u32));
    return false;
  }

  // Info from Xenia devs:
  // & 1 == predicate. When set, we do bin check to see if we should execute
  // the packet. Only type 3 packets are affected.
  // We also skip predicated swaps, as they are never valid (probably?).
  if (packetData & 1) {
    bool anyPass = (binSelect & binMask) != 0;
    if (!anyPass) {
      ringBuffer->AdvanceRead(dataCount * sizeof(u32));
      return true;
    }
  }

  bool result = false;

#ifdef XE_DEBUG
  LOG_DEBUG(Xenos, "CP[ExecutePacketType3]: Executing {}", GetPM4Opcode(static_cast<u8>(currentOpCode)));
#endif

  // PM4 Commands execution, basically the heart of the command processor.

  switch (currentOpCode) {
  case Xe::XGPU::PM4_NOP:
    result = ExecutePacketType3_NOP(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_REG_RMW:
    result = ExecutePacketType3_REG_RMW(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_DRAW_INDX:
    result = ExecutePacketType3_DRAW_INDX(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_IM_LOAD:
    result = ExecutePacketType3_IM_LOAD(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_IM_LOAD_IMMEDIATE:
    result = ExecutePacketType3_IM_LOAD_IMMEDIATE(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_CONSTANT:
    result = ExecutePacketType3_SET_CONSTANT(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_DRAW_INDX_2:
    result = ExecutePacketType3_DRAW_INDX_2(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_INDIRECT_BUFFER:
  case Xe::XGPU::PM4_INDIRECT_BUFFER_PFD:
    result = ExecutePacketType3_INDIRECT_BUFFER(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_INVALIDATE_STATE:
    result = ExecutePacketType3_INVALIDATE_STATE(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_WAIT_REG_MEM:
    result = ExecutePacketType3_WAIT_REG_MEM(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_COND_WRITE:
    result = ExecutePacketType3_COND_WRITE(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_EVENT_WRITE:
    result = ExecutePacketType3_EVENT_WRITE(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_ME_INIT:
    result = ExecutePacketType3_ME_INIT(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_MASK:
    result = ExecutePacketType3_SET_BIN_MASK(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT:
    result = ExecutePacketType3_SET_BIN_SELECT(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_INTERRUPT:
    result = ExecutePacketType3_INTERRUPT(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_CONSTANT2:
    result = ExecutePacketType3_SET_CONSTANT2(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_SHADER_CONSTANTS:
    result = ExecutePacketType3_SET_SHADER_CONSTANTS(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_EVENT_WRITE_SHD:
    result = ExecutePacketType3_EVENT_WRITE_SHD(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_MASK_LO:
    result = ExecutePacketType3_SET_BIN_MASK_LO(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_MASK_HI:
    result = ExecutePacketType3_SET_BIN_MASK_HI(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT_LO:
    result = ExecutePacketType3_SET_BIN_SELECT_LO(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT_HI:
    result = ExecutePacketType3_SET_BIN_SELECT_HI(ringBuffer, packetData, dataCount);
    break;
  default:
    LOG_ERROR(Xenos, "[CP]: Packet3 unimplemented opcode: {}", GetPM4Opcode(static_cast<u8>(currentOpCode)));
    break;
  }

  return result;
}

bool CommandProcessor::ExecutePacketType3_NOP(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Skips N 32-bit words to get to the next packet.
  ringBuffer->AdvanceRead(dataCount * sizeof(u32));
  return true;
}

bool CommandProcessor::ExecutePacketType3_REG_RMW(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 rmwInfo = ringBuffer->ReadAndSwap<u32>();
  const u32 andMask = ringBuffer->ReadAndSwap<u32>();
  const u32 orMask = ringBuffer->ReadAndSwap<u32>();

  const u32 regAddr = (rmwInfo & 0x1FFF);
  u32 value = state->ReadRawRegister(regAddr);
  const u32 oldValue = value;

  // OR value (with reg or immediate value)
  if ((rmwInfo >> 30) & 0x1) {
    // | reg
    const u32 orValue = state->ReadRawRegister(orMask & 0x1FFF);
    value |= orValue;
  } else {
    // | imm
    value |= orMask;
  }

  // AND value (with reg or immediate value)
  if ((rmwInfo >> 31) & 0x1) {
    // & reg
    const u32 andValue = state->ReadRawRegister(andMask & 0x1FFF);
    value &= andValue;
  } else {
    // & imm  
    value &= andMask;
  }

  // Write the value back
  state->WriteRawRegister(regAddr, value);
  return true;
}

bool CommandProcessor::ExecutePacketType3_EVENT_WRITE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Generates an event that creates a write to memory when completed
  const u32 initiator = ringBuffer->ReadAndSwap<u32>();

  // Writeback
  state->vgtDrawInitiator.hexValue = initiator & 0x3F;

  if (dataCount == 1) {
    // Unknown what should be done here
  } else {
    LOG_ERROR(Xenos, "CP[EP3] | EVENT_WRITE: Invalid type!");
    ringBuffer->AdvanceRead((dataCount - 1) * sizeof(u32));
  }

  return true;
}


bool CommandProcessor::ExecutePacketType3_COND_WRITE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Determines how long to wait for, and what to wait for
  const u32 waitInfo = ringBuffer->ReadAndSwap<u32>();
  const XeRegister pollReg = static_cast<XeRegister>(ringBuffer->ReadAndSwap<u32>());
  const u32 ref = ringBuffer->ReadAndSwap<u32>();
  const u32 mask = ringBuffer->ReadAndSwap<u32>();
  // Write data
  const XeRegister writeReg = static_cast<XeRegister>(ringBuffer->ReadAndSwap<u32>());
  u32 writeData = ringBuffer->ReadAndSwap<u32>();

  u32 value = 0;
  if (waitInfo & 0x10) {
    // Memory.
    auto endianness = static_cast<eEndian>(static_cast<u32>(pollReg) & 0x3);
    u8 *addrPtr = ram->GetPointerToAddress(static_cast<u32>(pollReg) & ~0x3);
    memcpy(&value, addrPtr, sizeof(value));
    value = xeEndianSwap(value, endianness);
  } else {
    value = state->ReadRegister(pollReg);
  }
  bool matched = false;
  switch (waitInfo & 0x7) {
  case 0: // Never
    matched = false;
    break;
  case 1: // Less than reference
    matched = (value & mask) < ref;
    break;
  case 2: // Less than or equal to reference
    matched = (value & mask) <= ref;
    break;
  case 3: // Equal to reference
    matched = (value & mask) == ref;
    break;
  case 4: // Not equal to reference
    matched = (value & mask) != ref;
    break;
  case 5: // Greater than or equal to reference
    matched = (value & mask) >= ref;
    break;
  case 6: // Greater than reference
    matched = (value & mask) > ref;
    break;
  case 7: // Always
    matched = true;
    break;
  }

  if (matched) {
    // Write.
    if (waitInfo & 0x100) { // Memory
      auto endianness = static_cast<eEndian>(static_cast<u32>(writeReg) & 0x3);
      u8 *addrPtr = ram->GetPointerToAddress(static_cast<u32>(writeReg) & ~0x3);
      writeData = xeEndianSwap(writeData, endianness);
      memcpy(addrPtr, &writeData, sizeof(writeData));
    } else { // Register
      state->WriteRegister(writeReg, writeData);
    }
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_INVALIDATE_STATE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 stateMask = ringBuffer->ReadAndSwap<u32>();
  return true;
}

bool CommandProcessor::ExecutePacketType3_ME_INIT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Initializes Command Processor's ME.
  cpME_PM4_ME_INIT_Data.clear();
  for (size_t i = 0; i < dataCount; i++) {
    cpME_PM4_ME_INIT_Data.push_back(ringBuffer->ReadAndSwap<u32>());
  }
  return true;
}

class GlobalInstructionExtractor : public Microcode::AST::ExpressionNode::Visitor {
public:
  virtual void OnExprStart(Microcode::AST::ExpressionNode::Ptr n) override final {
    if (n->GetType() == Microcode::AST::eExprType::VFETCH) {
      vfetch.push_back(static_cast<Microcode::AST::VertexFetch::Ptr>(n));
    }
    else if (n->GetType() == Microcode::AST::eExprType::TFETCH) {
      tfetch.push_back(static_cast<Microcode::AST::TextureFetch::Ptr>(n));
    }
    else if (n->GetType() == Microcode::AST::eExprType::EXPORT) {
      exports.push_back(static_cast<Microcode::AST::WriteExportRegister::Ptr>(n));
    }
    else {
      const s32 regIndex = n->GetRegisterIndex();
      if (regIndex != -1) {
        usedRegisters.insert((u32)regIndex);
      }
    }
  }

  virtual void OnExprEnd(Microcode::AST::ExpressionNode::Ptr n) override final
  {}

  std::vector<Microcode::AST::VertexFetch::Ptr> vfetch;
  std::vector<Microcode::AST::TextureFetch::Ptr> tfetch;
  std::vector<Microcode::AST::WriteExportRegister::Ptr> exports;
  std::set<u32> usedRegisters;
};

class AllExpressionVisitor : public Microcode::AST::StatementNode::Visitor {
public:
  AllExpressionVisitor(Microcode::AST::ExpressionNode::Visitor *vistor)
    : exprVisitor(vistor)
  {}

  virtual void OnWrite(Microcode::AST::ExpressionNode::Ptr dest, Microcode::AST::ExpressionNode::Ptr src, std::array<eSwizzle, 4> mask) override final {
    dest->Visit(*exprVisitor);
    src->Visit(*exprVisitor);
  }
  
  virtual void OnConditionPush(Microcode::AST::ExpressionNode::Ptr condition) override final {
    condition->Visit(*exprVisitor);
  }

  virtual void OnConditionPop() override final
  {}

private:
  Microcode::AST::ExpressionNode::Visitor *exprVisitor;
};

void VisitAll(const Microcode::AST::Block *b, Microcode::AST::StatementNode::Visitor &v) {
  if (b->GetCondition())
    v.OnConditionPush(b->GetCondition()->shared_from_this());

  if (b->GetCode())
    b->GetCode()->Visit(v);

  if (b->GetCondition())
    v.OnConditionPop();
}

void VisitAll(const Microcode::AST::ControlFlowGraph *cf, Microcode::AST::StatementNode::Visitor &v) {
  for (u32 i = 0; i < cf->GetNumBlocks(); ++i) {
    const Microcode::AST::Block *b = cf->GetBlock(i);
    VisitAll(b, v);
  }
}

std::pair<Microcode::AST::Shader*, std::vector<u32>> LoadShader(eShaderType shaderType, const std::vector<u32> &data, std::string baseString) {
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "cache" };
  fs::path path{ shaderPath / (baseString + ".spv") };
  std::vector<u32> code{};
  // Vali: Temporarily disable cache, emitting isn't 100% yet
  // Plus, I need to figure Shader ptr out
  /*{
    std::ifstream file{ path, std::ios::in | std::ios::binary };
    std::error_code error;
    if (fs::exists(path, error) && file.is_open()) {
      u64 fileSize = fs::file_size(path);
      code.resize(fileSize / 4);
      file.read(reinterpret_cast<char*>(code.data()), fileSize);
      file.close();
      return code;
    }
    file.close();
  }*/
  Microcode::AST::Shader *shader = Microcode::AST::Shader::DecompileMicroCode(reinterpret_cast<const u8*>(data.data()), data.size() * 4, shaderType);
#ifndef NO_GFX
  Microcode::AST::ShaderCodeWriterSirit writer{ shaderType };
  if (shader) {
    shader->EmitShaderCode(writer);
  }
  code = writer.module.Assemble();
  std::ofstream f{ shaderPath / (baseString + ".spv"), std::ios::out | std::ios::binary };
  f.write(reinterpret_cast<char*>(code.data()), code.size() * 4);
  f.close();
#endif
  return { shader, code };
}

bool CommandProcessor::ExecutePacketType3_IM_LOAD(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Load sequencer instruction memory (pointer-based)
  const u32 addrType = ringBuffer->ReadAndSwap<u32>();
  const eShaderType shaderType = static_cast<eShaderType>(addrType & 0x3);
  const u32 addr = addrType & ~0x3;
  const u32 startSize = ringBuffer->ReadAndSwap<u32>();
  const u32 start = startSize >> 16;
  const u64 size = (startSize & 0xFFFF) * 4;
  u8 *addrPtr = ram->GetPointerToAddress(addr);
  LOG_DEBUG(Xenos, "[CP::IM_LOAD] Shader Address: 0x{:X} | Shader Size: 0x{:X} (0x{:X}, 0x{:X})", addr, startSize, start, size);

  std::vector<u32> data{};
  u32 dwordCount = size / 4;
  data.resize(dwordCount);
  memcpy(data.data(), addrPtr, size);
  for (u32 &value : data) { 
    value = byteswap_be(value);
  }
  
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "cache" };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  u32 crc = CRC32::CRC32::calc(reinterpret_cast<const u8 *>(data.data()), data.size() * 4);
  std::string baseString = fmt::format("{}_shader_{:X}", typeString, crc);
  {
    std::ofstream f{ shaderPath / (baseString + ".bin"), std::ios::out | std::ios::binary };
    f.write(reinterpret_cast<char *>(data.data()), data.size() * 4);
    f.close();
  }

  std::pair<Microcode::AST::Shader*, std::vector<u32>> shader = LoadShader(shaderType, data, baseString);

  switch (shaderType) {
  case eShaderType::Pixel:{
#ifndef NO_GFX
    render->pendingPixelShaders[crc] = shader;
    render->currentPixelShader.store(crc);
#endif
    LOG_DEBUG(Xenos, "[CP::IM_LOAD] PixelShader CRC: 0x{:08X}", crc);
  } break;
  case eShaderType::Vertex:{
#ifndef NO_GFX
    render->pendingVertexShaders[crc] = shader;
    render->currentVertexShader.store(crc);
#endif
    LOG_DEBUG(Xenos, "[CP::IM_LOAD] VertexShader CRC: 0x{:08X}", crc);
  } break;
  case eShaderType::Unknown:
  default: {
    LOG_WARNING(Xenos, "[CP::IM_LOAD] Unknown shader type '{}'", static_cast<u32>(shaderType));
  } break;
  }
#ifndef NO_GFX
  {
    std::lock_guard<std::mutex> lock(render->frameReadyMutex);
    render->frameReady = true;
  }
  render->frameReadyCondVar.notify_one();
#endif

  return true;
}

bool CommandProcessor::ExecutePacketType3_IM_LOAD_IMMEDIATE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Load sequencer instruction memory (pointer-based)
  const u32 shaderTypeValue = ringBuffer->ReadAndSwap<u32>();
  const eShaderType shaderType = static_cast<eShaderType>(shaderTypeValue);
  const u32 startSize = ringBuffer->ReadAndSwap<u32>();
  const u32 start = startSize >> 16;
  const u64 sizeDwords = (startSize & 0xFFFF);
  
  std::vector<u32> data{};
  data.resize(sizeDwords);
  ringBuffer->Read(data.data(), data.size());
  for (u32 &value : data) {
    value = byteswap_be(value);
  }

  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "cache" };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  u32 crc = CRC32::CRC32::calc(reinterpret_cast<const u8 *>(data.data()), data.size() * 4);
  std::string baseString = fmt::format("{}_shader_{:X}", typeString, crc);
  {
    std::ofstream f{ shaderPath / (baseString + ".bin"), std::ios::out | std::ios::binary };
    f.write(reinterpret_cast<char *>(data.data()), data.size() * 4);
    f.close();
  }
  
  std::pair<Microcode::AST::Shader*, std::vector<u32>> shader = LoadShader(shaderType, data, baseString);

  switch (shaderType) {
  case eShaderType::Pixel:{
#ifndef NO_GFX
    render->pendingPixelShaders[crc] = shader;
    render->currentPixelShader.store(crc);
#endif
    LOG_DEBUG(Xenos, "[CP::IM_LOAD_IMMEDIATE] PixelShader CRC: 0x{:08X}", crc);
  } break;
  case eShaderType::Vertex:{
#ifndef NO_GFX
    render->pendingVertexShaders[crc] = shader;
    render->currentVertexShader.store(crc);
#endif
    LOG_DEBUG(Xenos, "[CP::IM_LOAD_IMMEDIATE] VertexShader CRC: 0x{:08X}", crc);
  } break;
  case eShaderType::Unknown:
  default: {
    LOG_WARNING(Xenos, "[CP::IM_LOAD_IMMEDIATE] Unknown shader type '{}'", static_cast<u32>(shaderType));
  } break;
  }
#ifndef NO_GFX
  {
    std::lock_guard<std::mutex> lock(render->frameReadyMutex);
    render->frameReady = true;
  }
  render->frameReadyCondVar.notify_one();
#endif

  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_CONSTANT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Get base index
  const u32 offsetType = ringBuffer->ReadAndSwap<u32>();
  // PM4_REG(reg) ((0x4 << 16) | (GSL_HAL_SUBBLOCK_OFFSET(reg)))
  u32 index = offsetType & 0x7FF;
  const u32 type = (offsetType >> 16) & 0xFF;
  switch (type) {
  // ALU
  case 0: index += 0x4000; break;
  // FETCH
  case 1: index += 0x4800; break;
  // BOOL
  case 2: index += 0x4900; break;
  // LOOP
  case 3: index += 0x4908; break;
  // REGISTER_RAWS
  case 4: index += 0x2000; break;
  default: ringBuffer->AdvanceRead((dataCount - 1) * sizeof(u32)); return true; break;
  }

  // Write constants
  for (u32 n = 0; n < dataCount - 1; n++, index++) {
    const u32 data = ringBuffer->ReadAndSwap<u32>();
    state->WriteRegister(static_cast<XeRegister>(index), data);
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_INDIRECT_BUFFER(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Indirect buffer.
  const u32 bufferPtr = ringBuffer->ReadAndSwap<u32>();
  // Get the list pointer.
  u32 bufferSize = ringBuffer->ReadAndSwap<u32>();
  bufferSize &= 0xFFFFF;

  // Execute indirect buffer.
  LOG_TRACE(Xenos, "CP[IndirectBuffer]: Executing indirect buffer at address 0x{:X}, size 0x{:X}", bufferPtr, bufferSize);
  cpExecuteIndirectBuffer(bufferPtr, bufferSize);
  return true;
}

bool CommandProcessor::ExecutePacketType3_INTERRUPT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // CPU(s) to interrupt
  const u32 cpuMask = ringBuffer->ReadAndSwap<u32>();
  LOG_DEBUG(Xenos, "[CP]: Executing Packet3 XPS INTERRUPT. CPU Mask {:#x}", cpuMask);
  std::this_thread::sleep_for(100ms);
  parentBus->RouteInterrupt(PRIO_XPS, cpuMask);
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_CONSTANT2(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Get base index
  const u32 offsetType = ringBuffer->ReadAndSwap<u32>();
  u32 index = offsetType & 0xFFFF;

  // Write constants
  for (u32 n = 0; n < dataCount - 1; n++, index++) {
    const u32 data = ringBuffer->ReadAndSwap<u32>();
    state->WriteRegister(static_cast<XeRegister>(index), data);
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_SHADER_CONSTANTS(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Get base index
  const u32 offsetType = ringBuffer->ReadAndSwap<u32>();
  u32 index = offsetType & 0xFFFF;

  // Write constants
  for (u32 n = 0; n < dataCount - 1; n++, index++) {
    const u32 data = ringBuffer->ReadAndSwap<u32>();
    state->WriteRegister(static_cast<XeRegister>(index), data);
  }
  return true;
}

bool CommandProcessor::ExecutePacketType3_EVENT_WRITE_SHD(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Generates a VS|PS_done event
  const u32 initiator = ringBuffer->ReadAndSwap<u32>();
  u32 address = ringBuffer->ReadAndSwap<u32>();
  const u32 value = ringBuffer->ReadAndSwap<u32>();

  // Writeback
  state->vgtDrawInitiator.hexValue = initiator & 0x3F;

#ifndef NO_GFX
  {
    std::lock_guard<std::mutex> lock(render->frameReadyMutex);
    render->frameReady = true;
  }
  render->frameReadyCondVar.notify_one();
#endif
  u32 writeValue = 0;
  if ((initiator >> 31) & 0x1) {
#ifndef NO_GFX
    writeValue = render->swapCount;
#else
    writeValue = value;
#endif
  } else {
    writeValue = value;
  }

  auto endianness = static_cast<eEndian>(address & 0x3);
  address &= ~0x3;
  writeValue = xeEndianSwap(writeValue, endianness);

  LOG_DEBUG(Xenos, "[CP][PT3](EVENT_WRITE_SHD): Writing value {:#x} to address {:#x}", writeValue, address);

  u8 *addrPtr = ram->GetPointerToAddress(address);
  memcpy(addrPtr, &writeValue, sizeof(writeValue));

  return true;
}

bool CommandProcessor::ExecutePacketType3_WAIT_REG_MEM(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Determines how long to wait for, and what to wait for
  const u32 waitInfo = ringBuffer->ReadAndSwap<u32>();
  const u32 pollReg = ringBuffer->ReadAndSwap<u32>();
  const u32 ref = ringBuffer->ReadAndSwap<u32>();
  const u32 mask = ringBuffer->ReadAndSwap<u32>();
  // Time to live
  const u32 wait = ringBuffer->ReadAndSwap<u32>();

  const bool isMemory = (waitInfo & 0x10) != 0;

  bool matched = false;
  do {
    u32 value = 0;
    if (isMemory) {
      u32 addr = pollReg & ~0x3;
      u8 *addrPtr = ram->GetPointerToAddress(addr);
      memcpy(&value, addrPtr, sizeof(value));
      value = xeEndianSwap(value, static_cast<eEndian>(pollReg & 0x3));
    } else {
      value = state->ReadRegister(static_cast<XeRegister>(pollReg));
      if (static_cast<XeRegister>(pollReg) == XeRegister::COHER_STATUS_HOST) {
        auto statusHost = state->coherencyStatusHost;
        const u32 baseHost = state->coherencyBaseHost;
        const u32 sizeHost = state->coherencySizeHost;

        if (statusHost.status) {
          const char* action = "N/A";
          if (statusHost.vcActionEnable && statusHost.tcActionEnable) { action = "VC | TC"; } 
          else if (statusHost.tcActionEnable) { action = "TC"; } 
          else if (statusHost.vcActionEnable) { action = "VC"; }

          LOG_DEBUG(Xenos, "[CP]: Making {:#x} -> {:#x} coherent, performed action = {}", baseHost, baseHost + sizeHost, action);
          state->coherencyStatusHost.hexValue = 0;
        }

        value = state->ReadRegister(static_cast<XeRegister>(pollReg));
      }
    }
    switch (waitInfo & 0x7) {
    case 0: // Never
      matched = false;
      break;
    case 1: // Less than reference
      matched = (value & mask) < ref;
      break;
    case 2: // Less than or equal to reference
      matched = (value & mask) <= ref;
      break;
    case 3: // Equal to reference
      matched = (value & mask) == ref;
      break;
    case 4: // Not equal to reference
      matched = (value & mask) != ref;
      break;
    case 5: // Greater than or equal to reference
      matched = (value & mask) >= ref;
      break;
    case 6: // Greater than reference
      matched = (value & mask) > ref;
      break;
    case 7: // Always
      matched = true;
      break;
    }

    if (!matched) {
#ifndef NO_GFX
      render->waiting = true;
      render->waitTime = wait;
#endif
      if (wait >= 0x100) {
        // Wait
        std::this_thread::sleep_for(std::chrono::milliseconds(wait / 0x100));
      } else {
        // Yield
        std::this_thread::yield();
      }
    }
  } while (!matched);
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_MASK(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const uint64_t maskHigh = ringBuffer->ReadAndSwap<u32>();
  const uint64_t maskLow = ringBuffer->ReadAndSwap<u32>();
  binMask = (maskHigh << 32) | maskLow;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_SELECT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const uint64_t selectHigh = ringBuffer->ReadAndSwap<u32>();
  const uint64_t selectlow = ringBuffer->ReadAndSwap<u32>();
  binSelect = (selectHigh << 32) | selectlow;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_MASK_LO(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 loMask = ringBuffer->ReadAndSwap<u32>();
  binMask = (binMask & 0xFFFFFFFF00000000ull) | loMask;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_MASK_HI(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 hiMask = ringBuffer->ReadAndSwap<u32>();
  binMask = (binMask & 0xFFFFFFFFull) | (static_cast<u64>(hiMask) << 32);
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_SELECT_LO(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 loSelect = ringBuffer->ReadAndSwap<u32>();
  binSelect = (binSelect & 0xFFFFFFFF00000000ull) | loSelect;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_SELECT_HI(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 hiSelect = ringBuffer->ReadAndSwap<u32>();
  binSelect = (binSelect & 0xFFFFFFFFull) | (static_cast<u64>(hiSelect) << 32);
  return true;
}

bool CommandProcessor::ExecutePacketType3_DRAW(RingBuffer *ringBuffer, u32 packetData, u32 dataCount, u32 vizQueryCondition, const char *opCodeName) {
  // Sanity check.
  if (!dataCount) {
    LOG_ERROR(Xenos, "[CP, PT3]: Packet too small, can't read VGT_DRAW_INITIATOR");
    return false;
  }

  // Get our VGT register data
  state->vgtDrawInitiator.hexValue = ringBuffer->ReadAndSwap<u32>();
  dataCount--;

  bool isIndexedDraw = false;
  bool drawOk = true;

  // Our Index Buffer info for indexed draws
  XeIndexBufferInfo indexBufferInfo;

  switch (state->vgtDrawInitiator.sourceSelect) {
  case eSourceSelect::xeDMA: {
    // Indexed draw
    isIndexedDraw = true;

    // Read VGT_DMA_BASE from data
    // Sanity check
    if (!dataCount) {
      LOG_ERROR(Xenos, "[CP, PT3]: DRAW failed, not enough data for VGT_DMA_BASE.");
      return false; // Failed
    }

    // Write the VGT_DMA_BASE register
    state->vgtDMABase = ringBuffer->ReadAndSwap<u32>();
    dataCount--;

    // Sanity check, again
    if (!dataCount) {
      LOG_ERROR(Xenos, "[CP, PT3]: DRAW failed, not enough data for VGT_DMA_SIZE.");
      return false; // Failed
    }

    // Write the VGT_DMA_SIZE register
    state->vgtDMASize.hexValue = ringBuffer->ReadAndSwap<u32>();
    dataCount--;

    // Get our index size from VGT_DRAW_INITIATOR size in bytes.
    u32 indexSizeInBytes = state->vgtDrawInitiator.indexSize == eIndexFormat::xeInt16 ? sizeof(u16) : sizeof(u32);

    // The base address must already be word-aligned according to the R6xx documentation.
    indexBufferInfo.guestBase = state->vgtDMABase & ~(indexSizeInBytes - 1);
    indexBufferInfo.elements = ram->GetPointerToAddress(indexBufferInfo.guestBase);
    indexBufferInfo.endianness = state->vgtDMASize.swapMode;
    indexBufferInfo.indexFormat = state->vgtDrawInitiator.indexSize;
    indexBufferInfo.length = state->vgtDMASize.numWords * indexSizeInBytes;
    indexBufferInfo.count = state->vgtDrawInitiator.numIndices;
  } break;
  case eSourceSelect::xeImmediate: {
    // TODO(bitshift3r): Do VGT_IMMED_DATA if any ocurrences are to be found.
    LOG_ERROR(Xenos, "[CP, PT3]{}: Is using immediate vertex indices, which are currently unsupported. "
      "Please submit an issue in Xenon github.", opCodeName);
    drawOk = false;
  } break;
  case eSourceSelect::xeAutoIndex: {
    // Auto draw.
    indexBufferInfo.guestBase = 0;
    indexBufferInfo.length = 0;
  } break;
  default: {
    // Unhandled or invalid source selection.
    drawOk = false;
    LOG_ERROR(Xenos, "[CP, PT3]: DRAW failed, invalid source select.");
  } break;
  }

  // Skip to the next command, if there are immediate indexes that we don't support yet.
  ringBuffer->AdvanceRead(dataCount * sizeof(u32));

  const eModeControl modeControl = state->modeControl.edramMode;
  if (drawOk) {
    // Get surface info
    const u32 surfacePitch = state->surfaceInfo.surfacePitch;
    bool hasRT = surfacePitch != 0;
    if (!hasRT) {
      return true;
    }
    // Get surface MSAA
    const eMSAASamples surfaceMSAA = state->surfaceInfo.msaaSamples;
    // Check the state of things
    if (modeControl == eModeControl::xeCopy) {
      // Copy to eDRAM, and clear if needed
#ifndef NO_GFX
      {
        std::lock_guard<std::mutex> lock(render->copyQueueMutex);
        render->copyQueue.push(state);
      }
#endif
      {
        std::lock_guard<std::mutex> lock(render->frameReadyMutex);
        render->frameReady = true;
      }
      render->frameReadyCondVar.notify_one(); // Wake up the renderer
      return true;
    }
    {
#ifndef NO_GFX
      std::lock_guard<std::mutex> lock(render->drawQueueMutex);
      u64 combinedShaderHash = (static_cast<u64>(render->currentVertexShader.load()) << 32) | render->currentPixelShader.load();
#endif
      XeDrawParams params = {};
      params.state = state;
      params.indexBufferInfo = indexBufferInfo;
      params.vgtDrawInitiator = state->vgtDrawInitiator;
      params.maxVertexIndex = state->maxVertexIndex;
      params.minVertexIndex = state->minVertexIndex;
      params.indexOffset = state->indexOffset;
      params.multiPrimitiveIndexBufferResetIndex = state->multiPrimitiveIndexBufferResetIndex;
      params.currentBinIdMin = state->currentBinIdMin;
#ifndef NO_GFX
      Render::DrawJob drawJob = {};
      drawJob.params = params;
      drawJob.indexed = isIndexedDraw;
      drawJob.shaderPS = render->currentPixelShader.load();
      drawJob.shaderVS = render->currentVertexShader.load();
      drawJob.shaderHash = combinedShaderHash;
      // Queue off to the Renderer
      render->drawQueue.push(drawJob);
#endif
    }
#ifndef NO_GFX
    LOG_DEBUG(Xenos, "[CP] Draw {}: PrimType {}, IndexCount {}, VS: 0x{:X}, PS: 0x{:X}",
      isIndexedDraw ? "Indexed" : "Auto",
      (u32)state->vgtDrawInitiator.primitiveType,
      state->vgtDrawInitiator.numIndices,
      render->currentVertexShader.load(),
      render->currentPixelShader.load());
#else
    LOG_DEBUG(Xenos, "[CP] Draw {}: PrimType {}, IndexCount {}",
      isIndexedDraw ? "Indexed" : "Auto",
      (u32)state->vgtDrawInitiator.primitiveType,
      state->vgtDrawInitiator.numIndices);
#endif
#ifndef NO_GFX
    {
      std::lock_guard<std::mutex> lock(render->frameReadyMutex);
      render->frameReady = true;
    }
    render->frameReadyCondVar.notify_one();
#endif
    state->ClearDirtyState();
  } else {
    LOG_ERROR(Xenos, "[CP] Invalid draw");
  }

  return drawOk;
}

bool CommandProcessor::ExecutePacketType3_DRAW_INDX(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Initiate fetch of index buffer and draw.
  // Generally used by Xbox 360 Direct3D 9 for kDMA and kAutoIndex sources.
  // With a VIZ Query token as the first one.
  
  u32 dataCountRemaining = dataCount;

  if (!dataCountRemaining) {
    LOG_ERROR(Xenos, "[CP,PT3]: PM4_DRAW_INDX: Packet too small, can't read the VIZ Query token.");
    return false;
  }

  // Get our VIZ Query Conditon.
  u32 vizQueryCondition = ringBuffer->ReadAndSwap<u32>();
  --dataCountRemaining;

  return ExecutePacketType3_DRAW(ringBuffer, packetData, dataCountRemaining, vizQueryCondition, "PM4_DRAW_INDX");
}

bool CommandProcessor::ExecutePacketType3_DRAW_INDX_2(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Draw using supplied indices in packet.
  // Generally used by Xbox 360 Direct3D 9 for kAutoIndex source.
  // No VIZ query token.

  return ExecutePacketType3_DRAW(ringBuffer, packetData, dataCount, 0, "PM4_DRAW_INDX_2");
}

} // namespace Xe::XGPU
