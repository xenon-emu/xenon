#pragma once

#include <atomic>
#include "Core/RAM/RAM.h"

/// Reader helper for command buffer
class CXenonGPUCommandBufferReader
{
public:
	CXenonGPUCommandBufferReader();
	CXenonGPUCommandBufferReader(const u32* bufferBase, const u32 bufferSize, const u32 readStartIndex, const u32 readEndIndex);

	/// Get we read data ?
	inline const bool CanRead() const { return m_readCount < m_readMaxCount; }

	/// Extract raw pointer, will assert if there's not enough space for read
	void GetBatch(const u32 numWords, u32* writePtr);

	/// Move command buffer
	void Advance(const u32 numWords);

	/// Read single value from command buffer (Peek + Advance(1))
	const u32 Read();

private:

	const u32* m_bufferBase; // const, absolute
	const u32 m_bufferSize; // const, in words

	const u32 m_readStartIndex;
	const u32 m_readEndIndex;

	u32 m_readIndex;
	u32 m_readCount;
	u32 m_readMaxCount;
};

/// Command buffer management for the GPU
/// NOTE: this is mapped to virtual address at 0x
class CXenonGPUCommandBuffer
{
public:
	CXenonGPUCommandBuffer();
	~CXenonGPUCommandBuffer();

	/// Initialize the ring buffer interface
	void Initialize(const void* ptr, const u32 numPages);

	/// Set the put pointer
	void AdvanceWriteIndex(const u32 newIndex);

	/// Set write back pointer (read register content will be written there)
	void SetWriteBackPointer(const u32 addr);

	/// Read word from command buffer, returns false if there's no data
	bool BeginRead(CXenonGPUCommandBufferReader& outReader);

	/// Signal completed read
	void EndRead(RAM *ram);

private:
	// command buffer initialization data
	const u32* m_commandBufferPtr;

	RAM* ram = nullptr;

	// in the whole command buffer
	u32 m_numWords; 

	// write index
	std::atomic< u32 > m_writeIndex;

	// read position, used by one thread only
	u32 m_readIndex;

	// event used to synchronize reading from command buffer
	u32 m_writeBackPtr;
};
