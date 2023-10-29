#pragma once

#include "xe_loader.h"

namespace Engine
{
	namespace XE
	{
		void ActivateImageIO(StandardLoader & ldr);
		Codec::Frame * ExtractFrameFromXFrame(handle xframe);
		Codec::Image * ExtractImageFromXImage(handle ximage);
	}
}