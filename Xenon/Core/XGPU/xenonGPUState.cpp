#include "build.h"
#include "xenonGPUUtils.h"
#include "xenonGPUState.h"
#include "xenonGPUAbstractLayer.h"
#include "xenonGPURegisters.h"
#include <xutility>
#include "xenonGPUTextures.h"
#include "xenonGPUDumpWriter.h"

#pragma optimize("",off)

namespace Helper
{
	static inline bool UpdateRegister( const CXenonGPURegisters& regs, const XenonGPURegister reg, u32& value )
	{
		// value is different
		if ( regs[reg].m_dword != value )
		{
			value = regs[reg].m_dword;
			return true;
		}

		// value is the same
		return false;
	}

	static inline bool UpdateRegister( const CXenonGPURegisters& regs, const XenonGPURegister reg, float& value )
	{
		// value is different
		if ( regs[reg].m_float != value )
		{
			value = regs[reg].m_float;
			return true;
		}

		// value is the same
		return false;
	}

	static inline XenonTextureFormat RTFormatToTextureFormat( XenonColorRenderTargetFormat format )
	{
		switch ( format )
		{
			case XenonColorRenderTargetFormat::Format_8_8_8_8: return XenonTextureFormat::Format_8_8_8_8;
			case XenonColorRenderTargetFormat::Format_8_8_8_8_GAMMA: return XenonTextureFormat::Format_8_8_8_8;
			case XenonColorRenderTargetFormat::Format_2_10_10_10: return XenonTextureFormat::Format_2_10_10_10;
			case XenonColorRenderTargetFormat::Format_2_10_10_10_FLOAT: return XenonTextureFormat::Format_2_10_10_10_FLOAT;
			case XenonColorRenderTargetFormat::Format_16_16: return XenonTextureFormat::Format_16_16;
			case XenonColorRenderTargetFormat::Format_16_16_16_16: return XenonTextureFormat::Format_16_16_16_16;
			case XenonColorRenderTargetFormat::Format_16_16_FLOAT: return XenonTextureFormat::Format_16_16_FLOAT;
			case XenonColorRenderTargetFormat::Format_16_16_16_16_FLOAT: return XenonTextureFormat::Format_16_16_16_16_FLOAT;
			case XenonColorRenderTargetFormat::Format_2_10_10_10_unknown: return XenonTextureFormat::Format_2_10_10_10;
			case XenonColorRenderTargetFormat::Format_2_10_10_10_FLOAT_unknown: return XenonTextureFormat::Format_2_10_10_10_FLOAT;
			case XenonColorRenderTargetFormat::Format_32_FLOAT: return XenonTextureFormat::Format_32_FLOAT;
			case XenonColorRenderTargetFormat::Format_32_32_FLOAT: return XenonTextureFormat::Format_32_32_FLOAT;
		}

		DEBUG_CHECK(!"Unsupported color format");
		return XenonTextureFormat::Format_Unknown;
	}

	static inline XenonTextureFormat ColorFormatToTextureFormat( XenonColorFormat format )
	{
		switch ( format )
		{
			case XenonColorFormat::Format_8: return XenonTextureFormat::Format_8;
			case XenonColorFormat::Format_8_8_8_8: return XenonTextureFormat::Format_8_8_8_8;
			case XenonColorFormat::Format_2_10_10_10: return XenonTextureFormat::Format_2_10_10_10;
			case XenonColorFormat::Format_32_FLOAT: return XenonTextureFormat::Format_32_FLOAT;
			case XenonColorFormat::Format_16_16: return XenonTextureFormat::Format_16_16;
			case XenonColorFormat::Format_16: return XenonTextureFormat::Format_16;
		}

		DEBUG_CHECK( !"Unsupported color format" );
		return XenonTextureFormat::Format_Unknown;
	}

	static inline XenonTextureFormat DepthFormatToTextureFormat(XenonDepthRenderTargetFormat format)
	{
		switch (format)
		{
			case XenonDepthRenderTargetFormat::Format_D24S8: return XenonTextureFormat::Format_24_8;
			case XenonDepthRenderTargetFormat::Format_D24FS8: return XenonTextureFormat::Format_24_8_FLOAT;
		}

		DEBUG_CHECK(!"Unsupported color format");
		return XenonTextureFormat::Format_Unknown;
	}

} // Helper

CXenonGPUState::CXenonGPUState()
{
	m_physicalRenderHeight = 0;
	m_physicalRenderWidth = 0;
}

bool CXenonGPUState::IssueDraw( IXenonGPUAbstractLayer* abstractLayer, IXenonGPUDumpWriter* traceDump, const CXenonGPURegisters& regs, const CXenonGPUDirtyRegisterTracker& dirtyRegs, const DrawIndexState& ds, RAM* ram)
{
	// global state
	const auto enableMode = (XenonModeControl)( regs[XenonGPURegister::REG_RB_MODECONTROL].m_dword & 0x7 );
	if ( enableMode == XenonModeControl::Ignore )
	{
		printf( "Ignore draw!\n" );
		return true;
	}
	else if ( enableMode == XenonModeControl::Copy )
	{
		return IssueCopy( abstractLayer, traceDump, regs, ram);
	}

	// skip draw when disabled
	const auto surfaceEnableMode = regs[XenonGPURegister::REG_RB_SURFACE_INFO].m_dword & 0x3FFF;
	if (surfaceEnableMode == 0) 
		return true;

	// crap
	/*if (ds.m_primitiveType == XenonPrimitiveType::PrimitiveRectangleList && ds.m_indexCount == 3)
	{
		return abstractLayer->DrawGeometry(regs, traceDump, ds);
	}*/

	// prepare drawing conditions
	if (!UpdateViewportState(abstractLayer, regs))
		return ReportFailedDraw("UpdateViewport failed");
	if ( !UpdateRenderTargets( abstractLayer, regs ) )
		return ReportFailedDraw( "UpdateRenderTargets failed" );
	if ( !UpdateDepthState( abstractLayer, regs ) )
		return ReportFailedDraw( "UpdateDepthState failed" );
	if ( !UpdateBlendState( abstractLayer, regs ) )
		return ReportFailedDraw( "UpdateBlendState failed" );
	if ( !UpdateRasterState( abstractLayer, regs ) )
		return ReportFailedDraw( "UpdateRasterState failed" );
	if ( !UpdateShaderConstants( abstractLayer, regs, dirtyRegs ) )
		return ReportFailedDraw( "UpdateShaderConstants failed" );
	if ( !UpdateTexturesAndSamplers( abstractLayer, regs, traceDump, ram ) )
		return ReportFailedDraw( "UpdateTexturesAndSamplers failed" );

	// perform draw
	return abstractLayer->DrawGeometry( regs, traceDump, ds, ram);
}

