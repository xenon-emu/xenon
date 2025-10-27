#pragma once

#include "xenonGPUConstants.h"
#include "xenonGPUState.h"
#include "Core/RAM/RAM.h"

/// Helper objects
class IXenonGPUAbstractRenderTarget;
class IXenonGPUAbstractDepthStencil;
class IXenonGPUDumpWriter;
struct XenonTextureInfo;
class XenonSamplerInfo;

/// Helper layer to translate native GPU command into more abstract stuff than can be mapped into DX11/DX12
class IXenonGPUAbstractLayer
{
public:
	virtual ~IXenonGPUAbstractLayer() {};

	/// Initialize the GPU "window"
	virtual bool Initialize() = 0;

	/// Set display mode
	virtual bool SetDisplayMode( const u32 width, const u32 height ) = 0;

	/// Being next frame
	virtual void BeingFrame() = 0;

	/// Swap the video swapchain
	virtual void Swap( const CXenonGPUState::SwapState& ss ) = 0;

	//------------------------------------------------------------

	/// Begin event
	virtual void BeginEvent(const char* name) = 0;

	/// End event
	virtual void EndEvent() = 0;

	//------------------------------------------------------------

	/// Bind color render target, returns reference (not reference counted) to the bounded RT
	virtual void BindColorRenderTarget( const u32 index, const XenonColorRenderTargetFormat format, const XenonMsaaSamples msaa, const u32 base, const u32 pitch ) = 0;

	/// Unbind any color render target from given slot
	virtual void UnbindColorRenderTarget( const u32 index ) = 0;

	/// Bind color render target, returns reference (not reference counted) to the bounded RT
	virtual void BindDepthStencil( const XenonDepthRenderTargetFormat format, const XenonMsaaSamples msaa, const u32 base, const u32 pitch ) = 0;	

	/// Unbind depth stencil
	virtual void UnbindDepthStencil() = 0;

	/// Set color mask for render target (dynamic stuff)
	virtual void SetColorRenderTargetWriteMask( const u32 index, const bool enableRed, const bool enableGreen, const bool enableBlue, const bool enableAlpha ) = 0;

	/// Clear currently bound color render target with given clear color. The bounded surface is always cleared, if requested, the EDRAM is cleared to (more expensive but mimics hardware better)
	virtual void ClearColorRenderTarget( const u32 index, const float* clearColor, const bool flushToEDRAM ) = 0;

	/// Clear currently bound depth surface with given values. The bounded surface is always cleared, if requested, the EDRAM is cleared to (more expensive but mimics hardware better)
	virtual void ClearDepthStencilRenderTarget( const float depthClear, const u32 stencilClear, const bool flushToEDRAM ) = 0;

	/// Realize the setup of render targets
	/// NOTE: this may do data copying and other stuff
	virtual bool RealizeSurfaceSetup( u32& outMainWidth,  u32& outMainHeight ) = 0;

	// Resolve color surface to a texture with given setup (note: the texture is implicitly created)
	virtual bool ResolveColorRenderTarget( const u32 srcIndex, const XenonColorRenderTargetFormat srcFormat, const u32 srcBase, const XenonRect2D& srcRect, 
										const u32 destBase, const u32 destLogicalWidth, const u32 destLogicalHeight, const u32 destBlockWidth, const u32 destBlockHeight, const XenonTextureFormat destFormat, const XenonRect2D& destRect ) = 0;

	// Resolve depth surface to a texture with given setup (note: the texture is implicitly created)
	virtual bool ResolveDepthRenderTarget( const XenonDepthRenderTargetFormat srcFormat, const u32 srcBase, const XenonRect2D& srcRect, 
		const u32 destBase, const u32 destLogicalWidth, const u32 destLogicalHeight, const u32 destBlockWidth, const u32 destBlockHeight, const XenonTextureFormat destFormat, const XenonRect2D& destRect ) = 0;

	//------------------------------------------------------------

	/// Apply geometry control
	// xyDivied - incoming X and Y are already divided by W
	// zDivied - incoming Z is already divided by W
	// wNotInverted - incoming W is NOT yet inverted to 1/W
	virtual void SetViewportVertexFormat( const bool xyDivied, const bool zDivied, const bool wNotInverted ) = 0;

	/// Set the viewport vertex scaling (true=coordinates are normalized to RT size, false=vertex coordinates are per-pixel)
	virtual void SetViewportWindowScale( const bool bNormalizedXYCoordinates ) = 0;

