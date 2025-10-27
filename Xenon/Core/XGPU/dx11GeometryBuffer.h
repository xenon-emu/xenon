#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "xenonGPUConstants.h"

/// Buffer management for the drawable geometry (vertex & index buffers)
class CDX11GeometryBuffer
{
public:
	CDX11GeometryBuffer( ID3D11Device* dev, ID3D11DeviceContext* context );
	~CDX11GeometryBuffer();

	// type of buffers
	static const u32 BUFFER_VERTEX = 1;
	static const u32 BUFFER_INDEX = 2;

	// handle to buffer data
	struct BufferHandle
	{
		u32	m_generation;
		u32	m_offset;
		u32	m_size:30;
		u32  m_type:2; // 1-vb, 2-ib

		inline BufferHandle()
			: m_generation(0)
			, m_offset(0)
			, m_size(0)
			, m_type(0)
		{}
	};

	// new frame
	void Swap();

	// we given data up to date ?
	const bool IsBufferUsable( BufferHandle handle ) const;

	// upload (or use existing) vertex buffer geometry, returns buffer handle
	// NOTE: the data is byteswapped (dwords)
	const BufferHandle UploadVertices( const void* sourceMemory, const u32 sourceMemorySize, const bool verticesNeedSwapping );

	// upload (or use existing) index buffer geometry, returns buffer handle
	// NOTE: the data is byteswapped
	const BufferHandle UploadIndices16( const void* sourceMemory, const u32 numIndices, const bool indicesNeedSwapping );

	// upload (or use existing) index buffer geometry, returns buffer handle
	// NOTE: the data is byteswapped
	const BufferHandle UploadIndices32( const void* sourceMemory, const u32 numIndices, const bool indicesNeedSwapping  );

	// bind vertex buffer data
	const bool BindData( BufferHandle data, const u32 bufferIndex );

private:
	// DX11 device, always valid
	ID3D11Device*				m_device;
	ID3D11DeviceContext*		m_mainContext;

	// helper shitty allocator
	class LinearAllocator
	{
	public:
		LinearAllocator( const u32 size );

		// get allocator size
		inline const u32 GetSize() const { return m_size; }

		// allocate space, will set "wrapped" to true if we wrapped back
		u32 Alloc( const u32 size, bool& wrapped );

		// reset allocator
		void Reset();

	private:
		u32			m_offset;
		u32			m_size;
	};

	// General buffer memory
	ID3D11Buffer*				m_geometryData;				// geometry data buffer (big shit, 100 MB or so)
	ID3D11Buffer*				m_geometryDataSecondary;	// secondary geometry data buffer (big shit, 100 MB or so)
	u32						m_geometryDataGeneration;	// internal generation counter
	u32						m_geometryDataTransferSize;	// transfer buffer helper
	LinearAllocator				m_geometryDataAllocator;	// data allocator
	bool						m_geomeryDataSwapped;		// data was just swapped
	

	// ensure there's enough room in the staging buffer and lock it
	bool AllocStagingBuffer( const u32 sourceMemorySize, void*& outPtr, u32& outOffset ); 

	// unlock and transfer staging data, return buffer handle
	BufferHandle UploadStagingData( const u32 sourceMemorySize, const u32 dataOffset, const u32 type );
};
