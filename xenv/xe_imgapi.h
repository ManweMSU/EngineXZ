#pragma once

#include "xe_loader.h"

namespace Engine
{
	namespace XE
	{
		typedef bool (* SynchronizeRoutine) (Object * owner);

		enum VisualObjectInterface : uint {
			VisualObjectInterfaceBuffer		= 0x10001,
			VisualObjectInterfaceTexture	= 0x10002,
			VisualObjectInterfaceScreen		= 0x20001,
			VisualObjectInterfaceWindow		= 0x20002,
		};
		class VisualObject : public DynamicObject
		{
		public:
			virtual void ExposeInterface(uint intid, void * data, ErrorContext & ectx) noexcept = 0;
		};

		void ActivateImageIO(StandardLoader & ldr);
		Codec::Frame * ExtractFrameFromXFrame(handle xframe);
		Codec::Image * ExtractImageFromXImage(handle ximage);
		Object * CreateXFrame(Codec::Frame * frame);
		Object * CreateDirectContext(void * data, int width, int height, int stride, Object * owner, SynchronizeRoutine sync);
	}
}