	/// Enable scissor rectangle
	virtual void EnableScisor( const u32 x, const u32 y, const u32 w, const u32 h ) = 0;

	/// Disable scissor rectangle
	virtual void DisableScisor() = 0;

	/// Setup viewport XY ranges
	virtual void SetViewportRange( const float x, const float y, const float w, const float h ) = 0;

	/// Setup depth range
	virtual void SetDepthRange( const float offset, const float scale ) = 0;

	/// Realize viewport setup, can return false to prevent rendering
	virtual bool RealizeViewportSetup() = 0;

	//------------------------------------------------------------

	/// Enable/Disable depth state
	virtual void SetDepthTest( const bool isEnabled ) = 0;

	/// Set depth test ask
	virtual void SetDepthWrite( const bool isEnabled ) = 0;

	/// Set depth function
	virtual void SetDepthFunc( const XenonCmpFunc func ) = 0;

	/// Enable/Disable stencil test
	virtual void SetStencilTest( const bool isEnabled ) = 0;

	// Set stencil write mask
	virtual void SetStencilWriteMask( const u8 mask ) = 0;

	// Set stencil read mask
	virtual void SetStencilReadMask( const u8 mask ) = 0;

	// Set stencil reference value
	virtual void SetStencilRef( const u8 mask ) = 0;

	// Set stencil function
	virtual void SetStencilFunc( const bool front, const XenonCmpFunc func ) = 0;

	// Set stencil ops
	virtual void SetStencilOps( const bool front, const XenonStencilOp sfail, const XenonStencilOp dfail, const XenonStencilOp dpass ) = 0;

	// Realize depth stencil state, can return false to prevent rendering
	virtual bool RealizeDepthStencilState() = 0;

	//------------------------------------------------------------

	// Enable blending for given render target output
	virtual void SetBlend( const u32 rtIndex, const bool isEnabled ) = 0;

	// Set blending equation for both color and alpha pipelines
	virtual void SetBlendOp( const u32 rtIndex, const XenonBlendOp colorOp, const XenonBlendOp alphaOp ) = 0;

	// Set blending arguments
	virtual void SetBlendArg( const u32 rtIndex, const XenonBlendArg colorSrc, const XenonBlendArg colorDest, const XenonBlendArg alphaSrc, const XenonBlendArg alphaDest ) = 0;

	// Set blending constant color
	virtual void SetBlendColor( const float r, const float g, const float b, const float a ) = 0;

	// Realize blending state, can return false to prevent rendering
	virtual bool RealizeBlendState() = 0;

	//------------------------------------------------------------

	// Set culling mode
	virtual void SetCullMode( const XenonCullMode cullMode ) = 0;

	// Set filling mode
	virtual void SetFillMode( const XenonFillMode fillMode ) = 0;

	// Set which face is the front face ?
	virtual void SetFaceMode( const XenonFrontFace faceMode ) = 0;

	// Set primitive restart mode
	virtual void SetPrimitiveRestart( const bool isEnabled ) = 0;

	// Set the primitive restart index
	virtual void SetPrimitiveRestartIndex( const u32 index ) = 0;

	// Realize raster state, can return false to prevent rendering
	virtual bool RealizeRasterState() = 0;

	//------------------------------------------------------------

	/// Set pixel shader code
	virtual void SetPixelShader( const void* microcode, const u32 numWords ) = 0;

	/// Set pixel shader code
	virtual void SetVertexShader( const void* microcode, const u32 numWords ) = 0;

	/// Set pixel shader constants
	virtual void SetPixelShaderConsts( const u32 firstVector, const u32 numVectors, const float* values ) = 0;

	/// Set vertex shader constants
	virtual void SetVertexShaderConsts( const u32 firstVector, const u32 numVectors, const float* values ) = 0;

	/// Set boolean constants (all of them)
	virtual void SetBooleanConstants( const u32* boolConstants ) = 0;

	/// Realize shader constants
	virtual bool RealizeShaderConstants() = 0;

	//------------------------------------------------------------

	/// Geometry interface
	// TEMPSHIT !
	virtual bool DrawGeometry( const class CXenonGPURegisters& regs, IXenonGPUDumpWriter* traceDump, const CXenonGPUState::DrawIndexState& ds, RAM *ram) = 0;

	//------------------------------------------------------------

