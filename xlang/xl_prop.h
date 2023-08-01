#pragma once

#include "xl_func.h"
#include "xl_var.h"

namespace Engine
{
	namespace XL
	{
		class XField : public XObject
		{
		public:
			virtual XA::ObjectSize & GetOffset(void) = 0;
			virtual XClass * GetInstanceType(void) = 0;
			virtual XComputable * SetInstance(LObject * instance) = 0;
			virtual LContext & GetContext(void) = 0;
		};
		class XInstancedProperty : public XComputable
		{
		public:
			virtual XMethodOverload * GetSetter(void) = 0;
			virtual XMethodOverload * GetGetter(void) = 0;
			virtual XClass * GetInstanceType(void) = 0;
			virtual LContext & GetContext(void) = 0;
		};
		class XProperty : public XObject
		{
		public:
			virtual XFunctionOverload * GetSetter(void) = 0;
			virtual XFunctionOverload * GetGetter(void) = 0;
			virtual XFunctionOverload * AddSetter(uint flags) = 0;
			virtual XFunctionOverload * AddSetter(uint flags, Point vfi) = 0;
			virtual XFunctionOverload * AddGetter(uint flags) = 0;
			virtual XFunctionOverload * AddGetter(uint flags, Point vfi) = 0;
			virtual XClass * GetInstanceType(void) = 0;
			virtual XInstancedProperty * SetInstance(LObject * instance) = 0;
			virtual LContext & GetContext(void) = 0;
		};

		XField * CreateField(XClass * on, XType * of_type, const string & name, XA::ObjectSize offset);
		XProperty * CreateProperty(XClass * on, XType * of_type, const string & name);
	}
}