bool CXenonGPUState::IssueSwap( IXenonGPUAbstractLayer* abstractLayer, IXenonGPUDumpWriter* traceDump, const CXenonGPURegisters& regs, const SwapState& ss )
{
	abstractLayer->BeingFrame();
	abstractLayer->Swap( ss );
	return true;
}

typedef struct
{
	DWORD Width : 24;   // DWORD
	DWORD : 8;
} GPUTEXTURESIZE_1D;

typedef struct
{
	DWORD Width : 13;   // DWORD
	DWORD Height : 13;   // DWORD
	DWORD : 6;
} GPUTEXTURESIZE_2D;

typedef struct
{
	DWORD Width : 13;   // DWORD
	DWORD Height : 13;   // DWORD
	DWORD Depth : 6;    // DWORD
} GPUTEXTURESIZE_STACK;

typedef struct
{
	DWORD Width : 11;   // DWORD
	DWORD Height : 11;   // DWORD
	DWORD Depth : 10;   // DWORD
} GPUTEXTURESIZE_3D;

typedef union {
	struct {
		DWORD CopySrcSelect : 3;    // GPUCOPYSRCSELECT
		DWORD : 1;
		DWORD CopySampleSelect : 3;    // GPUCOPYSAMPLESELECT
		DWORD : 1;
		DWORD ColorClearEnable : 1;
		DWORD DepthClearEnable : 1;
		DWORD : 10;
		DWORD CopyCommand : 2;    // GPUCOPYCOMMAND
		DWORD : 10;
	};
	DWORD dword;
} GPU_COPYCONTROL;

typedef union {
	struct {
		DWORD CopyDestPitch : 14;
		DWORD : 2;
		DWORD CopyDestHeight : 14;
		DWORD : 2;
	};
	DWORD dword;
} GPU_COPYDESTPITCH;

typedef union {
	struct {
		DWORD CopyDestEndian : 3;    // GPUENDIAN128
		DWORD CopyDestArray : 1;    // GPUCOLORARRAY
		DWORD CopyDestSlice : 3;
		DWORD CopyDestFormat : 6;    // GPUCOLORFORMAT
		DWORD CopyDestNumber : 3;    // GPUSURFACENUMBER
		DWORD CopyDestExpBias : 6;
		DWORD : 2;
		DWORD CopyDestSwap : 1;    // GPUSURFACESWAP
		DWORD : 7;
	};
	DWORD dword;
} GPU_COPYDESTINFO;

typedef union {
	struct {
		// DWORD 0:

		DWORD Type : 2;    // GPUCONSTANTTYPE
		DWORD SignX : 2;    // GPUSIGN
		DWORD SignY : 2;    // GPUSIGN
		DWORD SignZ : 2;    // GPUSIGN
		DWORD SignW : 2;    // GPUSIGN
		DWORD ClampX : 3;    // GPUCLAMP
		DWORD ClampY : 3;    // GPUCLAMP
		DWORD ClampZ : 3;    // GPUCLAMP
		DWORD : 2;
		DWORD : 1;
		DWORD Pitch : 9;    // DWORD
		DWORD Tiled : 1;    // BOOL

		// DWORD 1:

		DWORD DataFormat : 6;    // GPUTEXTUREFORMAT
		DWORD Endian : 2;    // GPUENDIAN
		DWORD RequestSize : 2;    // GPUREQUESTSIZE
		DWORD Stacked : 1;    // BOOL
		DWORD ClampPolicy : 1;    // GPUCLAMPPOLICY
		DWORD BaseAddress : 20;   // DWORD

		// DWORD 2:

		union
		{
			GPUTEXTURESIZE_1D OneD;
			GPUTEXTURESIZE_2D TwoD;
			GPUTEXTURESIZE_3D ThreeD;
			GPUTEXTURESIZE_STACK Stack;
		} Size;

		// DWORD 3:

		DWORD NumFormat : 1;    // GPUNUMFORMAT
		DWORD SwizzleX : 3;    // GPUSWIZZLE
		DWORD SwizzleY : 3;    // GPUSWIZZLE
		DWORD SwizzleZ : 3;    // GPUSWIZZLE
		DWORD SwizzleW : 3;    // GPUSWIZZLE
		INT   ExpAdjust : 6;    // int
		DWORD MagFilter : 2;    // GPUMINMAGFILTER
		DWORD MinFilter : 2;    // GPUMINMAGFILTER
		DWORD MipFilter : 2;    // GPUMIPFILTER
		DWORD AnisoFilter : 3;    // GPUANISOFILTER
		DWORD : 3;
		DWORD BorderSize : 1;    // DWORD

		// DWORD 4:

		DWORD VolMagFilter : 1;    // GPUMINMAGFILTER
		DWORD VolMinFilter : 1;    // GPUMINMAGFILTER
		DWORD MinMipLevel : 4;    // DWORD
		DWORD MaxMipLevel : 4;    // DWORD
		DWORD MagAnisoWalk : 1;    // BOOL
		DWORD MinAnisoWalk : 1;    // BOOL
		INT   LODBias : 10;   // int
		INT   GradExpAdjustH : 5;    // int
		INT   GradExpAdjustV : 5;    // int

		// DWORD 5:

		DWORD BorderColor : 2;    // GPUBORDERCOLOR
		DWORD ForceBCWToMax : 1;    // BOOL
		DWORD TriClamp : 2;    // GPUTRICLAMP
		INT   AnisoBias : 4;    // int
		DWORD Dimension : 2;    // GPUDIMENSION
		DWORD PackedMips : 1;    // BOOL
		DWORD MipAddress : 20;   // DWORD
	};
	DWORD dword[6];
} GPUTEXTURE_FETCH_CONSTANT;

typedef union {
	struct {
		// DWORD 0:

		DWORD Type : 2;    // GPUCONSTANTTYPE
		DWORD BaseAddress : 30;   // DWORD

		// DWORD 1:

		DWORD Endian : 2;    // GPUENDIAN
		DWORD Size : 24;   // DWORD
		DWORD AddressClamp : 1;    // GPUADDRESSCLAMP
		DWORD : 1;
		DWORD RequestSize : 2;    // GPUREQUESTSIZE
		DWORD ClampDisable : 2;    // BOOL
	};
	DWORD dword[2];
} GPUVERTEX_FETCH_CONSTANT;

typedef union {
	GPUTEXTURE_FETCH_CONSTANT           Texture;
	GPUVERTEX_FETCH_CONSTANT            Vertex[3];
} GPUFETCH_CONSTANT;

