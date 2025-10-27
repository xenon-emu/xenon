#pragma once

/// based on AMD Driver implementation
// https://github.com/freedreno/amd-gpu/blob/master/

#include "xenonGPUConstants.h"
#include "xenonGPUUtils.h"

class IXenonGPUAbstractLayer;
class IXenonGPUDumpWriter;
class CXenonGPURegisters;
class CXenonGPUDirtyRegisterTracker;

/// State holder for render targets
struct XenonStateRenderTargetsRegisters
{
	u32 regModeControl;
	u32 regSurfaceInfo;
	u32 regColorInfo[4];
	u32 regColorMask;
	u32 regDepthControl;
	u32 regStencilRefMask;
	u32 regDepthInfo;

	inline XenonStateRenderTargetsRegisters() { Reset(); }
	inline void Reset() { memset(this, 0, sizeof(*this)); }
};

/// State holder for viewport state
struct XenonStateViewportRegisters
{
	// uint32_t pa_cl_clip_cntl;
	u32 regSurfaceInfo;
	u32 regPaClVteCntl;
	u32 regPaSuScModeCntl;
	u32 regPaScWindowOffset;
	u32 regPaScWindowScissorTL;
	u32 regPaScWindowScissorBR;
	float regPaClVportXoffset;
	float regPaClVportYoffset;
	float regPaClVportZoffset;
	float regPaClVportXscale;
	float regPaClVportYscale;
	float regPaClVportZscale;

	inline XenonStateViewportRegisters() { Reset(); }
	inline void Reset() { memset(this, 0, sizeof(*this)); }
};

/// State holder for rasterization state
struct XenonStateRasterizerRegisters
{
	u32 regPaSuScModeCntl;
	u32 regPaScScreenScissorTL;
	u32 regPaScScreenScissorBR;
	u32 regMultiPrimIbResetIndex;

	inline XenonStateRasterizerRegisters() { Reset(); }
	inline void Reset() { std::memset(this, 0, sizeof(*this)); }
};

/// State holder for blend state registers
struct XenonStateBlendRegisters
{
	u32 regRbBlendControl[4];
	float regRbBlendRGBA[4];

	inline XenonStateBlendRegisters() { Reset(); }
	inline void Reset() { std::memset(this, 0, sizeof(*this)); }
};

/// State holder for depth/stencil registers
struct XenonStateDepthStencilRegisters
{
	u32 regRbDepthControl;
	u32 regRbStencilRefMask;

	inline XenonStateDepthStencilRegisters() { Reset(); }
	inline void Reset() { std::memset(this, 0, sizeof(*this)); }
};

/// State holder for shader registers
struct XenonStateShadersRegisters
{
	//PrimitiveType prim_type;
	u32 regPaSuScModeCntl;
	u32 regSqProgramCntl;
	
	inline XenonStateShadersRegisters() { Reset(); }
	inline void Reset() { std::memset(this, 0, sizeof(*this)); }
};

/// State management
class CXenonGPUState
{
public:
	CXenonGPUState();

	// draw state
	struct DrawIndexState
	{
		XenonPrimitiveType		m_primitiveType;
		const void*				m_indexData;
		XenonIndexFormat		m_indexFormat;
		XenonGPUEndianFormat	m_indexEndianess;
		u32					m_indexCount;
		u32					m_baseVertexIndex;
	};

	// swap state
	struct SwapState
	{
		u32					m_frontBufferBase;
		u32					m_frontBufferWidth;
		u32					m_frontBufferHeight;
	};

public:
	/// Issue an indexed geometry draw using specified state
	bool IssueDraw( IXenonGPUAbstractLayer* abstractLayer, IXenonGPUDumpWriter* traceDump, const CXenonGPURegisters& regs, const CXenonGPUDirtyRegisterTracker& dirtyRegs, const DrawIndexState& ds, RAM* ram);

	/// Issue an copy/clear operation
	bool IssueCopy( IXenonGPUAbstractLayer* abstractLayer, IXenonGPUDumpWriter* traceDump, const CXenonGPURegisters& regs, RAM *ram );

	/// Issue swap
	bool IssueSwap( IXenonGPUAbstractLayer* abstractLayer, IXenonGPUDumpWriter* traceDump, const CXenonGPURegisters& regs, const SwapState& ss );

private:
	/// Report failed draw, returns false
	bool ReportFailedDraw( const char* reason );

	/// Update (and reapply if needed) render target state
	bool UpdateRenderTargets( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs );

	/// Update (and reapply if needed) viewport state
	bool UpdateViewportState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs );

	/// Update (and reapply if needed) rasterization state
	bool UpdateRasterState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs );

	/// Update (and reapply if needed) blend state
	bool UpdateBlendState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs );

	/// Update (and reapply if needed) the depth stencil state
	bool UpdateDepthState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs );

	/// Update pixel & vertex shader constants, uses the dirty mask to check which values to update
	bool UpdateShaderConstants( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs, const CXenonGPUDirtyRegisterTracker& dirtyRegs );

	/// Update bound textures and samples, uses the dirty mask to check which values to update
	bool UpdateTexturesAndSamplers( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs, class IXenonGPUDumpWriter* traceDump, RAM *ram );

	/// Apply render target state via the abstraction layer, returns false only on errors
	/// NOTE: the function is static only to ensure there are NO dependencies on anything else than the passed state
	static bool ApplyRenderTargets( IXenonGPUAbstractLayer* abstractLayer, const XenonStateRenderTargetsRegisters& rtState, u32& outPhysicalWidth, u32& outPhysicalHeight );

	/// Apply viewport state via the abstraction layer, returns false only on errors
	/// NOTE: the function is static only to ensure there are NO dependencies on anything else than the passed state
	static bool ApplyViewportState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateViewportRegisters& viewState, const u32 physicalWidth, const u32 physicalHeight );

	/// Apply raster state via the abstraction layer, returns false only on errors
	/// NOTE: the function is static only to ensure there are NO dependencies on anything else than the passed state
	static bool ApplyRasterState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateRasterizerRegisters& rasterState );

	/// Apply raster state via the abstraction layer, returns false only on errors
	/// NOTE: the function is static only to ensure there are NO dependencies on anything else than the passed state
	static bool ApplyBlendState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateBlendRegisters& blendState );

	/// Apply raster state via the abstraction layer, returns false only on errors
	/// NOTE: the function is static only to ensure there are NO dependencies on anything else than the passed state
	static bool ApplyDepthState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateDepthStencilRegisters& depthState );

	// state vectors
	XenonStateRenderTargetsRegisters		m_rtState;
	XenonStateViewportRegisters				m_viewState;
	XenonStateRasterizerRegisters			m_rasterState;
	XenonStateBlendRegisters				m_blendState;
	XenonStateDepthStencilRegisters			m_depthState;
	XenonStateShadersRegisters				m_shaderState;

	// physical rendering surface size
	u32									m_physicalRenderWidth;
	u32									m_physicalRenderHeight;
};