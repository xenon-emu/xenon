#pragma once

#include <Windows.h>
#include <d3d11.h>

//---------------------------------------------------------------------------

/// Staging texture
class CDX11StagingTexture
{
public:
	~CDX11StagingTexture();

	// get format info
	inline const DXGI_FORMAT GetFormat() const { return m_format; }
	inline const u32 GetDimmensions() const { return m_dims; }
	inline const u32 GetWidth() const { return m_width; }
	inline const u32 GetHeight() const { return m_height; }
	inline const u32 GetDepth() const { return m_depth; }

	// begin update, lock the staging buffer
	void* Lock( u32& outRowPitch, u32& outSlicePitch );

	// flush update
	void Flush( ID3D11Resource* target, const u32 targetSlice );

	// create staging texture
	static CDX11StagingTexture* Create( ID3D11Device* dev, ID3D11DeviceContext*	context, const u32 dims, DXGI_FORMAT format, const u32 width, const u32 height, const u32 depth );

private:
	CDX11StagingTexture();

	// device owner
	ID3D11Device*			m_device;
	ID3D11DeviceContext*	m_deviceContext;

	// format and size info
	DXGI_FORMAT		m_format;
	u32			m_dims;
	u32			m_width;
	u32			m_height;
	u32			m_depth;

	// texture
	ID3D11Resource*	m_texture;

	// lock state
	bool			m_locked;
};

//---------------------------------------------------------------------------

/// Cache for staging textures
class CDX11StagingTextureCache
{
public:
	CDX11StagingTextureCache( ID3D11Device* dev, ID3D11DeviceContext* context );
	~CDX11StagingTextureCache();

	// get staging resource out of cache
	CDX11StagingTexture* GetStagingTexture( const u32 dims, DXGI_FORMAT format, const u32 width, const u32 height, const u32 depth );

private:
	typedef std::vector< CDX11StagingTexture* >		 TTextures;
	TTextures		m_textures;

	// free all resources
	void FreeResources();

	// internal lock
	typedef std::mutex					TLock;
	typedef std::lock_guard<TLock>		TScopeLock;
	TLock					m_lock;

	// device owner
	ID3D11Device*			m_device;
	ID3D11DeviceContext*	m_deviceContext;
};

//---------------------------------------------------------------------------
