#pragma once

#include "xenonGPURegisters.h"
#include "xenonGPUState.h"
#include "Core/RAM/RAM.h"
#include "Core/PCI/Bridge/PCIBridge.h"

class CXenonGPUCommandBufferReader;
class IXenonGPUDumpWriter;
class CXenonGPUTraceWriter;

/// GPU command executor, translates GPU commands from ring/command buffer (& registers) into state changes in the abstract layer
class CXenonGPUExecutor
{
public:
	CXenonGPUExecutor( IXenonGPUAbstractLayer* abstractionLayer, RAM* ramPtr, PCIBridge* pciBridgePtr);
	~CXenonGPUExecutor();

	/// Process command from command buffer
	void Execute( CXenonGPUCommandBufferReader& reader );	

	// set internal interrupt callback
	void SetInterruptCallbackAddr( const u32 addr, const u32 userData );

	/// Signal external VBlank
	void SignalVBlank();

	/// Read register value
	u64 ReadRegister( const u32 registerIndex );

	/// Request a trace dump of the next frame
	void RequestTraceDump();

	bool doSwap() {
		// count frames
		m_swapCounter += 1;
		CXenonGPUState::SwapState ss;
		ss.m_frontBufferBase = 0;
		ss.m_frontBufferWidth = 1280;
		ss.m_frontBufferHeight = 720;
		return m_state.IssueSwap(m_abstractLayer, m_traceDumpFile, m_registers, ss);
	}

	// gpu internal register map
	CXenonGPURegisters				m_registers;
private:	
	// gpu abstraction layer (owned externally)
	IXenonGPUAbstractLayer*		m_abstractLayer;

	// gpu trace dumper
	IXenonGPUDumpWriter*		m_traceDumpFile;
	std::atomic< bool >			m_traceDumpRequested;

	// gpu log
	CXenonGPUTraceWriter*		m_logWriter;


	CXenonGPUDirtyRegisterTracker	m_registerDirtyMask;

	// gpu state management
	CXenonGPUState				m_state;

	// tiled rendering packet mask
	u64		m_tiledMask;
	u64		m_tiledSelector;

	// internal swap counter
	std::atomic< u32	>	m_swapCounter;
	std::atomic< u32	>	m_vblankCounter;

	// interrupt
	u32		m_interruptAddr;
	u32		m_interruptUserData;

	RAM* ram = nullptr;
	PCIBridge* pciBridge = nullptr;

	// command buffer execution (pasted from AMD driver)
	void ExecutePrimaryBuffer( CXenonGPUCommandBufferReader& reader );
	bool ExecutePacket( CXenonGPUCommandBufferReader& reader );

	// packet types
	bool ExecutePacketType0( CXenonGPUCommandBufferReader& reader, const u32 packetData );
	bool ExecutePacketType1( CXenonGPUCommandBufferReader& reader, const u32 packetData );
	bool ExecutePacketType2( CXenonGPUCommandBufferReader& reader, const u32 packetData );
	bool ExecutePacketType3( CXenonGPUCommandBufferReader& reader, const u32 packetData );

	// Type3 packets
	bool ExecutePacketType3_ME_INIT( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_NOP( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_INTERRUPT( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_INDIRECT_BUFFER( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_WAIT_REG_MEM( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_REG_RMW( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_COND_WRITE( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_EVENT_WRITE( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_EVENT_WRITE_SHD( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_EVENT_WRITE_EXT( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_DRAW_INDX( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_DRAW_INDX_2( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_SET_CONSTANT( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_SET_CONSTANT2( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_LOAD_ALU_CONSTANT( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_SET_SHADER_CONSTANTS( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_IM_LOAD( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_IM_LOAD_IMMEDIATE( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );
	bool ExecutePacketType3_INVALIDATE_STATE( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );

	// Hacks
	bool ExecutePacketType3_HACK_SWAP( CXenonGPUCommandBufferReader& reader, const u32 packetData, const u32 count );

	// reg access
	void WriteRegister( const u32 registerIndex, const u32 registerData );
	void WriteRegister( const XenonGPURegister registerIndex, const u32 registerData );

	// make state coherent
	void MakeCoherent();

	// waiting state
	void BeingWait();
	void FinishWait();

	// dispatch processor interrupt
	void DispatchInterrupt( const u32 source, const u32 cpu );
};