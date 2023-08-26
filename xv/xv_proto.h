#pragma once

#include "xv_lexical.h"
#include "../xlang/xl_lal.h"

namespace Engine
{
	namespace XV
	{
		class ICompilationContext
		{
		public:
			virtual XL::LContext * GetLanguageContext(void) = 0;
			virtual XL::LObject * ProcessLanguageExpression(ITokenStream * input, Token & current_token, XL::LObject ** vns, int num_vns) = 0;
			virtual XL::LObject * ProcessLanguageExpressionThrow(ITokenStream * input, Token & current_token, XL::LObject ** vns, int num_vns) = 0;
			virtual bool ProcessLanguageDefinitions(ITokenStream * input, XL::LObject * dest_ns, XL::LObject ** vns, int num_vns) = 0;
		};

		void EnablePrototypes(ICompilationContext * on_context);
		void SupplyPrototypeImplementation(XL::LObject * proto, ITokenStream * impl);
		void SetPrototypeVisibility(XL::LObject * proto, XL::LObject ** vns, int num_vns);
		XL::LObject * CreateClassPrototype(ICompilationContext * context, XL::LObject * at, const string & name, int argc, const string * argv);
		XL::LObject * CreateFunctionPrototype(ICompilationContext * context, XL::LObject * at, const string & name, int argc, const string * argv);
		XL::LObject * CreateBlockPrototype(ICompilationContext * context, XL::LObject * at, const string & name, int argc, const string * argv);

		bool IsBlockAwaitPrototype(XL::LObject * proto);
		XL::LObject * InvokeBlockPrototype(XL::LObject * proto, ITokenStream * block, XL::LObject ** ssl, int ssc);
	}
}