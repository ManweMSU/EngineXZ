#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XV
	{
		enum class CompilerStatus : uint {
			Success					= 0x0000,
			FileAccessFailure		= 0x0001,
			ReferenceAccessFailure	= 0x0002,
			InvalidTokenInput		= 0x0003,
			AnotherTokenExpected	= 0x0004,
			SymbolRedefinition		= 0x0005,
			InapproptiateAttribute	= 0x0006,
			InvalidSubsystem		= 0x0007,
			InvalidOperatorName		= 0x0008,
			NoSuchSymbol			= 0x0009,
			NoSuchOverload			= 0x000A,
			ObjectTypeMismatch		= 0x000B,
			AssemblyError			= 0x000C,
			ExpressionMustBeConst	= 0x000D,
			ResourceFileNotFound	= 0x000E,
			InvalidPictureFormat	= 0x000F,
			InvalidResourceType		= 0x0010,
			
			// TODO: IMPLEMENT

			InternalError			= 0xFFFF,
		};
		struct CompilerStatusDesc
		{
			CompilerStatus status;
			string error_line;
			int error_line_no, error_line_pos, error_line_len;
		};
		class ICompilerCallback : public Object
		{
		public:
			virtual Streaming::Stream * QueryModuleFileStream(const string & module_name) = 0;
			virtual Streaming::Stream * QueryResourceFileStream(const string & resource_file_name) = 0;
		};
		class IOutputModule : public Object
		{
		public:
			virtual string GetOutputModuleName(void) const = 0;
			virtual string GetOutputModuleExtension(void) const = 0;
			virtual Streaming::Stream * GetOutputModuleData(void) const = 0;
		};
		ICompilerCallback * CreateCompilerCallback(const string * res_pv, int res_pc, const string * mdl_pv, int mdl_pc, ICompilerCallback * dropback);
		void CompileModule(const string & module_name, const Array<uint32> & input, IOutputModule ** output, ICompilerCallback * callback, CompilerStatusDesc & status);
		void CompileModule(const string & input, string & output_path, ICompilerCallback * callback, CompilerStatusDesc & status);
	}
}