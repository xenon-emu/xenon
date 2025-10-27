#include "xenonGPU.h"
#include "build.h"
#include "xenonGPU.h"
#include "xenonGPUThread.h"
#include "xenonGPUExecutor.h"

#include "dx11AbstractLayer.h"

CXenonGPU::CXenonGPU(RAM *ramPtr, PCIBridge* pciBridgePtr)
	: m_abstractLayer( nullptr )
	, m_executor( nullptr)
	, m_thread(nullptr)
	, ram(ramPtr)
	, pciBridge(pciBridgePtr)
{
	// create rendering abstraction layer
	m_abstractLayer = new CDX11AbstractLayer();

	// create execution wrapper
	m_executor = new CXenonGPUExecutor(m_abstractLayer, ram, pciBridge);
}

void CXenonGPU::RequestTraceDump()
{
	m_executor->RequestTraceDump();
}

bool CXenonGPU::Initialize( const void* ptr, const u32 numPages)
{
	// initialize command buffer
	m_commandBuffer.Initialize( ptr, numPages );

	// initialize GPU execution thread
	m_thread = new CXenonGPUThread( m_commandBuffer, *m_executor, m_abstractLayer, ram);

	// done
	printf( "GPU: Xenon GPU emulation initialized\n" );
	return true;
}

void CXenonGPU::Close()
{
	// disable interrupt calls
	extern bool GSuppressGPUInterrupts;
	GSuppressGPUInterrupts = true;

	// close thread
	if ( m_thread )
	{
		delete m_thread;
		m_thread = nullptr;
	}

	// close executor
	if ( m_executor )
	{
		delete m_executor;
		m_executor = nullptr;
	}

	// close abstract layer
	if ( m_abstractLayer )
	{
		delete m_abstractLayer;
		m_abstractLayer = nullptr;
	}	
}

void CXenonGPU::WriteWord( const u32 val, const u32 addr)
{
	const u32 regIndex = addr & 0xFFFF; // lo word

	if ( regIndex == 0x0714 ) //CP_RB_WPTR
	{
		u32 newWriteAddress = (const u32) val;
		newWriteAddress = byteswap_be<u32>(newWriteAddress);
		if (newWriteAddress >= 8100) {
			printf("Hit");
		}
		m_commandBuffer.AdvanceWriteIndex( newWriteAddress ); // this allows GPU to read stuff
	}
	else if (regIndex == 0x70C) { // Command Processor Writeback Pointer
		u32 newValue = (const u32)val;
		newValue = byteswap_be<u32>(newValue);
		m_commandBuffer.SetWriteBackPointer(newValue);
	}
	else if ( regIndex == 0x6110 )
	{
		//printf( "GPU: Unimplemented GPU external register 0x%04X, value written=0x%08X", regIndex, val );
	}
	else
	{
		//printf( "GPU: Unknown GPU external register 0x%04X, value written=0x%08X", regIndex, val );
	}

	//register_file_.values[r].u32 = static_cast<uint32_t>(value);
}

void CXenonGPU::ReadWord( u64* val, const u32 addr )
{
	const u32 regIndex = addr & 0xFFFF; // lo word
	const u64 regData = m_executor->ReadRegister( regIndex );
	*val = regData; // NO BYTESWAP SINCE THE TARGET IS A REGISTRY
}

void CXenonGPU::EnableReadPointerWriteBack( const u32 ptr, const u32 blockSize )
{
	m_commandBuffer.SetWriteBackPointer( ptr );
}

void CXenonGPU::SetInterruptCallbackAddr( const u32 addr, const u32 userData )
{
	m_executor->SetInterruptCallbackAddr( addr, userData );
}

bool CXenonGPU::doSwapFrame()
{
	return m_executor->doSwap();
}
