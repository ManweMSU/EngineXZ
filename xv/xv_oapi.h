#pragma once

#include "../xlang/xl_lal.h"

namespace Engine
{
	namespace XV
	{
		XL::LObject * CreateOperatorNew(XL::LContext & ctx);
		XL::LObject * CreateOperatorConstruct(XL::LContext & ctx);

		XL::LObject * CreateNew(XL::LContext & ctx, XL::LObject * type, int argc, XL::LObject ** argv);
		XL::LObject * CreateConstruct(XL::LContext & ctx, XL::LObject * at, int argc, XL::LObject ** argv);
		XL::LObject * CreateDelete(XL::LObject * object);
		XL::LObject * CreateFree(XL::LObject * object);
		XL::LObject * CreateDestruct(XL::LObject * object);
		XL::LObject * CreateDynamicCast(XL::LObject * object, XL::LObject * type_into);

		bool IsValidEnumerationBase(XL::LObject * type);
		bool CreateEnumerationValue(Volumes::ObjectDictionary<string, XL::LObject> & enum_db, XL::LObject * enum_type, const string & name, XL::LObject * value);
		void CreateEnumerationRoutines(Volumes::ObjectDictionary<string, XL::LObject> & enum_db, XL::LObject * enum_type);
		void CreateTypeServiceRoutines(XL::LObject * cls);
	}
}