#pragma once

#include "../EngineBase.h"
#include "../ImageCodec/CodecBase.h"
#include <CoreText/CoreText.h>

namespace Engine
{
	namespace Cocoa
	{
		string EngineString(CFStringRef str);
		CGImageRef CocoaCoreImage(Codec::Frame * frame);
	}
}