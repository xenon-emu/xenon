#pragma once

#include "Core/RAM/RAM.h"
#include "xenonGPUConstants.h"

#define XENON_GPU_MAKE_SWIZZLE(x, y, z, w)								\
	(((eXenonGPUSwizzle_##x) << 0) | ((eXenonGPUSwizzle_##y) << 3) |	\
	((eXenonGPUSwizzle_##z) << 6) | ((eXenonGPUSwizzle_##w) << 9))

// swizzle combinations used on GPU
enum XenonGPUSwizzle
{
	eXenonGPUSwizzle_X = 0,
	eXenonGPUSwizzle_R = 0,
	eXenonGPUSwizzle_Y = 1,
	eXenonGPUSwizzle_G = 1,
	eXenonGPUSwizzle_Z = 2,
	eXenonGPUSwizzle_B = 2,
	eXenonGPUSwizzle_W = 3,
	eXenonGPUSwizzle_A = 3,
	eXenonGPUSwizzle_0 = 4,
	eXenonGPUSwizzle_1 = 5,

	eXenonGPUSwizzle_RGBA = XENON_GPU_MAKE_SWIZZLE(R, G, B, A),
	eXenonGPUSwizzle_BGRA = XENON_GPU_MAKE_SWIZZLE(B, G, R, A),
	eXenonGPUSwizzle_RGB1 = XENON_GPU_MAKE_SWIZZLE(R, G, B, 1),
	eXenonGPUSwizzle_BGR1 = XENON_GPU_MAKE_SWIZZLE(B, G, R, 1),
	eXenonGPUSwizzle_000R = XENON_GPU_MAKE_SWIZZLE(0, 0, 0, R),
	eXenonGPUSwizzle_RRR1 = XENON_GPU_MAKE_SWIZZLE(R, R, R, 1),
	eXenonGPUSwizzle_R111 = XENON_GPU_MAKE_SWIZZLE(R, 1, 1, 1),
	eXenonGPUSwizzle_R000 = XENON_GPU_MAKE_SWIZZLE(R, 0, 0, 0),

};

inline u16 XenonGPUSwap16( u16 value, XenonGPUEndianFormat format )
{
	switch (format)
	{
		// no swap
		case XenonGPUEndianFormat::FormatUnspecified:
			return value;

		// pair swap
		case XenonGPUEndianFormat::Format8in16:
			return ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0x00FF00FF);
	}

	DEBUG_CHECK( !"Unknown endianness format for 16-bit value" );
	return value;
}

inline u32 XenonGPUSwap32( u32 value, XenonGPUEndianFormat format )
{
	switch (format)
	{
		// no swap
		case XenonGPUEndianFormat::FormatUnspecified:
			return value;

		// bytes in half word swap
		case XenonGPUEndianFormat::Format8in16:
			return ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0x00FF00FF);

		// bytes in 32-bit value (full swap)
		case XenonGPUEndianFormat::Format8in32:
			return _byteswap_ulong( value );

		// swap half words
		case XenonGPUEndianFormat::Format16in32:
			return ((value >> 16) & 0xFFFF) | (value << 16);
	}

	DEBUG_CHECK( !"Unknown endianness format for 32-bit value" );
	return value;
}

inline float XenonGPUSwapFloat( float value, XenonGPUEndianFormat format )
{
	union {
		u32 u;
		float f;
	} v;
	v.f = value;
	v.u = XenonGPUSwap32( v.u, format );
	return v.f;
}

inline u32 XenonGPUAddrToCPUAddr( const u32 addr )
{
	return addr;
}

inline u32 XenonCPUAddrToGPUAddr( const u32 addr )
{
	return addr & 0x1FFFFFFF;
}

inline u32 XenonGPULoadPhysical(RAM *ram, const u32 addr )
{
	const u32 cpuAddr = ( addr );
	return *(const u32*) ram->GetPointerToAddress(cpuAddr);
}

inline u32 XenonGPULoadPhysical(RAM* ram, const u32 addr, const XenonGPUEndianFormat format )
{
	const u32 cpuAddr = ( addr );
	return XenonGPUSwap32( *(const u32*)ram->GetPointerToAddress(cpuAddr), format );
}

inline u32 XenonGPULoadPhysicalAddWithFormat(RAM* ram, const u32 addrWithFormat )
{
	const XenonGPUEndianFormat format = static_cast< XenonGPUEndianFormat >( addrWithFormat & 0x3 );
	const u32 cpuAddr = ( addrWithFormat & ~0x3 );
	return XenonGPUSwap32( *(const u32*)ram->GetPointerToAddress(cpuAddr), format );
}

inline void XenonGPUStorePhysical(RAM* ram, const u32 addr, const u32 value )
{
	const u32 cpuAddr = ( addr );
	*(u32*)ram->GetPointerToAddress(cpuAddr) = value;
}

inline void XenonGPUStorePhysical(RAM* ram, const u32 addr, const u32 value, const XenonGPUEndianFormat format )
{
	const u32 cpuAddr = ( addr );
	*(u32*)ram->GetPointerToAddress(cpuAddr) = XenonGPUSwap32( value, format );
}

inline void XenonGPUStorePhysicalAddrWithFormat(RAM* ram, const u32 addrWithFormat, const u32 value )
{
	const XenonGPUEndianFormat format = static_cast< XenonGPUEndianFormat >( addrWithFormat & 0x3 );
	const u32 cpuAddr = ( addrWithFormat & ~0x3 );
	*(u32*)ram->GetPointerToAddress(cpuAddr) = XenonGPUSwap32( value, format );
}

extern u32 XenonGPUCalcCRC( const void* memory, const u32 memorySize );

extern u64 XenonGPUCalcCRC64( const void* memory, const u32 memorySize );

extern const char* XenonGPUGetColorRenderTargetFormatName( const XenonColorRenderTargetFormat format );

extern const char* XenonGPUGetDepthRenderTargetFormatName( const XenonDepthRenderTargetFormat format );

extern const char* XenonGPUGetMSAAName( const XenonMsaaSamples msaa );

extern const char* XenonGPUTextureFormatName( const XenonTextureFormat format );

//-----------------

/// copy pasted from Inferno engine shader language
/// TODO: find better place for this stuff

/// Abstract code statement (no result value)
class ICodeStatement
{
protected:
	virtual ~ICodeStatement() {};

public:
	virtual ICodeStatement* Copy() = 0;
	virtual void Release() = 0;
	virtual std::string ToString() const = 0; // debug only

	template< typename T >
	T* CopyWithType()
	{
		return static_cast<T*>( Copy() );
	}
};

/// Abstract code expression (has value)
class ICodeExpr
{
protected:
	virtual ~ICodeExpr() {};

public:
	virtual ICodeExpr* Copy() = 0;
	virtual void Release() = 0;
	virtual std::string ToString() const = 0; // debug only

	template< typename T >
	T* CopyWithType()
	{
		return static_cast<T*>( Copy() );
	}
};

/// Safe wrapper for code expression objects
class CodeExpr
{
public:
	CodeExpr();
	CodeExpr( const CodeExpr& other );
	CodeExpr( ICodeExpr* ptrFromNew );
	~CodeExpr();

	CodeExpr& operator=( const CodeExpr& other );

	std::string ToString() const;

	inline ICodeExpr* GetRaw() const { return m_expr; }
	inline operator bool() const { return m_expr != nullptr; }

	template< typename T >
	inline T* Get() const { return static_cast< T* >( m_expr ); }

private:
	ICodeExpr*		m_expr;
};

/// Safe wrapper for code statement objects
class CodeStatement
{
public:
	CodeStatement();
	CodeStatement( const CodeStatement& other );
	CodeStatement( ICodeStatement* ptrFromNew );
	~CodeStatement();

	CodeStatement& operator=( const CodeStatement& other );

	std::string ToString() const;

	inline ICodeStatement* GetRaw() const { return m_statement; }
	inline operator bool() const { return m_statement != nullptr; }

	template< typename T >
	inline T* Get() const { return static_cast< T* >( m_statement ); }

private:
	ICodeStatement*		m_statement;
};

//-----------------

class CodeChunk
{
public:
	CodeChunk();
	CodeChunk( const CodeChunk& other );
	CodeChunk( const char* txt );
	~CodeChunk();

	CodeChunk& operator=( const char* txt );
	CodeChunk& operator=( const CodeChunk& other );

	void Set( const char* txt );

	CodeChunk& Append( const char* txt );
	CodeChunk& Append( const CodeChunk& txt );
	CodeChunk& Appendf( const char* txt, ... );

	inline const char* c_str() const
	{
		if ( !m_longBuf.empty() )
			return m_longBuf.c_str();

		return m_buf;
	}

private:
	static const u32 MAX_LEN = 63;

	char			m_buf[ MAX_LEN+1 ];
	std::string		m_longBuf;
	u32			m_bufLength;
};

//-----------------

namespace Helper
{
	static const u32 RoundUp( const u32 value, const u32 step )
	{
		return (((value + (step-1)) / step) * step);
	}

	static inline u32 Log2Ceil( u32 v )
	{
		return 32 - __lzcnt( v-1 );
	}
}

//-----------------

struct XenonRect2D
{
	int x, y, w, h;

	inline const bool operator==(const XenonRect2D& other) const
	{
		return (x == other.x) && (y == other.y) && (w == other.w) && (h == other.h);
	}		 
};

//-----------------
