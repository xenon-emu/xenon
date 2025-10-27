#pragma once

#include "xenonGPUConstants.h"

namespace ucode
{
	union instr_fetch_t;
	union instr_cf_t;
}

/// Debug (print only) decompiler for Xbox360 shaders
class CDX11MicrocodeDecompiler
{
public:
	CDX11MicrocodeDecompiler( XenonShaderType shaderType );
	~CDX11MicrocodeDecompiler();

	class ICodeWriter
	{
	public:
		virtual ~ICodeWriter() {};

		virtual void Append( const char* txt ) = 0;
		virtual void Appendf( const char* txt, ... ) = 0;
	};

	/// Decompile shader
	void DisassembleShader( ICodeWriter& writer, const u32* words, const u32 numWords );

private:
	struct VectorInstr
	{
		u32			m_numSources;
		const char*		m_name;
	};

	struct ScalarInstr
	{
		u32			m_numSources;
		const char*		m_name;
	};

	struct FetchType
	{
		const char*		m_name;
	};

	struct FetchInstr
	{
		const char*		m_name;
		int32_t			m_type;
	};

	struct FlowInstr
	{
		const char*		m_name;
		int32_t			m_type;
	};

	static const char*	st_tabLevels[]; // tab levels
	static const char	st_chanNames[]; // channel names
	static VectorInstr	st_vectorInstructions[0x20]; // vector instruction names
	static ScalarInstr	st_scalarInstructions[0x40]; // scalar instruction names
	static FetchType	st_fetchTypes[0xFF]; // data types for fetch instruction
	static FetchInstr	st_fetchInstr[28]; // fetch instruction
	static FlowInstr	st_flowInstr[]; // control flow instructions

	XenonShaderType			m_shaderType;

	void PrintSrcReg( ICodeWriter& writer, const u32 num, const u32 type, const u32 swiz, const u32 negate, const u32 abs_constants );
	void PrintDestReg( ICodeWriter& writer, const u32 num, const u32 mask, const u32 destExp );
	void PrintExportComment( ICodeWriter& writer, const u32 num );
	void PrintFetchDest( ICodeWriter& writer, const u32 dstReg, const u32 dstSwiz );
	void PrintFetchVtx( ICodeWriter& writer, const ucode::instr_fetch_t* fetch );
	void PrintFetchTex( ICodeWriter& writer, const ucode::instr_fetch_t* fetch );

	void PrintFlowNop( ICodeWriter& writer, const ucode::instr_cf_t* cf );
	void PrintFlowExec( ICodeWriter& writer, const ucode::instr_cf_t* cf );
	void PrintFlowLoop( ICodeWriter& writer, const ucode::instr_cf_t* cf );
	void PrintFlowJmpCall( ICodeWriter& writer, const ucode::instr_cf_t* cf );
	void PrintFlowAlloc( ICodeWriter& writer, const ucode::instr_cf_t* cf );
	void PrintFlow( ICodeWriter& writer, const ucode::instr_cf_t* cf, const u32 level );

	void DisasembleALU( ICodeWriter& writer, const u32* words, const u32 aluOff, const u32 level, const u32 sync );
	void DisasembleFETCH( ICodeWriter& writer, const u32* words, const u32 aluOff, const u32 level, const u32 sync );
	void DisasembleEXEC( ICodeWriter& writer, const u32* words, const u32 numWords, const u32 level, const ucode::instr_cf_t* cf );
};