bool CXenonGPUState::IssueCopy( IXenonGPUAbstractLayer* abstractLayer, IXenonGPUDumpWriter* traceDump, const CXenonGPURegisters& regs, RAM *ram )
{
	XenonGPUScope scope(abstractLayer, "IssueCopy");

	// Resolve + optional (quite often) clear
	// Resolve is done through the abstractInterface (which forwards it to the EDRAM emulator)
	// The clear is done directly

	// master register
  GPU_COPYCONTROL copyReg;
	copyReg.dword = regs[XenonGPURegister::REG_RB_COPY_CONTROL].m_dword;

	// which render targets are affected ? (0-3 = colorRT, 4=depth)
	const u32 copyRT = (copyReg.dword & 7);

	// should we clear after copy ?
	const bool colorClearEnabled = (copyReg.dword >> 8) & 1;
	const bool depthClearEnabled = (copyReg.dword >> 9) & 1;

	// what to do
	auto copyCommand = (XenonCopyCommand)( (copyReg.dword >> 20) & 3);

	// target memory and format for the copy operation
	GPU_COPYDESTINFO copyDestInfoReg;
	copyDestInfoReg.dword = regs[XenonGPURegister::REG_RB_COPY_DEST_INFO].m_dword;
	const auto copyDestEndian = (XenonGPUEndianFormat128)(copyDestInfoReg.dword & 7 );
	const u32 copyDestArray = (copyDestInfoReg.dword >> 3) & 1;
	DEBUG_CHECK( copyDestArray == 0 ); // other values not yet supported
	const u32 copyDestSlice = (copyDestInfoReg.dword >> 4) & 1;
	DEBUG_CHECK( copyDestSlice == 0 ); // other values not yet supported
	const auto copyDestFormat = (XenonColorFormat)( (copyDestInfoReg.dword >> 7) & 0x3F );
	if (copyDestFormat == XenonColorFormat::Unknown)
		return true;

	const u32 copyDestNumber = (copyDestInfoReg.dword >> 13) & 7;
	const u32 copyDestBias = (copyDestInfoReg.dword >> 16) & 0x3F;
	const u32 copyDestSwap = (copyDestInfoReg.dword >> 25) & 1;

	const u32 copyDestBase = regs[ XenonGPURegister::REG_RB_COPY_DEST_BASE ].m_dword;
	const u32 copyDestPitch = regs[ XenonGPURegister::REG_RB_COPY_DEST_PITCH ].m_dword & 0x3FFF;
	const u32 copyDestHeight = (regs[ XenonGPURegister::REG_RB_COPY_DEST_PITCH ].m_dword >> 16) & 0x3FFF;

	// additional way to copy
	const u32 copySurfaceSlice = regs[ XenonGPURegister::REG_RB_COPY_SURFACE_SLICE ].m_dword;
	DEBUG_CHECK( copySurfaceSlice == 0 );
	const u32 copyFunc = regs[ XenonGPURegister::REG_RB_COPY_FUNC ].m_dword;
	DEBUG_CHECK( copyFunc == 0 );
	const u32 copyRef = regs[ XenonGPURegister::REG_RB_COPY_REF ].m_dword;
	DEBUG_CHECK( copyRef == 0 );
	const u32 copyMask = regs[ XenonGPURegister::REG_RB_COPY_MASK ].m_dword;
	DEBUG_CHECK( copyMask == 0 );

	// RB_SURFACE_INFO
	// http://fossies.org/dox/MesaLib-10.3.5/fd2__gmem_8c_source.html
	const u32 surfaceInfoReg = regs[ XenonGPURegister::REG_RB_SURFACE_INFO ].m_dword;
	const u32 surfacePitch = surfaceInfoReg & 0x3FFF;
	const auto surfaceMSAA = static_cast< XenonMsaaSamples >((surfaceInfoReg >> 16) & 0x3);

	// do the actual copy
	int32_t copyDestOffset = 0;
	if ( copyCommand != XenonCopyCommand::Null && copyCommand != XenonCopyCommand::ConstantOne )
	{
		const u32 destLogicalWidth = copyDestPitch;
		const u32 destLogicalHeight = copyDestHeight;
		const u32 destBlockWidth = Helper::RoundUp( destLogicalWidth, 32 );
		const u32 destBlockHeight = Helper::RoundUp( destLogicalHeight, 32 );

		// Compute destination and source area
		XenonRect2D destRect, srcRect;
		{		
			u32 windowOffset = regs[ XenonGPURegister::REG_PA_SC_WINDOW_OFFSET ].m_dword;
			int16_t windowOffsetX = (int16_t) windowOffset & 0x7FFF;
			int16_t windowOffsetY = (int16_t)( (windowOffset >> 16) & 0x7FFF );

			if ( windowOffsetX & 0x4000)
				windowOffsetX |= 0x8000;

			if ( windowOffsetY & 0x4000)
				windowOffsetY |= 0x8000;

			// HACK: vertices to use are always in vf0.
			{
				const u32 copyVertexFetchSlot = 0;
				const u32 reg = (u32) XenonGPURegister::REG_SHADER_CONSTANT_FETCH_00_0 + (copyVertexFetchSlot * 2);
				const XenonGPUVertexFetchData* fetch = &regs.GetStructAt<XenonGPUVertexFetchData>( (XenonGPURegister) reg );
				const GPUFETCH_CONSTANT* fetchData = &regs.GetStructAt<GPUFETCH_CONSTANT>((XenonGPURegister)reg);

				DEBUG_CHECK(fetch->type == 3);
				DEBUG_CHECK(fetch->endian == 2);
				DEBUG_CHECK(fetch->size == 6);

				const float* vertexData = (const float*) ram->GetPointerToAddress( fetch->address << 2 );

				// fetch vertex data
				float vertexX[3]; // x coords
				float vertexY[3]; // y coords
				const auto endianess = (XenonGPUEndianFormat) fetch->endian;
				vertexX[0] = XenonGPUSwapFloat( vertexData[0], endianess );
				vertexY[0] = XenonGPUSwapFloat( vertexData[1], endianess );

				vertexX[1] = XenonGPUSwapFloat( vertexData[2], endianess );
				vertexY[1] = XenonGPUSwapFloat( vertexData[3], endianess );

				vertexX[2] = XenonGPUSwapFloat( vertexData[4], endianess );
				vertexY[2] = XenonGPUSwapFloat( vertexData[5], endianess );

				// get min/max ranges
				const float destMinX = std::min<float>( std::min<float>( vertexX[0], vertexX[1] ), vertexX[2] );
				const float destMinY = std::min<float>( std::min<float>( vertexY[0], vertexY[1] ), vertexY[2] );
				const float destMaxX = std::max<float>( std::max<float>( vertexX[0], vertexX[1] ), vertexX[2] );
				const float destMaxY = std::max<float>( std::max<float>( vertexY[0], vertexY[1] ), vertexY[2] );

				// setup dest area
				destRect.x = (int)(destMinX + 0.5f);
				destRect.y = (int)(destMinY + 0.5f);
				destRect.w = (int)( destMaxX - destMinX );
				destRect.h = (int)( destMaxY - destMinY );

				// setup source copy area
				srcRect.x = 0;
				srcRect.y = 0;
				srcRect.w = destRect.w;
				srcRect.h = destRect.h;
			}

			// The dest base address passed in has already been offset by the window
			// offset, so to ensure texture lookup works we need to offset it.
			copyDestOffset = (windowOffsetY * copyDestPitch * 4) + (windowOffsetX * 32 * 4);
		}

		// Resolve the source render target to target texture
		// NOTE: we may be requested to do format conversion
		{
			XenonTextureFormat srcFormat = XenonTextureFormat::Format_Unknown;

			// color RT to copy ?
			if ( copyRT <= 3 ) 
			{
				const XenonGPURegister colorInfoRegs[] = { XenonGPURegister::REG_RB_COLOR_INFO, XenonGPURegister::REG_RB_COLOR1_INFO, XenonGPURegister::REG_RB_COLOR2_INFO, XenonGPURegister::REG_RB_COLOR3_INFO };
				const u32 colorInfo = regs[ colorInfoRegs[ copyRT ] ].m_dword;

				const u32 colorBase = colorInfo & 0xFFF; // EDRAM base
				const auto colorFormat = ( XenonColorRenderTargetFormat )( (colorInfo >> 16) & 0xF );

				abstractLayer->ResolveColorRenderTarget( copyRT, colorFormat, colorBase, srcRect, 
					copyDestBase + copyDestOffset, destLogicalWidth, destLogicalHeight, destBlockWidth, destBlockHeight, 
					Helper::ColorFormatToTextureFormat(copyDestFormat), destRect );
			}
			else
			{
				// Source from depth/stencil.
				const u32 depthInfo = regs[ XenonGPURegister::REG_RB_DEPTH_INFO ].m_dword;
				const u32 depthBase = (depthInfo & 0xFFF); // EDRAM base

				auto depthFormat = (XenonDepthRenderTargetFormat)( (depthInfo >> 16) & 0x1 );
				abstractLayer->ResolveDepthRenderTarget(depthFormat, depthBase, srcRect,
					copyDestBase + copyDestOffset, destLogicalWidth, destLogicalHeight, destBlockWidth, destBlockHeight,
					Helper::DepthFormatToTextureFormat(depthFormat), destRect);
			}
		}
	}

	// Perform any requested clears.
	const u32 copyDepthClear = regs[ XenonGPURegister::REG_RB_DEPTH_CLEAR ].m_dword;
	const u32 copyColorClear = regs[ XenonGPURegister::REG_RB_COLOR_CLEAR ].m_dword;
	const u32 copyColorClearLow = regs[ XenonGPURegister::REG_RB_COLOR_CLEAR_LOW ].m_dword;
	DEBUG_CHECK( copyColorClear == copyColorClearLow );

	// Clear the color buffer
	// NOTE: this happens AFTER resolve
	if ( colorClearEnabled )
	{
		DEBUG_CHECK( copyRT <= 3 );

		// Extract clear color
		float clearColor[4];
		clearColor[0] = ((copyColorClear >> 16) & 0xFF) / 255.0f;
		clearColor[1] = ((copyColorClear >> 8) & 0xFF) / 255.0f;
		clearColor[2] = ((copyColorClear >> 0) & 0xFF) / 255.0f;
		clearColor[3] = ((copyColorClear >> 24) & 0xFF) / 255.0f; // Alpha

		// clear the RT using the abstract interface
		// NOTE: this will clear the EDRAM surface + the actual RT that is bound there
		const bool flushToEDRAM = true;
		abstractLayer->ClearColorRenderTarget( copyRT, clearColor, flushToEDRAM );
	}

	// Clear the depth buffer
	// NOTE: this happens AFTER resolve
	if ( depthClearEnabled )
	{
		const float clearDepthValue = (copyDepthClear & 0xFFFFFF00) / (float)0xFFFFFF00; // maps well to 0-1 range for now
		const u32 clearStencilValue = copyDepthClear & 0xFF;

		// clear the DS
		// NOTE: this will clear the EDRAM surface + the actual RT that is bound there
		const bool flushToEDRAM = true;
		abstractLayer->ClearDepthStencilRenderTarget( clearDepthValue, clearStencilValue, flushToEDRAM );
	}

	// done
	return true;
}

