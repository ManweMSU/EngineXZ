#pragma once

#include "xl_types.h"
#include "../xw/xw_types.h"

namespace Engine
{
	namespace XL
	{
		struct FunctionImplementationDesc
		{
			bool _pure, _is_xw;
			XA::Function _xa;
			Volumes::Dictionary<XW::ShaderLanguage, XW::TranslationRules> _xw;
			string _import_func, _import_library;
		};

		class XMethodOverload;
		class XFunctionOverload;
		class XMethod;
		class XFunction;

		class XMethodOverload : public XObject
		{
		public:
			virtual VirtualFunctionDesc & GetVFDesc(void) = 0;
			virtual XClass * GetInstanceType(void) = 0;
			virtual LContext & GetContext(void) = 0;
			virtual string GetCanonicalType(void) = 0;
			virtual LObject * InvokeNoVirtual(int argc, LObject ** argv) = 0;
		};
		class XFunctionOverload : public XObject
		{
		public:
			virtual XMethodOverload * SetInstance(LObject * instance) = 0;
			virtual VirtualFunctionDesc & GetVFDesc(void) = 0;
			virtual FunctionImplementationDesc & GetImplementationDesc(void) = 0;
			virtual bool NeedsInstance(void) = 0;
			virtual bool Throws(void) = 0;
			virtual uint & GetFlags(void) = 0;
			virtual XClass * GetInstanceType(void) = 0;
			virtual LContext & GetContext(void) = 0;
			virtual string GetCanonicalType(void) = 0;
			virtual bool CheckForInline(void) = 0;
		};
		class XMethod : public XObject
		{
		public:
			virtual XMethodOverload * GetOverloadT(const string & ocn) = 0;
			virtual XMethodOverload * GetOverloadT(int argc, const string * argv) = 0;
			virtual XMethodOverload * GetOverloadV(int argc, LObject ** argv) = 0;
			virtual XClass * GetInstanceType(void) = 0;
			virtual LContext & GetContext(void) = 0;
		};
		class XFunction : public XObject
		{
		public:
			virtual XFunctionOverload * GetOverloadT(const string & ocn, bool allow_instance = false) = 0;
			virtual XFunctionOverload * GetOverloadT(int argc, const string * argv, bool allow_instance = false) = 0;
			virtual XFunctionOverload * GetOverloadV(int argc, LObject ** argv, bool allow_instance = false) = 0;
			virtual XFunctionOverload * AddOverload(XType * retval, int argc, XType ** argv, uint flags, bool local) = 0;
			virtual XFunctionOverload * AddOverload(XType * retval, int argc, XType ** argv, uint flags, bool local, Point vfi) = 0;
			virtual void ListOverloads(Array<string> & fcn, bool allow_instance = false) = 0;
			virtual XClass * GetInstanceType(void) = 0;
			virtual XMethod * SetInstance(LObject * instance) = 0;
			virtual LContext & GetContext(void) = 0;
		};

		XFunction * CreateFunction(LContext & ctx, const string & name, const string & path, XClass * instance_type = 0);
	}
}