#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XA
	{
		namespace Encoder
		{
			void EncodeALInt(Streaming::Stream * stream, uint64 value);
			void EncodeALInt(Streaming::Stream * stream, int64 value);
			void EncodeALInt(Streaming::Stream * stream, int32 value);
			void EncodeALInt(Streaming::Stream * stream, uint32 value);
			uint64 DecodeALInt(Streaming::Stream * stream);
			void EncodeString(Streaming::Stream * stream, const string & str);
			string DecodeString(Streaming::Stream * stream);
			void EncodeData(Streaming::Stream * stream, const DataBlock & data);
			void DecodeData(Streaming::Stream * stream, DataBlock & data);
			void EncodeLongData(Streaming::Stream * stream, const Array<uint32> & data);
			void DecodeLongData(Streaming::Stream * stream, Array<uint32> & data);
		}
	}
}