bool CXenonGPUState::ReportFailedDraw( const char* reason )
{
	printf( "GPU: Failed draw, reason: %hs\n", reason );
	return false;
}

bool CXenonGPUState::UpdateRenderTargets( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs )
{
	/// update state and check if it's different
	bool stateChanged = false;
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_MODECONTROL, m_rtState.regModeControl );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_SURFACE_INFO, m_rtState.regSurfaceInfo );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_COLOR_INFO, m_rtState.regColorInfo[0] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_COLOR1_INFO, m_rtState.regColorInfo[1] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_COLOR2_INFO, m_rtState.regColorInfo[2] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_COLOR3_INFO, m_rtState.regColorInfo[3] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_COLOR_MASK, m_rtState.regColorMask );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_DEPTHCONTROL, m_rtState.regDepthControl );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_STENCILREFMASK, m_rtState.regStencilRefMask );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_DEPTH_INFO, m_rtState.regDepthInfo );

	// check if state is up to date
	if ( !stateChanged )
		return true;

	// apply the state
	return ApplyRenderTargets( abstractLayer, m_rtState, m_physicalRenderWidth, m_physicalRenderHeight );
}

bool CXenonGPUState::UpdateViewportState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs )
{
	/// update state and check if it's different
	bool stateChanged = false;
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_SURFACE_INFO, m_viewState.regSurfaceInfo );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_CL_VTE_CNTL, m_viewState.regPaClVteCntl );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_SU_SC_MODE_CNTL, m_viewState.regPaSuScModeCntl );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_SC_WINDOW_OFFSET, m_viewState.regPaScWindowOffset );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_SC_WINDOW_SCISSOR_TL, m_viewState.regPaScWindowScissorTL );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_SC_WINDOW_SCISSOR_BR, m_viewState.regPaScWindowScissorBR );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_CL_VPORT_XOFFSET, m_viewState.regPaClVportXoffset );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_CL_VPORT_YOFFSET, m_viewState.regPaClVportYoffset );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_CL_VPORT_ZOFFSET, m_viewState.regPaClVportZoffset );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_CL_VPORT_XSCALE, m_viewState.regPaClVportXscale );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_CL_VPORT_YSCALE, m_viewState.regPaClVportYscale );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_CL_VPORT_ZSCALE, m_viewState.regPaClVportZscale );

	// check if state is up to date
	if ( !stateChanged )
		return true;

	// apply the state
	return ApplyViewportState( abstractLayer, m_viewState, m_physicalRenderWidth, m_physicalRenderHeight );
}

