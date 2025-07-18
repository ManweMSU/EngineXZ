﻿#pragma once

#include "xv_manual.h"

namespace Engine
{
	namespace XV
	{
		enum CompilerFlags : uint {
			CompilerFlagMakeModule		= 0x00001,
			CompilerFlagMakeMetadata	= 0x00002,
			CompilerFlagMakeManual		= 0x00004,
			CompilerFlagSystemConsole	= 0x00010,
			CompilerFlagSystemGUI		= 0x00020,
			CompilerFlagSystemNull		= 0x00040,
			CompilerFlagSystemLibrary	= 0x00080,
			CompilerFlagVersionControl	= 0x00100,
			CompilerFlagDebugData		= 0x00200,
			CompilerFlagUseModuleCache	= 0x00400,
			CompilerFlagLanguageV		= 0x00000,
			CompilerFlagLanguageW		= 0x10000,
			CompilerFlagLanguageMask	= 0xF0000,
		};
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
			ExpressionMustBeValue	= 0x0011,
			InvalidThrowPlace		= 0x0012,
			FunctionMustBeInstance	= 0x0013,
			InvalidFunctionTrats	= 0x0014,
			MustBeLocalClass		= 0x0015,
			InvalidAutoVariable		= 0x0016,
			InvalidLoopCtrlPlace	= 0x0017,
			InvalidParentClass		= 0x0018,
			InvalidInterfaceClass	= 0x0019,
			InternalError			= 0xFFFF,
		};
		struct CompilerStatusDesc
		{
			CompilerStatus status;
			string error_line;
			int error_line_no, error_line_pos, error_line_len;
		};

		enum class CodeRangeTag {
			NoData, Comment, Keyword, Punctuation, Prototype,
			LiteralBoolean, LiteralNumeric, LiteralString, LiteralNull,
			IdentifierUnknown, IdentifierNamespace,
			IdentifierType, IdentifierPrototype,
			IdentifierFunction, IdentifierConstant, IdentifierVariable,
			IdentifierProperty, IdentifierField
		};
		enum CodeRangeFlags {
			CodeRangeSymbolDefinition	= 0x01,
			CodeRangeSymbolLocal		= 0x02,
			CodeRangeClauseOpen			= 0x04,
			CodeRangeClauseClose		= 0x08
		};
		struct CodeRangeInfo
		{
			int from, length;
			CodeRangeTag tag;
			string path, identifier, type, value;
			uint flags;
		};
		struct ArgumentInfo
		{
			CodeRangeTag tag;
			string type;
		};
		struct FunctionOverloadInfo
		{
			string identifier, path;
			ArgumentInfo retval;
			Volumes::List<ArgumentInfo> args;
		};
		struct CodeMetaInfo
		{
			Volumes::Dictionary<int, CodeRangeInfo> info;
			Volumes::Dictionary<string, CodeRangeTag> autocomplete;
			Volumes::List<FunctionOverloadInfo> overloads;
			int autocomplete_at;
			int function_info_at, function_info_argument;
			int error_absolute_from;
		};

		class ICompilerCallback : public Object
		{
		public:
			virtual Streaming::Stream * QueryModuleFileStream(const string & module_name) = 0;
			virtual Streaming::Stream * QueryResourceFileStream(const string & resource_file_name) = 0;
			virtual void QueryAvailableModules(Volumes::Set<string> & set) = 0;
		};
		class IOutputModule : public Object
		{
		public:
			virtual string GetOutputModuleName(void) const = 0;
			virtual string GetOutputModuleExtension(void) const = 0;
			virtual Streaming::Stream * GetOutputModuleData(void) const = 0;
		};
		ICompilerCallback * CreateCompilerCallback(const string * res_pv, int res_pc, const string * mdl_pv, int mdl_pc, ICompilerCallback * dropback, uint lm = CompilerFlagLanguageV);

		struct CompileDesc
		{
			// Input data
			uint flags;
			string module_name;
			string source_full_path;
			const Array<uint32> * input;
			ICompilerCallback * callback;
			Volumes::Set<string> imports;
			Volumes::Dictionary<string, string> defines;
			SafePointer<Object> module_cache;
			CodeMetaInfo * meta;
			// Output data
			CompilerStatusDesc status;
			SafePointer<IOutputModule> output_module;
			SafePointer<ManualVolume> output_volume;
		};

		void Compile(CompileDesc & desc);

		void MakeManual(const string & module_name, const Array<uint32> & input, ManualVolume ** output, ICompilerCallback * callback, CompilerStatusDesc & status, uint lm = CompilerFlagLanguageV, const Volumes::Set<string> * imports = 0);
		void MakeManual(const string & input, ManualVolume ** output, ICompilerCallback * callback, CompilerStatusDesc & status, const Volumes::Set<string> * imports = 0);
		void CompileModule(const string & module_name, const Array<uint32> & input, IOutputModule ** output, ICompilerCallback * callback, CompilerStatusDesc & status, uint lm = CompilerFlagLanguageV, CodeMetaInfo * meta = 0, const Volumes::Set<string> * imports = 0);
		void CompileModule(const string & input, string & output_path, ICompilerCallback * callback, CompilerStatusDesc & status, const Volumes::Set<string> * imports = 0);
	}
}