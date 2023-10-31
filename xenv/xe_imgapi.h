#pragma once

#include "xe_loader.h"

namespace Engine
{
	namespace XE
	{
		typedef bool (* SynchronizeRoutine) (Object * owner);

		void ActivateImageIO(StandardLoader & ldr);
		Codec::Frame * ExtractFrameFromXFrame(handle xframe);
		Codec::Image * ExtractImageFromXImage(handle ximage);
		Object * CreateXFrame(Codec::Frame * frame);
		Object * CreateDirectContext(void * data, int width, int height, int stride, Object * owner, SynchronizeRoutine sync);
	}
}