bool CXenonGPUState::UpdateRasterState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs )
{
	/// update state and check if it's different
	bool stateChanged = false;
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_SU_SC_MODE_CNTL, m_rasterState.regPaSuScModeCntl );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_SC_SCREEN_SCISSOR_TL, m_rasterState.regPaScScreenScissorTL );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_PA_SC_SCREEN_SCISSOR_BR, m_rasterState.regPaScScreenScissorBR );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_VGT_MULTI_PRIM_IB_RESET_INDX, m_rasterState.regMultiPrimIbResetIndex );

	// check if state is up to date
	if ( !stateChanged )
		return true;

	// apply the state
	return ApplyRasterState( abstractLayer, m_rasterState );
}

bool CXenonGPUState::UpdateBlendState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs )
{
	/// update state and check if it's different
	bool stateChanged = false;
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLENDCONTROL_0, m_blendState.regRbBlendControl[0] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLENDCONTROL_1, m_blendState.regRbBlendControl[1] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLENDCONTROL_2, m_blendState.regRbBlendControl[2] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLENDCONTROL_3, m_blendState.regRbBlendControl[3] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLEND_RED, m_blendState.regRbBlendRGBA[0] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLEND_GREEN, m_blendState.regRbBlendRGBA[1] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLEND_BLUE, m_blendState.regRbBlendRGBA[2] );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_BLEND_ALPHA, m_blendState.regRbBlendRGBA[3] );

	// check if state is up to date
	//if ( !stateChanged )
		//return true;

	// apply the state
	return ApplyBlendState( abstractLayer, m_blendState );
}

bool CXenonGPUState::UpdateDepthState( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs )
{
	/// update state and check if it's different
	bool stateChanged = false;
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_DEPTHCONTROL, m_depthState.regRbDepthControl );
	stateChanged |= Helper::UpdateRegister( regs, XenonGPURegister::REG_RB_STENCILREFMASK, m_depthState.regRbStencilRefMask );

	// check if state is up to date
	if ( !stateChanged )
		return true;

	// apply the state
	return ApplyDepthState( abstractLayer, m_depthState );
}

bool CXenonGPUState::ApplyRenderTargets( IXenonGPUAbstractLayer* abstractLayer, const XenonStateRenderTargetsRegisters& rtState, u32& outPhysicalWidth, u32& outPhysicalHeight )
{
	// RB_SURFACE_INFO
	// http://fossies.org/dox/MesaLib-10.3.5/fd2__gmem_8c_source.html

	// extract specific settings
	const auto enableMode = (XenonModeControl)( rtState.regModeControl & 0x7 );
	const auto surfaceMSAA = (XenonMsaaSamples)((rtState.regSurfaceInfo >> 16) & 3);
	const u32 surfacePitch = rtState.regSurfaceInfo & 0x3FFF;

	// NOTE: this setup is all f*cked, the MSAA is not supported ATM
	// NOTE: THIS WAS FOUND TO BE SET TO BULLSHIT VALUES SOMETIMES
	if (1)// enableMode == XenonModeControl::ColorDepth )
	{
		for ( u32 rtIndex=0; rtIndex<4; ++rtIndex )
		{
			const u32 rtInfo = rtState.regColorInfo[ rtIndex ];

			// no rt
			if (surfacePitch == 0)
				continue;

			// get color mask for this specific render target
			const u32 writeMask = (rtState.regColorMask >> (rtIndex * 4)) & 0xF;
			if ( !writeMask )
			{
				abstractLayer->UnbindColorRenderTarget( rtIndex );
				continue;
			}

			// get base EDRAM tile index
			const u32 memoryBase = rtInfo & 0xFFF;

			// get format of the RT
			const XenonColorRenderTargetFormat rtFormat = (XenonColorRenderTargetFormat) ( (rtInfo >> 16) & 0xF );

			// bind RT
			abstractLayer->BindColorRenderTarget( rtIndex, rtFormat, surfaceMSAA, memoryBase, surfacePitch );
			abstractLayer->SetColorRenderTargetWriteMask( rtIndex, 0 != (writeMask&1), 0 != (writeMask&2), 0 != (writeMask&4), 0 != (writeMask&8) );
		}
	}
	else
	{
		// no color render targets
		abstractLayer->UnbindColorRenderTarget( 0 );
		abstractLayer->UnbindColorRenderTarget( 1 );
		abstractLayer->UnbindColorRenderTarget( 2 );
		abstractLayer->UnbindColorRenderTarget( 3 );
	}

	// Create the depth buffer
	// TODO: MSAA!
	const bool usesDepth = (rtState.regDepthControl & 0x00000002) || (rtState.regDepthControl & 0x00000004);
	const u32 stencilWriteMask = (rtState.regStencilRefMask & 0x00FF0000) >> 16;
	const bool usesStencil = (rtState.regDepthControl & 0x00000001) || (stencilWriteMask != 0);
	if ( usesDepth || usesStencil )
	{
		// get base EDRAM tile index
		const u32 memoryBase = rtState.regDepthInfo & 0xFFF;

		// get format of the DS surface
		const XenonDepthRenderTargetFormat dsFormat = (XenonDepthRenderTargetFormat)( (rtState.regDepthInfo >> 16) & 1 );
		abstractLayer->BindDepthStencil( dsFormat, surfaceMSAA, memoryBase, surfacePitch );
	}
	else
	{
		abstractLayer->UnbindDepthStencil();
	}

	// realize the frame setup
	return abstractLayer->RealizeSurfaceSetup( outPhysicalWidth, outPhysicalHeight );
}

