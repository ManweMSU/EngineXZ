#pragma once

#include "../xasm/xa_types.h"

namespace Engine
{
	namespace XCL
	{
		enum class Class { Namespace, Scope, Alias, Transform, Type, Function, Variable, Literal };

		class LObject : public Object
		{
		public:
			// Object information
			virtual Class GetClass(void) = 0;
			virtual string GetName(void) = 0;
			virtual string GetFullName(void) = 0;
			// Working with
			virtual LObject * GetType(void) = 0;
			virtual LObject * GetMember(const string & name) = 0;
			virtual LObject * Invoke(int argc, const LObject * argv) = 0;
			virtual XA::ExpressionTree Evaluate(void) = 0;
		};
	}
}