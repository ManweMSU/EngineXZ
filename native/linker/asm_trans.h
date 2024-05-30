#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XN
	{
		enum class AssemblyErrorCode {
			Success					= 0,
			InvalidTokenInput		= 1,
			AnotherTokenExpected	= 2,
			UnknownInstruction		= 3,
			UnknownRegister			= 4,
			UnknownTranslatorHint	= 5,
			UnknownLabel			= 6,
			IllegalAddressMode		= 7,
			IllegalEncoding			= 8,
			SegmentOverflow			= 9,
		};
		struct AssemblyState
		{
			int initial_offset;
			uint initial_segment;
		};
		struct AssemblyError
		{
			AssemblyErrorCode code;
			string object;
			uint position;
		};
		struct AssemblyOutput
		{
			DataBlock data = DataBlock(0x10000);
			Array<uint> relocate_segments = Array<uint>(0x1000);
			uint entry_point_offset;
			uint entry_point_segment_relocate;
			uint required_memory;
			uint required_stack;
			uint desired_memory;
		};
		void Assembly(const string & input, const AssemblyState & initstate, AssemblyOutput & output, AssemblyError & error);
	}
}