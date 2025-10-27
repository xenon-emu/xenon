#pragma once

#include "xenonGPUConstants.h"

#pragma pack(push, 4)
union XenonGPUTextureFetch
{
	struct
	{
		u32 type               : 2;  // dword_0
		u32 sign_x             : 2;
		u32 sign_y             : 2;
		u32 sign_z             : 2;
		u32 sign_w             : 2;
		u32 clamp_x            : 3;
		u32 clamp_y            : 3;
		u32 clamp_z            : 3;
		u32 unk0               : 3;
		u32 pitch              : 9;
		u32 tiled              : 1;
		u32 format             : 6;  // dword_1
		u32 endianness         : 2;
		u32 unk1               : 4;
		u32 address            : 20;
		union {                           // dword_2
			struct {
				u32 width          : 24;
				u32 unused         : 8;
			} size_1d;
			struct {
				u32 width          : 13;
				u32 height         : 13;
				u32 unused         : 6;
			} size_2d;
			struct {
				u32 width          : 13;
				u32 height         : 13;
				u32 depth          : 6;
			} size_stack;
			struct {
				u32 width          : 11;
				u32 height         : 11;
				u32 depth          : 10;
			} size_3d;
		};
		u32 unk3_0             :  1; // dword_3
		u32 swizzle            :  12; // xyzw, 3b each (XE_GPU_SWIZZLE)
		u32 unk3_1             :  6;
		u32 mag_filter         :  2;
		u32 min_filter         :  2;
		u32 mip_filter         :  2;
		u32 unk3_2             :  6;
		u32 border             :  1;
		u32 unk4_0             :  2; // dword_4
		u32 mip_min_level      :  4;
		u32 mip_max_level      :  4;
		u32 unk4_1             : 22;
		u32 unk5               :  9;  // dword_5
		u32 dimension          :  2;
		u32 unk5b              : 21;
	};

	struct{
		u32 dword_0;
		u32 dword_1;
		u32 dword_2;
		u32 dword_3;
		u32 dword_4;
		u32 dword_5;
	};
};
#pragma pack(pop)

struct XenonTextureFormatInfo
{
	XenonTextureFormat		m_format;
	XenonTextureFormatType	m_type;
	u32					m_blockWidth;
	u32					m_blockHeight;;
	u32					m_bitsPerBlock;

	const u32 GetBlockSizeInBytes() const;

	static const XenonTextureFormatInfo* Get( const u32 gpuFormat );
};

struct XenonTextureInfo
{
	u32							m_address;			// address in physical memory
	u32							m_swizzle;			// texture swizzling
	XenonTextureDimension			m_dimension;		// texture dimensions (1D, 2D, 3D, Cube)
	u32							m_width;			// width of texture data (as rendered)
	u32							m_height;			// height of the texture data (as rendered)
	u32							m_depth;			// depth of the texture data (as rendered)
	const XenonTextureFormatInfo*	m_format;			// information about the data format
	XenonGPUEndianFormat			m_endianness;		// endianess (BE or LE)
	bool							m_isTiled;			// is the texture tiles ? (affects upload)

	inline const bool IsCompressed() const
	{
		return m_format->m_type == XenonTextureFormatType::Compressed;
	}

	union
	{
		struct
		{
			u32	m_width;
		} m_size1D;

		struct
		{
			u32 m_logicalWidth; // size limits for rendering
			u32 m_logicalHeight; // size limits for rendering
			u32 m_actualBlockWidth; // actual memory layout blocks
			u32 m_actualBlockHeight; // actual memory layout blocks
			u32 m_actualWidth;
			u32 m_actualHeight;
			u32 m_actualPitch;
		}
		m_size2D;

		struct
		{
			u32 m_logicalWidth;
			u32 m_logicalHeight;
			u32 m_actualBlockWidth;
			u32 m_actualBlockHeight;
			u32 m_actualWidth;
			u32 m_actualHeight;
			u32 m_actualPitch;
			u32 m_actualFaceLength;
		}
		m_sizeCube;
	};

	static bool Parse( const XenonGPUTextureFetch& fetchInfo, XenonTextureInfo& outInfo );

	void GetPackedTileOffset( u32& outOffsetX, u32& outOffsetY ) const;

	static u32 TiledOffset2DOuter( const u32 y, const u32 width, const u32 log_bpp );
	static u32 TiledOffset2DInner( const u32 x, const u32 y, const u32 bpp, const u32 baseOffset );

	const u32 CalculateMemoryRegionSize() const;

	const u64 GetHash() const;

private:
	void CalculateTextureSizes1D( const XenonGPUTextureFetch& fetch );
	void CalculateTextureSizes2D( const XenonGPUTextureFetch& fetch );
	void CalculateTextureSizesCube( const XenonGPUTextureFetch& fetch );
};

// sampler definition
class XenonSamplerInfo
{
public:
	XenonSamplerInfo();

	XenonTextureFilter m_minFilter;
	XenonTextureFilter m_magFilter;
	XenonTextureFilter m_mipFilter;
	XenonClampMode m_clampU;
	XenonClampMode m_clampV;
	XenonClampMode m_clampW;
	XenonAnisoFilter m_anisoFilter;
	XenonBorderColor m_borderColor;
	float m_lodBias;

	u32 GetHash() const;

	static XenonSamplerInfo Parse(const XenonGPUTextureFetch& info);
};