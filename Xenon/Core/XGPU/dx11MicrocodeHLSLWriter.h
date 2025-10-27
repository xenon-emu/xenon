#pragma once

//----------------------------------------------------------------------------

namespace DX11Microcode
{
	class IHLSLWriter
	{
	public:
		virtual ~IHLSLWriter() {};

		virtual CodeChunk GetExportDest( const ExprWriteExportReg::EExportReg reg ) = 0;
		virtual CodeChunk GetReg( u32 regIndex ) = 0;
		virtual CodeChunk GetBoolVal( const u32 boolRegIndex ) = 0;
		virtual CodeChunk GetFloatVal( const u32 floatRegIndex ) = 0;
		virtual CodeChunk GetFloatValRelative( const u32 floatRegOffset ) = 0;
		virtual CodeChunk GetPredicate() = 0;

		virtual CodeChunk FetchVertex( CodeChunk src, const ExprVertexFetch& fetchInstr ) = 0;
		virtual CodeChunk FetchTexture( CodeChunk src, const ExprTextureFetch& fetchInstr ) = 0;

		virtual CodeChunk AllocLocalVector( CodeChunk initCode ) = 0;
		virtual CodeChunk AllocLocalScalar( CodeChunk initCode ) = 0;
		virtual CodeChunk AllocLocalBool( CodeChunk initCode ) = 0;

		virtual void BeingCondition( CodeChunk condition ) = 0;
		virtual void EndCondition() = 0;

		virtual void BeginControlFlow( const u32 address, const bool bHasJumps, const bool bHasCalls, const bool bIsCalled ) = 0;
		virtual void EndControlFlow() = 0;

		virtual void BeginBlockWithAddress( const u32 address ) = 0;
		virtual void EndBlockWithAddress() = 0;		

		virtual void ControlFlowEnd() = 0;
		virtual void ControlFlowReturn( const u32 targetAddress ) = 0;
		virtual void ControlFlowCall( const u32 targetAddress ) = 0;
		virtual void ControlFlowJump( const u32 targetAddress ) = 0;

		virtual void SetPredicate( CodeChunk newValue ) = 0;
		virtual void PushPredicate( CodeChunk newValue ) = 0;
		virtual void PopPredicate() = 0;

		virtual void Assign( CodeChunk dest, CodeChunk src ) = 0;
		virtual void Emit(CodeChunk src) = 0;
	};
}

//----------------------------------------------------------------------------
