#pragma once

/// Format of GPU dump file
class CXenonGPUDumpFormat
{
public:
#pragma pack( push, 4 )
	struct Block
	{
		char		m_tag[16];				// block tag (primary, indirect, etc)

		u32		m_firstSubBlock;		// first sub block
		u32		m_numSubBlocks;			// number of sub blocks

		u32		m_firstPacket;			// first packed executed in this block
		u32		m_numPackets;			// number of packets executed in this block
	};

	struct Packet
	{
		u32		m_packetData;			// packet data itself
		u32		m_firstDataWord;		// first extra data word
		u32		m_numDataWords;			// number of extra data words

		u32		m_firstMemoryRef;		// first reference to memory block used by this command
		u32		m_numMemoryRefs;		// number of memory references
	};

	struct MemoryRef
	{
		u32		m_blockIndex;			// index to actual memory block
		u32		m_mode;					// 0-read, 1-write
		char		m_tag[16];				// memory tag (texture, shader, etc)
	};

	struct Memory
	{
		u64		m_crc;					// crc of the memory block
		u64		m_fileOffset;			// offset in file where the memory is
		u32		m_address;				// address where to put the data
		u32		m_size;					// size of the data
	};

	struct Header
	{
		u32		m_magic;				// file magic number
		u32		m_version;				// file version number
					
		u32		m_numBlocks;			// number of blocks in the file
		u64		m_blocksOffset;			// offset to packet blocks

		u32		m_numPackets;			// number of packets in the file
		u64		m_packetsOffset;		// offset to packet data

		u32		m_numMemoryRefs;		// number of memory refs in the file
		u64		m_memoryRefsOffset;		// offset to memory refs

		u32		m_numMemoryBlocks;		// number of memory blocks in the file
		u64		m_memoryBlocksOffset;	// offset to memory blocks

		u32		m_numDataRegs;			// number of additional data registers in the file
		u64		m_dataRegsOffset;		// offset to data regs

		u64		m_memoryDumpOffset;		// offset to memory dump block
	};
#pragma pack( pop, 4 )

	std::vector< Block >		m_blocks;		// blocks (top level hierarchy of the frame)
	std::vector< Packet >		m_packets;		// packets to execute (NON recursive)
	std::vector< MemoryRef >	m_memoryRefs;	// memory references
	std::vector< Memory >		m_memoryBlocks;	// memory block dumps
	std::vector< u32 >		m_dataRegs;		// packets data

public:
	CXenonGPUDumpFormat();

	/// load from file, offset to memory dump data is extracted to
	bool Load( const std::wstring& path, u64& memoryDumpOffset );

	// save to file, raw memory dump is copied from the file specified
	bool Save( const std::wstring& path, const std::wstring& memoryBlobFile ) const;

private:
	static const u32 FILE_MAGIC;
	static const u32 FILE_VERSION;
};