bool CXenonGPUState::ApplyViewportState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateViewportRegisters& viewState, const u32 physicalWidth, const u32 physicalHeight )
{
	// source:
	// http://fossies.org/dox/MesaLib-10.3.5/fd2__gmem_8c_source.html
	// http://www.x.org/docs/AMD/old/evergreen_3D_registers_v2.pdf
	// https://github.com/freedreno/mesa/blob/master/src/mesa/drivers/dri/r200/r200_state.c

	// VTX_XY_FMT = true: the incoming X, Y have already been multiplied by 1/W0
	// VTX_Z_FMT = true: the incoming Z has already been multiplied by 1/W0
	// VTX_W0_FMT = true: the incoming W0 is not 1/W0. Perform the reciprocal to get 1/W0.
	{
		const bool xyDivided = 0 != ( (viewState.regPaClVteCntl >> 8) & 1 );
		const bool zDivided = 0 != ( (viewState.regPaClVteCntl >> 9) & 1 );
		const bool wNotInverted = 0 != ( (viewState.regPaClVteCntl >> 10) & 1 );
		abstractLayer->SetViewportVertexFormat( xyDivided, zDivided, wNotInverted );
	}

	// Normalized/Unnormalized pixel coordinates 
	{
		const bool bNormalizedCoordinates = (viewState.regPaClVteCntl & 1);
		abstractLayer->SetViewportWindowScale( bNormalizedCoordinates );
		if ( !bNormalizedCoordinates )
			printf( "Not normalized!\n");
	}

	// Clipping.
	// https://github.com/freedreno/amd-gpu/blob/master/include/reg/yamato/14/yamato_genenum.h#L1587
	// bool clip_enabled = ((regs.pa_cl_clip_cntl >> 17) & 0x1) == 0;
	// bool dx_clip = ((regs.pa_cl_clip_cntl >> 19) & 0x1) == 0x1;
	// if (dx_clip) {
	//  glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
	//} else {
	//  glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
	//}

	// Window parameters.
	// http://ftp.tku.edu.tw/NetBSD/NetBSD-current/xsrc/external/mit/xf86-video-ati/dist/src/r600_reg_auto_r6xx.h
	// See r200UpdateWindow:
	// https://github.com/freedreno/mesa/blob/master/src/mesa/drivers/dri/r200/r200_state.c
	int16_t windowOffsetX = 0, windowOffsetY = 0;
	if ( (viewState.regPaSuScModeCntl >> 16) & 1)
	{
		// extract 15-bit signes values
		windowOffsetX = viewState.regPaScWindowOffset & 0x7FFF;
		windowOffsetY = (viewState.regPaScWindowOffset >> 16) & 0x7FFF;

		// restore signs
		if ( windowOffsetX & 0x4000 )
			windowOffsetX |= 0x8000;
		if ( windowOffsetY & 0x4000 )
			windowOffsetY |= 0x8000;
	}

	// Setup scissor
	{
		const u32 scissorX = (viewState.regPaScWindowScissorTL & 0x7FFF);
		const u32 scissorY = ((viewState.regPaScWindowScissorTL >> 16) & 0x7FFF);
		const u32 scissorW = (viewState.regPaScWindowScissorBR & 0x7FFF) - scissorX;
		const u32 scissorH = ((viewState.regPaScWindowScissorBR >> 16) & 0x7FFF) - scissorY;
		abstractLayer->EnableScisor( scissorX + windowOffsetX, scissorY + windowOffsetY, scissorW, scissorH );
	}

	// Setup viewport
	{
		// Whether each of the viewport settings are enabled.
		// http://www.x.org/docs/AMD/old/evergreen_3D_registers_v2.pdf
		const bool xScaleEnabled = (viewState.regPaClVteCntl & (1 << 0)) != 0;
		const bool xOffsetEnabled = (viewState.regPaClVteCntl & (1 << 1)) != 0;
		const bool yScaleEnabled = (viewState.regPaClVteCntl & (1 << 2)) != 0;
		const bool yOffsetEnabled = (viewState.regPaClVteCntl & (1 << 3)) != 0;
		const bool zScaleEnabled = (viewState.regPaClVteCntl & (1 << 4)) != 0;
		const bool zOffsetEnabled = (viewState.regPaClVteCntl & (1 << 5)) != 0;

		// They all should be enabled together
		DEBUG_CHECK( xScaleEnabled == yScaleEnabled == zScaleEnabled == xOffsetEnabled == yOffsetEnabled == zOffsetEnabled );

		// get viewport params
		const float vox = xOffsetEnabled ? viewState.regPaClVportXoffset : 0;
		const float voy = yOffsetEnabled ? viewState.regPaClVportYoffset : 0;
		const float voz = zOffsetEnabled ? viewState.regPaClVportZoffset : 0;
		const float vsx = xScaleEnabled ? viewState.regPaClVportXscale : 0;
		const float vsy = yScaleEnabled ? viewState.regPaClVportYscale : 0;
		const float vsz = zScaleEnabled ? viewState.regPaClVportZscale : 0;

		// pixel->texel offset
		const float xTexelOffset = 0.0f;
		const float yTexelOffset = 0.0f;

		// Setup actual viewport
		if ( xScaleEnabled )
		{
			const float vpw = 2 * vsx;
			const float vph = -2 * vsy;
			const float vpx = vox - vpw / 2 + windowOffsetX;
			const float vpy = voy - vph / 2 + windowOffsetY;
			abstractLayer->SetViewportRange( vpx + xTexelOffset, vpy + yTexelOffset, vpw, vph );
		}
		else
		{
			// We have no viewport information, use default

			// Determine window scale
			// HACK: no clue where to get these values.
			// RB_SURFACE_INFO
			u32 windowScaleX = 1;
			u32 windowScaleY = 1;
			const XenonMsaaSamples surfaceMsaa = (XenonMsaaSamples)( (viewState.regSurfaceInfo >> 16) & 3 );
			if ( surfaceMsaa == XenonMsaaSamples::MSSA2X )
			{
				windowScaleX = 2;
			}
			else if ( surfaceMsaa == XenonMsaaSamples::MSSA4X )
			{
				windowScaleX = 2;
				windowScaleY = 2;
			}

			// set the natural (physical) viewport size
			const float vpw = 2.0f * (float) physicalWidth;
			const float vph = 2.0f * (float) physicalHeight;
			const float vpx = -(float) physicalWidth / 1.0f;
			const float vpy = -(float) physicalHeight / 1.0f;

			abstractLayer->SetViewportRange( vpx + xTexelOffset, vpy + yTexelOffset, vpw, vph);
		}

		// Setup depth range
		if ( zScaleEnabled && zOffsetEnabled )
			abstractLayer->SetDepthRange( voz, voz + vsz );
	}

	// valid so far, see what's the abstraction layer has to say
	return abstractLayer->RealizeViewportSetup();
}

