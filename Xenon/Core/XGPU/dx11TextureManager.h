#pragma once

#include <Windows.h>
#include <d3d11.h>

#include "xenonGPUConstants.h"
#include "xenonGPUAbstractLayer.h"
#include "xenonGPUTextures.h"

//---------------------------------------------------------------------------

class CDX11StagingTexture;
class CDX11StagingTextureCache;

//---------------------------------------------------------------------------

/// Actual DX11 surface implementation
class CDX11AbstractSurface : public IXenonGPUAbstractSurface
{
public:
	/// IXenonGPUAbstractSurface interface
	virtual XenonTextureFormat GetFormat() const override final;
	virtual u32 GetWidth() const override final;
	virtual u32 GetHeight() const override final;
	virtual u32 GetDepth() const override final;
	virtual u32 GetRowPitch() const override final;
	virtual u32 GetSlicePitch() const override final;

	/// Get assigned memory address
	virtual u32 GetSourceMemoryAddress() const override final;

	// get writable view
	inline ID3D11UnorderedAccessView* GetWritableView() const { return m_writeView; }


	// Upload texture from CPU data (may recompress)
	void Upload(const void* srcTextureData, void* destData, u32 destRowPitch, u32 destSlicePitch) const;

protected:
	CDX11AbstractSurface();
	virtual ~CDX11AbstractSurface();

	u32					m_width;
	u32					m_height;
	u32					m_depth;

	XenonTextureFormat		m_sourceFormat;
	u32					m_sourceFormatBlockWidth;
	XenonGPUEndianFormat	m_sourceEndianess;
	u32					m_sourceWidth;
	u32					m_sourceHeight;
	u32					m_sourceBlockWidth;
	u32					m_sourceBlockHeight;
	u32					m_sourceBlockSize;
	u32					m_sourceRowPitch;
	u32					m_sourceSlicePitch;
	u32					m_sourceMemoryOffset; // offset vs base (may depend on tiling)
	u32					m_sourcePackedTileOffsetX;
	u32					m_sourcePackedTileOffsetY;
	bool					m_sourceIsTiled;
	bool					m_isBlockCompressed;

	ID3D11UnorderedAccessView*	m_writeView; // for resolve

	DXGI_FORMAT					m_runtimeFormat;





	friend class CDX11AbstractTexture;
};

//---------------------------------------------------------------------------

/// Texture wrapper
class CDX11AbstractTexture : public IXenonGPUAbstractTexture
{
public:
	CDX11AbstractTexture(RAM* ramPtr);
	// IXenonGPUAbstractTexture interface
	virtual u32 GetBaseAddress() const override final;
	virtual XenonTextureFormat GetFormat() const override final;
	virtual XenonTextureType GetType() const override final;
	virtual u32 GetBaseWidth() const override final;
	virtual u32 GetBaseHeight() const override final;
	virtual u32 GetBaseDepth() const override final;
	virtual u32 GetNumMipLevels() const override final;
	virtual u32 GetNumArraySlices() const  override final;
	virtual IXenonGPUAbstractSurface* GetSurface(const u32 slice, const u32 mip) override final;

	// get runtime texture format
	inline DXGI_FORMAT GetRuntimeFormat() const { return m_runtimeFormat; }

	// get runtime texture
	inline ID3D11Resource* GetRuntimeTexture() const { return m_runtimeTexture; }

	// get general resource view
	inline ID3D11ShaderResourceView* GetView() const { return m_view; }

	// get view format
	inline DXGI_FORMAT GetViewFormat() const { return m_viewFormat; }

	// Create new texture based on the XenonTextureInfo
	// NOTE: the XenonTextureInfo is usually created from fetch structure
	static CDX11AbstractTexture* Create(ID3D11Device* device, CDX11StagingTextureCache* stagingCache, const struct XenonTextureInfo* textureInfo);
	virtual ~CDX11AbstractTexture(); // NOTE: textures are purged if they are not used for a while

									 // Ensure that texture is up to date
	void EnsureUpToDate(RAM* ram);

protected:
	RAM *ram = nullptr;
	CDX11AbstractTexture();

	XenonTextureInfo			m_sourceInfo;
	XenonTextureType			m_sourceType;
	u32						m_sourceMips;
	u32						m_sourceArraySlices;

	DXGI_FORMAT					m_runtimeFormat;
	ID3D11Resource*				m_runtimeTexture;


	DXGI_FORMAT					m_viewFormat;
	ID3D11ShaderResourceView*	m_view; // for shaders only

	bool						m_initialDirty;

	std::vector< CDX11AbstractSurface* >	m_surfaces; // per slices, per mip
	std::vector< CDX11StagingTexture* >		m_stagingBuffers; // per mip

															  // resource creation
	bool CreateResources(ID3D11Device* device);

	// surface creation
	bool CreateSurfaces(ID3D11Device* device);

	// staging data creation
	bool CreateStagingBuffers(CDX11StagingTextureCache* stagingDataCache);

	// should texture be updated ?
	bool ShouldBeUpdated() const;

	// update texture (forced)
	void Update(RAM* ramPtr);

	// mapping
	static bool MapFormat(XenonTextureFormat sourceFormat, DXGI_FORMAT& runtimeFormat, DXGI_FORMAT& viewFormat);

	static bool IsBlockCompressedFormat(XenonTextureFormat sourceFormat);
};

//---------------------------------------------------------------------------

/// Texture manager
class CDX11TextureManager
{
public:
	CDX11TextureManager(ID3D11Device* device, ID3D11DeviceContext* context);
	~CDX11TextureManager();

	/// How to evict textures
	enum class EvictionPolicy
	{
		All,			// evict ALL textures (they will be recreated and reuploaded)
		Unused,			// evict textures not used for few frames
	};

	/// Evict unused textures
	void EvictTextures(const EvictionPolicy policy);

	/// Find texture with given base address (not created if missing)
	CDX11AbstractTexture* FindTexture(const u32 baseAddress);

	/// Get/create texture matching given settings (RT only), one mip, no slices
	/// NOTE: this is ONLY for render targets/other textures NOT coming from samplers
	CDX11AbstractTexture* GetTexture(const u32 baseAddress, const u32 width, const u32 height, const XenonTextureFormat format);

	/// Get/create texture matching given runtime texture setup
	/// NOTE: texture will not be re-uploaded unless the memory region is marked as changed
	CDX11AbstractTexture* GetTexture(const XenonTextureInfo* textureInfo);

private:
	typedef std::vector< CDX11AbstractTexture* >		TTextures;

	// internal lock
	typedef std::mutex					TLock;
	typedef std::lock_guard<TLock>		TScopeLock;
	TLock			m_lock;

	// all created (and managed) textures
	TTextures				m_textures;

	// device
	ID3D11Device*			m_device;
	ID3D11DeviceContext*	m_context;

	// texture staging buffer
	CDX11StagingTextureCache*	m_stagingCache;

	// internal timing (used to evict textures)
	u32					m_frameIndex;
};

//---------------------------------------------------------------------------
