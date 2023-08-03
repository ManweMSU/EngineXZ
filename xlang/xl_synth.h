#pragma once

#include "xl_types.h"

namespace Engine
{
	namespace XL
	{
		LObject * CreateStaticArrayRoutine(XArray * on_array, uint flags);
		void CreateDefaultImplementation(XClass * on_class, uint flags, ObjectArray<LObject> & vft_init);
		void CreateDefaultImplementations(XClass * on_class, uint flags, ObjectArray<LObject> & vft_init);
	}
}