bool CXenonGPUState::ApplyRasterState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateRasterizerRegisters& rasterState )
{
	// set cull mode
	const auto cullMode = (XenonCullMode)( (rasterState.regPaSuScModeCntl & 0x3) );
	abstractLayer->SetCullMode( cullMode );

	// set face winding
	const auto frontFaceCW = (rasterState.regPaSuScModeCntl & 0x4) != 0;
	abstractLayer->SetFaceMode( frontFaceCW ? XenonFrontFace::CW :XenonFrontFace::CCW );

	// set polygon mode
	const bool bPolygonMode = ((rasterState.regPaSuScModeCntl >> 3) & 0x3) != 0;
	if ( bPolygonMode )
	{
		const auto frontMode = (XenonFillMode)( (rasterState.regPaSuScModeCntl >> 5) & 0x7 );
		const auto backMode = (XenonFillMode)( (rasterState.regPaSuScModeCntl >> 8) & 0x7 );
		DEBUG_CHECK( frontMode == backMode );

		abstractLayer->SetFillMode( frontMode );
	}
	else
	{
		abstractLayer->SetFillMode( XenonFillMode::Solid );
	}
	
	// primitive restart index - not supported yet
	const bool bPrimitiveRestart = (rasterState.regPaSuScModeCntl & (1 << 21)) != 0;
	abstractLayer->SetPrimitiveRestart( bPrimitiveRestart );
	abstractLayer->SetPrimitiveRestartIndex( rasterState.regMultiPrimIbResetIndex );

	// realize state
	return abstractLayer->RealizeRasterState();
}

bool CXenonGPUState::ApplyBlendState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateBlendRegisters& blendState )
{
	// blend color
	abstractLayer->SetBlendColor( blendState.regRbBlendRGBA[0], blendState.regRbBlendRGBA[1], blendState.regRbBlendRGBA[2], blendState.regRbBlendRGBA[3] );

	// render targets
	for ( u32 i=0; i<4; ++i )
	{		
		const u32 blendControl = blendState.regRbBlendControl[i];

		// A2XX_RB_BLEND_CONTROL_COLOR_SRCBLEND
		const auto colorSrc = (XenonBlendArg)( (blendControl & 0x0000001F) >> 0 );

		// A2XX_RB_BLEND_CONTROL_COLOR_DESTBLEND
		const auto colorDest = (XenonBlendArg)( (blendControl & 0x00001F00) >> 8 );

		// A2XX_RB_BLEND_CONTROL_COLOR_COMB_FCN
		const auto colorOp = (XenonBlendOp)( (blendControl & 0x000000E0) >> 5 );

		// A2XX_RB_BLEND_CONTROL_ALPHA_SRCBLEND
		const auto alphaSrc = (XenonBlendArg)( (blendControl & 0x001F0000) >> 16 );

		// A2XX_RB_BLEND_CONTROL_ALPHA_DESTBLEND
		const auto alphaDest = (XenonBlendArg)( (blendControl & 0x1F000000) >> 24 );

		// A2XX_RB_BLEND_CONTROL_ALPHA_COMB_FCN
		const auto alphaOp = (XenonBlendOp)( (blendControl & 0x00E00000) >> 21 );

		// is blending enabled ?
		const bool isColorSolid = ( (colorSrc == XenonBlendArg::One) && (colorDest == XenonBlendArg::Zero) && (colorOp == XenonBlendOp::Add) );
		const bool isAlphaSolid = ( (alphaSrc == XenonBlendArg::One) && (alphaDest == XenonBlendArg::Zero) && (alphaOp == XenonBlendOp::Add) );
		if ( isColorSolid && isAlphaSolid )
		{
			abstractLayer->SetBlend( i, false );
		}
		else
		{
			abstractLayer->SetBlend( i, true );
			abstractLayer->SetBlendOp( i, colorOp, alphaOp );
			abstractLayer->SetBlendArg( i, colorSrc, colorDest, alphaSrc, alphaDest );
		}
	}

	// realize state via the D3D11 interface
	return abstractLayer->RealizeBlendState();
}

bool CXenonGPUState::ApplyDepthState( IXenonGPUAbstractLayer* abstractLayer, const XenonStateDepthStencilRegisters& depthState )
{
	// A2XX_RB_DEPTHCONTROL_Z_ENABLE
	const bool bDepthTest = (0 != (depthState.regRbDepthControl & 0x00000002));
	abstractLayer->SetDepthTest( bDepthTest );

	// A2XX_RB_DEPTHCONTROL_Z_WRITE_ENABLE
	const bool bDepthWrite = (0 != (depthState.regRbDepthControl & 0x00000004));
	abstractLayer->SetDepthWrite( bDepthWrite );

	// A2XX_RB_DEPTHCONTROL_EARLY_Z_ENABLE
	// ?
	// A2XX_RB_DEPTHCONTROL_ZFUNC
	const auto depthFunc = (XenonCmpFunc)( (depthState.regRbDepthControl & 0x00000070) >> 4);
	abstractLayer->SetDepthFunc( depthFunc );

	// A2XX_RB_DEPTHCONTROL_STENCIL_ENABLE
	const bool bStencilEnabled = (0 != (depthState.regRbDepthControl & 0x00000001));
	abstractLayer->SetStencilTest( bStencilEnabled );

	// RB_STENCILREFMASK_STENCILREF
	const u32 stencilRef = (depthState.regRbStencilRefMask & 0x000000FF);
	abstractLayer->SetStencilRef( (const u8) stencilRef );

	// RB_STENCILREFMASK_STENCILMASK
	const u32 stencilReadMask = (depthState.regRbStencilRefMask & 0x0000FF00) >> 8;
	abstractLayer->SetStencilReadMask( (const u8) stencilReadMask );

	// RB_STENCILREFMASK_STENCILWRITEMASK
	const u32 stencilWriteMask = (depthState.regRbStencilRefMask & 0x00FF0000) >> 16;
	abstractLayer->SetStencilWriteMask( (const u8) stencilWriteMask );

	// A2XX_RB_DEPTHCONTROL_BACKFACE_ENABLE
	const bool bBackfaceEnabled = (depthState.regRbDepthControl & 0x00000080) != 0;
	if ( bBackfaceEnabled )
	{
		// A2XX_RB_DEPTHCONTROL_STENCILFUNC
		const auto frontFunc = (XenonCmpFunc)( (depthState.regRbDepthControl & 0x00000700) >> 8 );
		abstractLayer->SetStencilFunc( true, frontFunc );

		// A2XX_RB_DEPTHCONTROL_STENCILFAIL
		// A2XX_RB_DEPTHCONTROL_STENCILZFAIL
		// A2XX_RB_DEPTHCONTROL_STENCILZPASS
		const auto frontSfail = (XenonStencilOp)( (depthState.regRbDepthControl & 0x00003800) >> 11 );
		const auto frontDfail = (XenonStencilOp)( (depthState.regRbDepthControl & 0x000E0000) >> 17 );
		const auto frontDpass = (XenonStencilOp)( (depthState.regRbDepthControl & 0x0001C000) >> 14 );
		abstractLayer->SetStencilOps( true, frontSfail, frontDfail, frontDpass );

		// A2XX_RB_DEPTHCONTROL_STENCILFUNC_BF
		const auto backFunc = (XenonCmpFunc)( (depthState.regRbDepthControl & 0x00700000) >> 20 );
		abstractLayer->SetStencilFunc( false, backFunc );

		// A2XX_RB_DEPTHCONTROL_STENCILFAIL_BF
		// A2XX_RB_DEPTHCONTROL_STENCILZFAIL_BF
		// A2XX_RB_DEPTHCONTROL_STENCILZPASS_BF
		const auto backSfail = (XenonStencilOp)( (depthState.regRbDepthControl & 0x03800000) >> 23 );
		const auto backDfail = (XenonStencilOp)( (depthState.regRbDepthControl & 0xE0000000) >> 29 );
		const auto backDpass = (XenonStencilOp)( (depthState.regRbDepthControl & 0x1C000000) >> 26 );
		abstractLayer->SetStencilOps( false, backSfail, backDfail, backDpass );
	}
	else
	{
		// A2XX_RB_DEPTHCONTROL_STENCILFUNC
		const auto frontFunc = (XenonCmpFunc)( (depthState.regRbDepthControl & 0x00000700) >> 8 );
		abstractLayer->SetStencilFunc( true, frontFunc );
		abstractLayer->SetStencilFunc( false, frontFunc );

		// A2XX_RB_DEPTHCONTROL_STENCILFAIL
		// A2XX_RB_DEPTHCONTROL_STENCILZFAIL
		// A2XX_RB_DEPTHCONTROL_STENCILZPASS
		const auto frontSfail = (XenonStencilOp)( (depthState.regRbDepthControl & 0x00003800) >> 11 );
		const auto frontDfail = (XenonStencilOp)( (depthState.regRbDepthControl & 0x000E0000) >> 17 );
		const auto frontDpass = (XenonStencilOp)( (depthState.regRbDepthControl & 0x0001C000) >> 14 );
		abstractLayer->SetStencilOps( true, frontSfail, frontDfail, frontDpass );
		abstractLayer->SetStencilOps( false, frontSfail, frontDfail, frontDpass );
	}

	// realize state
	return abstractLayer->RealizeDepthStencilState();
}

