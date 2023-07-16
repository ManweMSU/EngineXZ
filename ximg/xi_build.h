#pragma once

#include "xi_module.h"
#include "../xasm/xa_compiler.h"

namespace Engine
{
	namespace XI
	{
		constexpr const widechar * BuilderStamp = L"Engine XI Structor";
		constexpr uint BuilderVersionMajor = 1;
		constexpr uint BuilderVersionMinor = 0;
		constexpr uint BuilderSubversion = 0;
		constexpr uint BuilderBuildNumber = 1;

		enum class BuilderStatus : uint {
			Success					= 0x0000,
			CompilationError		= 0x0001,
			TextRegistryError		= 0x0002,
			FileReadError			= 0x0003,
			InternalError			= 0xFFFF,
		};
		struct BuilderStatusDesc
		{
			BuilderStatus status;
			string file;
			XA::CompilerStatusDesc com_stat;
		};
		void BuildModule(const string & scheme_file, Module & dest, BuilderStatusDesc & status);
	}
}