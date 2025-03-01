#pragma once

#include "xw_types.h"

namespace Engine
{
	namespace XW
	{
		enum DecompilerFlags : uint {
			DecompilerFlagVersionControl	= 0x001,
			DecompilerFlagHumanReadable		= 0x002,
			DecompilerFlagHumanReadableC	= 0x004,
			DecompilerFlagForThisMachine	= 0x008,
			DecompilerFlagRawOutput			= 0x010,
			DecompilerFlagBundleToEGSU		= 0x020,
			DecompilerFlagBundleToXO		= 0x040,
			DecompilerFlagBundleToCXX		= 0x080,
			DecompilerFlagProduceHLSL		= 0x100,
			DecompilerFlagProduceMSL		= 0x200,
			DecompilerFlagProduceGLSL		= 0x400,
			DecompilerFlagProduceCXX		= 0x800,
			DecompilerFlagProduceMaximas	= DecompilerFlagProduceHLSL | DecompilerFlagProduceMSL,
		};
		enum class DecompilerStatus : uint {
			Success					= 0x0000,
			XModuleNotFound			= 0x0001,
			WModuleNotFound			= 0x0002,
			WModuleIncompatible		= 0x0003,
			InvalidImage			= 0x0004,
			SymbolNotFound			= 0x0005,
			DecompilationError		= 0x0006,
			NotSupported			= 0x0007,
			InvalidSemantics		= 0x0008,
			NoMandatorySemantics	= 0x0009,
			BadShaderName			= 0x000A,
			DuplicateShaderName		= 0x000B,
			RecursiveDependencies	= 0x000C,
			BadResourceTypeUsage	= 0x000D,
			InternalError			= 0xFFFF,
		};
		enum class PortionClass : uint { Unknown = 0, HLSL = 1, MSL = 2, GLSL = 3, H = 4, CXX = 5, EGSU = 6, EGSO = 7, XO = 8 };
		struct DecompilerStatusDesc
		{
			DecompilerStatus status;
			ShaderLanguage language;
			string object, addendum;
		};
		class IDecompilerCallback : public Object
		{
		public:
			virtual Streaming::Stream * QueryXModuleFileStream(const string & module_name) = 0;
			virtual Streaming::Stream * QueryWModuleFileStream(const string & module_name) = 0;
		};
		class IOutputPortion : public Object
		{
		public:
			virtual string GetPortionPostfix(void) const = 0;
			virtual string GetPortionExtension(void) const = 0;
			virtual PortionClass GetPortionClass(void) const = 0;
			virtual Streaming::Stream * GetPortionData(void) const = 0;
		};
		struct DecompileDesc
		{
			// Input data
			uint flags;
			ObjectArray<DataBlock> root_modules;
			IDecompilerCallback * callback;
			// Output data
			DecompilerStatusDesc status;
			ObjectArray<IOutputPortion> output_objects;
		};

		IDecompilerCallback * CreateDecompilerCallback(const string * xmdl_pv, int xmdl_pc, const string * wmdl_pv, int wmdl_pc, IDecompilerCallback * dropback);
		IOutputPortion * CreatePortion(const string & postfix, const string & extension, PortionClass cls, DataBlock * data);
		void Decompile(DecompileDesc & desc);
	}
}