	// Get list of active fetch slots (depends on the shaders bound)
	// NOTE: abstraction leakage
	virtual u32 GetActiveTextureFetchSlotMask() const = 0;

	// Bind texture to given fetch slot
	virtual void SetTexture( const u32 fetchSlot, const XenonTextureInfo* texture ) = 0;

	// Bind sampler to given fetch slot
	virtual void SetSampler(const u32 fetchSlot, const XenonSamplerInfo* sampler) = 0;
};

/// Helper render target information
class IXenonGPUAbstractRenderTarget
{
public:
	/// Get render target format
	virtual XenonColorRenderTargetFormat GetFormat() const = 0;

	/// Get MSAA settings
	virtual XenonMsaaSamples GetMSAA() const = 0;

	/// Get memory pitch
	virtual u32 GetMemoryPitch() const = 0;

	/// Get assigned EDRAM memory placement (-1 if not assigned)
	virtual int32_t GetEDRAMPlacement() const = 0;

	/// Clear to given color (note: should be called from the rendering thread)
	virtual void Clear( const float* clearColor ) = 0;

protected:
	virtual ~IXenonGPUAbstractRenderTarget() {};
};

/// Helper render target information
class IXenonGPUAbstractDepthStencil
{
public:
	/// Get render target format
	virtual XenonDepthRenderTargetFormat GetFormat() const = 0;

	/// Get MSAA settings
	virtual XenonMsaaSamples GetMSAA() const = 0;

	/// Get memory pitch
	virtual u32 GetMemoryPitch() const = 0;

	/// Get assigned EDRAM memory placement (-1 if not assigned)
	virtual int32_t GetEDRAMPlacement() const = 0;

	/// Clear to given depth and stencil values (note: should be called from the rendering thread)
	virtual void Clear( const bool clearDepth, const bool clearStencil, const float depthValue, const u32 stencilValue ) = 0;

protected:
	virtual ~IXenonGPUAbstractDepthStencil() {};
};

/// Helper for abstract surface access
class IXenonGPUAbstractSurface
{
public:
	/// Get texture format
	virtual XenonTextureFormat GetFormat() const = 0;

	/// Get base level width
	virtual u32 GetWidth() const = 0;

	/// Get base level height (1 for 1D textures)
	virtual u32 GetHeight() const = 0;

	/// Get base level depth (1 for 2D textures)
	virtual u32 GetDepth() const = 0;

	/// Get memory row pitch (2D and 3D textures only)
	virtual u32 GetRowPitch() const = 0;

	/// Get memory slice pitch (3D textures only)
	virtual u32 GetSlicePitch() const = 0;

	/// Get assigned memory address
	virtual u32 GetSourceMemoryAddress() const = 0;

protected:
	virtual ~IXenonGPUAbstractSurface() {};
};

/// Helper for abstract texture
class IXenonGPUAbstractTexture
{
public:
	/// Get base address (GPU)
	virtual u32 GetBaseAddress() const = 0;

	/// Get texture format
	virtual XenonTextureFormat GetFormat() const = 0;

	/// Get top level type of the texture
	virtual XenonTextureType GetType() const = 0;

	/// Get base level width
	virtual u32 GetBaseWidth() const = 0;

	/// Get base level height (1 for 1D textures)
	virtual u32 GetBaseHeight() const = 0;

	/// Get base level depth (1 for 2D textures)
	virtual u32 GetBaseDepth() const = 0;

	/// Get number of mipmaps in the texture
	virtual u32 GetNumMipLevels() const = 0;

	/// Get number of array slices (1 for simple cases)
	virtual u32 GetNumArraySlices() const = 0;

	// Get sub-surface
	virtual IXenonGPUAbstractSurface* GetSurface( const u32 slice, const u32 mip ) = 0;

protected:
	virtual ~IXenonGPUAbstractTexture() {};
};

struct XenonGPUScope
{
	XenonGPUScope(IXenonGPUAbstractLayer* layer, const char* name, ...)
		: m_layer(layer)
	{
		if (m_layer)
		{
			char buffer[1024];
			va_list args;

			va_start(args, name);
			vsprintf_s(buffer, name, args);
			va_end(args);

			m_layer->BeginEvent(buffer);
		}
	}

	~XenonGPUScope()
	{
		if (m_layer)
			m_layer->EndEvent();
	}

private:
	IXenonGPUAbstractLayer* m_layer;
};