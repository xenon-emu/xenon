#include "build.h"
#include "xenonGPUCommandBuffer.h"

//-------------------------------------

//#define DUMP_COMMAND_BUFFER

//-------------------------------------

CXenonGPUCommandBufferReader::CXenonGPUCommandBufferReader()
	: m_bufferBase(nullptr)
	, m_bufferSize(0)
	, m_readStartIndex(0)
	, m_readEndIndex(0)
	, m_readIndex(0)
	, m_readCount(0)
	, m_readMaxCount(0)
{
}

CXenonGPUCommandBufferReader::CXenonGPUCommandBufferReader(const u32* bufferBase, const u32 bufferSize, const u32 readStartIndex, const u32 readEndIndex)
	: m_bufferBase(bufferBase)
	, m_bufferSize(bufferSize)
	, m_readStartIndex(readStartIndex)
	, m_readEndIndex(readEndIndex)
	, m_readIndex(readStartIndex)
	, m_readCount(0)

{
	auto end = m_readEndIndex;
	if (end < m_readStartIndex)
		end += m_bufferSize;

	m_readMaxCount = end - m_readStartIndex;

	DEBUG_CHECK(m_readMaxCount > 0);
	DEBUG_CHECK(bufferSize > 0);
}

const u32 CXenonGPUCommandBufferReader::Read()
{
	DEBUG_CHECK(m_readIndex != m_readEndIndex);

	std::atomic_thread_fence(std::memory_order_acq_rel);
	u32 data = 0;
	memcpy(&data, m_bufferBase + m_readIndex, sizeof(data));
	u32 ret = byteswap_be< u32 >(data);
	Advance(1);

	return ret;
}

void CXenonGPUCommandBufferReader::GetBatch(const u32 numWords, u32* writePtr)
{
	DEBUG_CHECK(m_readCount + numWords <= m_readMaxCount);

	std::atomic_thread_fence(std::memory_order_acq_rel);

	auto pos = m_readIndex;
	for (u32 index = 0; index<numWords; ++index)
	{
		// copy data
		writePtr[index] = m_bufferBase[pos]; // NOTE: no endianess swap

		// advance
		pos += 1;
		if (pos >= m_bufferSize)
			pos = pos - m_bufferSize;
	}
}

void CXenonGPUCommandBufferReader::Advance(const u32 numWords)
{
	DEBUG_CHECK(m_readCount + numWords <= m_readMaxCount);

	// advance
	m_readIndex += numWords;
	m_readCount += numWords;

	// wrap around
	if (m_bufferSize && (m_readIndex >= m_bufferSize))
		m_readIndex = m_readIndex - m_bufferSize;
}

//-------------------------------------

CXenonGPUCommandBuffer::CXenonGPUCommandBuffer()
	: m_commandBufferPtr(nullptr)
	, m_writeBackPtr(0)
{
}

CXenonGPUCommandBuffer::~CXenonGPUCommandBuffer()
{
}

void CXenonGPUCommandBuffer::Initialize(const void* ptr, const u32 numPages)
{
	// setup basics
	m_commandBufferPtr = (const u32*)ptr;
	m_numWords = 1 << (1 + numPages); // size of the command buffer

	// reset writing/reading ptr
	m_writeIndex = 0;
	m_readIndex = 0;

	printf("GPU: Command buffer initialized, ptr=0x%08X, numWords=%d\n", ptr, m_numWords);

}

void CXenonGPUCommandBuffer::AdvanceWriteIndex(const u32 newIndex)
{
	std::atomic_thread_fence(std::memory_order_acq_rel);
	u32 oldIndex = m_writeIndex.exchange(newIndex);

#ifdef DUMP_COMMAND_BUFFER
	printf( "GPU: Command buffer writeIndex=%d (delta: %d)\n", newIndex, newIndex - oldIndex );
	for ( u32 i=oldIndex; i<newIndex; ++i )
	{
		const auto memAddr = m_commandBufferPtr + i;
		u32 data = 0;
		memcpy(&data, memAddr, 4);
		data = byteswap_be<u32>(data);
		printf( "GPU: Mem[+%d, @%d]: 0x%08X\n", i-oldIndex, i, data );
	}
#endif
}

void CXenonGPUCommandBuffer::SetWriteBackPointer(const u32 addr)
{
	// CP_RB_RPTR_ADDR - Ring Buffer Read Pointer Address 
	// * Bits [0:1]: RB_RPTR_SWAP 
	// Swap control of the reported read pointer address. See 
	// CP_RB_CNTL.BUF_SWAP for the encoding.
	// * Bits [2:31]: RB_RPTR_ADDR
	// Ring Buffer Read Pointer Address. Address of the Host`s 
	// copy of the Read Pointer.CP_RB_RPTR(RO) Ring Buffer Read Pointer

	m_writeBackPtr = (addr & 0xFFFFFFFC);
	printf("GPU: Writeback pointer set to 0x%08X\n", addr);
}

bool CXenonGPUCommandBuffer::BeginRead(CXenonGPUCommandBufferReader& outReader)
{
	// capture read offset
	auto curWriteIndex = m_writeIndex.load();
	if (m_readIndex == curWriteIndex)
	{
#ifdef DUMP_COMMAND_BUFFER
		//printf("GPU: No new data @%d\n", curWriteIndex);
#endif
		return false;
	}

	// setup actual write index
	const u32 readStartIndex = m_readIndex; // prev position
	const u32 readEndIndex = curWriteIndex; // current position
#ifdef DUMP_COMMAND_BUFFER
	printf("GPU: Read started at %d, end %d\n", readStartIndex, readEndIndex);
	for (u32 i = readStartIndex; i < readEndIndex; ++i)
	{
		u32 data = 0;//cpu::mem::load<u32>(m_commandBufferPtr + i);
		memcpy(&data, m_commandBufferPtr + i, 4);
		data = byteswap_be<u32>(data);
		printf("GPU: RMem[+%d, @%d]: 0x%08X\n", i - readStartIndex, i, data);
	}
#endif
	// copy data out
	CXenonGPUCommandBufferReader commandBufferReader(m_commandBufferPtr, m_numWords, readStartIndex, readEndIndex);
	memcpy(&outReader, &commandBufferReader, sizeof(CXenonGPUCommandBufferReader));
	m_readIndex = curWriteIndex;

	// advance
	return true;
}

void CXenonGPUCommandBuffer::EndRead(RAM *ram)
{
	if (m_writeBackPtr)
	{
		u8* addr = ram->GetPointerToAddress(m_writeBackPtr);
		u32 data = byteswap_be<u32>(m_readIndex);
		memcpy(addr, &data, sizeof(u32));

#ifdef DUMP_COMMAND_BUFFER
		printf("GPU STORE at %08Xh, val %d (%08Xh)\n", m_writeBackPtr, m_readIndex, m_readIndex);
#endif
	}
}

//-------------------------------------