bool CXenonGPUState::UpdateShaderConstants( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs, const CXenonGPUDirtyRegisterTracker& dirtyRegs )
{
	// pixel shader constants
	{
		const u32 firstReg = (u32) XenonGPURegister::REG_SHADER_CONSTANT_256_X;
		const u32 lastReg  = (u32) XenonGPURegister::REG_SHADER_CONSTANT_511_W;

		u32 regIndex = firstReg;
		while ( regIndex < lastReg )
		{
			const u32 baseShaderVector = (regIndex - firstReg) / 4;

			const u64 dirtyMask = dirtyRegs.GetBlock( regIndex ); // 64 registers
			if ( dirtyMask )
			{
				const float* values = &regs[ regIndex ].m_float;
				abstractLayer->SetPixelShaderConsts( baseShaderVector, 16, values );
			}

			regIndex += 64;
		}
	}

	// vertex shader constants
	{
		const u32 firstReg = (u32) XenonGPURegister::REG_SHADER_CONSTANT_000_X;
		const u32 lastReg  = (u32) XenonGPURegister::REG_SHADER_CONSTANT_255_W;

		u32 regIndex = firstReg;
		while ( regIndex < lastReg )
		{
			const u32 baseShaderVector = (regIndex - firstReg) / 4;

			const u64 dirtyMask = dirtyRegs.GetBlock( regIndex ); // 64 registers
			if ( dirtyMask )
			{
				const float* values = &regs[ regIndex ].m_float;
				abstractLayer->SetVertexShaderConsts( baseShaderVector, 16, values );
			}

			regIndex += 64;
		}
	}

	// boolean constants
	{
		const u32 firstReg = (u32) XenonGPURegister::REG_SHADER_CONSTANT_BOOL_000_031;
		const u64 dirtyMask = dirtyRegs.GetBlock( firstReg );

		if ( dirtyMask & 0xFF ) // 8 regs total
		{
			const u32* values = &regs[ firstReg ].m_dword;
			abstractLayer->SetBooleanConstants( values );
		}
	}

	// realize state
	return abstractLayer->RealizeShaderConstants();
}

bool CXenonGPUState::UpdateTexturesAndSamplers( IXenonGPUAbstractLayer* abstractLayer, const CXenonGPURegisters& regs, class IXenonGPUDumpWriter* traceDump, RAM *ram )
{
	// get the "hot" fetch slots from the bounded shaders
	// we technically could flush ALL of the textures & samples but that's just a waste
	// this is one of the places when leaking some abstraction actually helps...
	const u32 activeFetchSlotsMask = abstractLayer->GetActiveTextureFetchSlotMask();
	for ( u32 fetchSlot=0; fetchSlot<32; ++fetchSlot )
	{
		// skip if slot is not active for current shaders
		if ( !(activeFetchSlotsMask & (1 << fetchSlot) ) )
			continue;

		// get texture information from the registers
		{
			const auto fetchReg = (XenonGPURegister)( (u32)XenonGPURegister::REG_SHADER_CONSTANT_FETCH_00_0 + ((fetchSlot & 15) *6) );
			const auto& fetchInfo = regs.GetStructAt< XenonGPUTextureFetch >( fetchReg );
			
			// parse out texture format info from the GPU structure
			XenonTextureInfo textureInfo;
			if ( !XenonTextureInfo::Parse( fetchInfo, textureInfo ) )
			{
				printf( "GPU: Failed to parse texture info for fetch slot %d\n", fetchSlot );

				// bind NULL texture (default)
				abstractLayer->SetTexture( fetchSlot, nullptr );
				continue;
			}

			// set sampler
			auto samplerInfo = XenonSamplerInfo::Parse(fetchInfo);
			abstractLayer->SetSampler(fetchSlot, &samplerInfo);

			// dump texture
			if (traceDump != nullptr)
			{
				const u32 realAddress = ( textureInfo.m_address );
				const u32 realMemortySize = textureInfo.CalculateMemoryRegionSize();
				traceDump->MemoryAccessRead( reinterpret_cast<u64>(ram->GetPointerToAddress(realAddress)), realMemortySize, "Texture" );
			}

			// set actual texture to the pipeline at given fetch slot
			abstractLayer->SetTexture( fetchSlot, &textureInfo );
		}
	}

	// valid
	return true;
}

