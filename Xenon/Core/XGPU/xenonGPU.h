#pragma once

class IXenonGPUAbstractLayer;
class CXenonGPUThread;
class CXenonGPUExecutor;

#include "xenonGPUCommandBuffer.h"
#include "Core/RAM/RAM.h"
#include "Core/PCI/Bridge/PCIBridge.h"

/// GPU emulation
class CXenonGPU
{
public:
	CXenonGPU(RAM *ramPtr, PCIBridge* pciBridgePtr);

	/// Request a trace dump of next frame
	void RequestTraceDump();

	/// Initialize, usually creates the window
	bool Initialize( const void* ptr, const u32 numPages);

	/// Close the GPU
	void Close();

	// Write stuff to GPU external register
	void WriteWord( const u32 val, const u32 addr);

	// Read stuff from GPU external register
	void ReadWord( u64* val, const u32 addr );

	// command buffer stuff
	void EnableReadPointerWriteBack( const u32 ptr, const u32 blockSize );

	// set internal interrupt callback
	void SetInterruptCallbackAddr( const u32 addr, const u32 userData );

	bool doSwapFrame();

private:
	// low level command buffer
	CXenonGPUCommandBuffer		m_commandBuffer;

	// executor of commands
	CXenonGPUExecutor*			m_executor;

	// execution thread
	CXenonGPUThread*			m_thread;

	// abstract layer
	IXenonGPUAbstractLayer*		m_abstractLayer;

	RAM* ram = nullptr;
	PCIBridge* pciBridge = nullptr;
};