#pragma once

#include "xa_types.h"

namespace Engine
{
	namespace XA
	{
		enum class CompilerStatus : uint {
			Success					= 0x0000,
			InvalidToken			= 0x0001,
			AnotherTokenExpected	= 0x0002,
			DataSizeNotSupported	= 0x0003,
			CallInputLengthMismatch	= 0x0004,
			InternalError			= 0xFFFF,
		};
		struct CompilerStatusDesc
		{
			CompilerStatus status;
			string error_line;
			int error_line_no, error_line_pos, error_line_len;
		};
		void CompileFunction(const string & input, Function & output, CompilerStatusDesc & result);
	}
}