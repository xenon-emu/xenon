#pragma once

/// based on AMD Driver implementation
// https://github.com/freedreno/amd-gpu/blob/master/

/// Known registers
enum class XenonGPURegister : u32
{
#define DECLARE_XENON_GPU_REGISTER(index, type, name) REG_##name = index,
#include "xenonGPURegisterMap.h"
#undef DECLARE_XENON_GPU_REGISTER	
};

/// GPU Register map
class CXenonGPURegisters
{
public:
	CXenonGPURegisters();

	enum Type
	{
		eType_Unknown,
		eType_Float,
		eType_Dword,
	};

	struct Info
	{
		const char*		m_name;
		Type			m_type;
	};

	union Value
	{
		float			m_float;
		u32			m_dword;
	};

	// number of registers in the GPU
	static const u32 NUM_REGISTER_RAWS = 0x5003;

	// get info about GPU register
	static const Info& GetInfo( const u32 index );

	// get/set value
	inline Value& operator[]( const u32 index ) { return m_values[index]; }
	inline const Value& operator[]( const u32 index ) const { return m_values[index]; }

	// get/set for given named register
	inline Value& operator[]( const XenonGPURegister index ) { return m_values[(u32)index]; }
	inline const Value& operator[]( const XenonGPURegister index ) const { return m_values[(u32)index]; }	

	// get ptr at
	template< typename T >
	inline const T& GetStructAt( const XenonGPURegister index ) const { return *( const T*) &m_values[(u32)index]; }

private:
	Value	m_values[ NUM_REGISTER_RAWS ];
};

/// Helper to track dirty registers
class CXenonGPUDirtyRegisterTracker
{
public:
	CXenonGPUDirtyRegisterTracker();
	~CXenonGPUDirtyRegisterTracker();

	void ClearAll();
	void SetAll();

	inline void Set( const u32 index )
	{
		const u64 mask = 1ULL << (index & BIT_MASK);
		m_mask[ index / BIT_COUNT ] |= mask;
	}

	inline bool Get( const u32 index ) const
	{
		const u64 mask = 1ULL << (index & BIT_MASK);
		return 0 != (m_mask[ index / BIT_COUNT ] & mask);
	}

	inline u64 GetBlock( const u32 firstIndex ) const
	{
		DEBUG_CHECK( (firstIndex & BIT_MASK) == 0 );
		return m_mask[ firstIndex / BIT_COUNT ];
	}

	inline u64 GetAndClear( const u32 firstIndex )
	{
		DEBUG_CHECK( (firstIndex & BIT_MASK) == 0 );
		u64 prev = m_mask[ firstIndex / BIT_COUNT ];
		m_mask[ firstIndex / BIT_COUNT ] = 0;
		return prev;
	}

private:
	static const u32 NUM_REGISTER_RAWS = CXenonGPURegisters::NUM_REGISTER_RAWS;
	static const u32 BIT_COUNT = 64;
	static const u32 BIT_MASK = BIT_COUNT-1;

	u64		m_mask[ (NUM_REGISTER_RAWS+(BIT_COUNT-1)) / BIT_COUNT ];
};

union XenonGPUVertexFetchData
{
	struct
	{
		u32 type               : 2;
		u32 address            : 30;
		u32 endian             : 2;
		u32 size               : 24;
		u32 unk1               : 6;
	};
	struct
	{
		u32 dword_0;
		u32 dword_1;
	};
};

union XenonGPUProgramCntl
{
	struct 
	{
		u32 vs_regs : 6;
		u32 unk_0 : 2;
		u32 ps_regs : 6;
		u32 unk_1 : 2;
		u32 vs_resource : 1;
		u32 ps_resource : 1;
		u32 param_gen : 1;
		u32 unknown0 : 1;
		u32 vs_export_count : 4;
		u32 vs_export_mode : 3;
		u32 ps_export_depth : 1;
		u32 ps_export_count : 3;
		u32 gen_index_vtx : 1;
	};

	u32 dword_0;
};
