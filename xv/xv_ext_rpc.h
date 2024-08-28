#pragma once

#include "xv_oapi.h"

namespace Engine
{
	namespace XV
	{
		void CreateRPCServiceRoutines(XL::LObject * cls);
		void CreateRPCServiceObjects(XL::LObject * cls, ObjectArray<XL::LObject> & init_list);
	}
}