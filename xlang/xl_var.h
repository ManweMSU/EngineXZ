﻿#pragma once

#include "xl_types.h"

namespace Engine
{
	namespace XL
	{
		class IComputableProvider
		{
		public:
			virtual Object * ComputableProviderQueryObject(void) = 0;
			virtual XType * ComputableGetType(void) = 0;
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) = 0;
		};
		class XComputable : public XObject
		{
		public:
			virtual LObject * GetMember(const string & name) override;
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override;
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override;
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override;
			virtual void AddMember(const string & name, LObject * child) override;
			virtual bool GetWarpMode(void) = 0;
			virtual LObject * UnwarpedGetType(void) = 0;
			virtual XA::ExpressionTree UnwarpedEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) = 0;

		};
		class XLiteral : public XComputable
		{
		public:
			virtual void Attach(const string & name, const string & path, bool as_local) = 0;
			virtual int QueryValueAsInteger(void) = 0;
			virtual string QueryValueAsString(void) = 0;
			virtual XLiteral * Clone(void) = 0;
			virtual XI::Module::Literal & Expose(void) = 0;
		};

		XLiteral * CreateLiteral(LContext & ctx, bool value);
		XLiteral * CreateLiteral(LContext & ctx, uint64 value);
		XLiteral * CreateLiteral(LContext & ctx, uint64 value, int quant, bool sgn);
		XLiteral * CreateLiteral(LContext & ctx, double value);
		XLiteral * CreateLiteral(LContext & ctx, double value, int quant);
		XLiteral * CreateLiteral(LContext & ctx, const string & value);
		XLiteral * CreateLiteral(LContext & ctx, const XI::Module::Literal & data);
		XComputable * CreateNullLiteral(LContext & ctx);
		XComputable * CreateComputable(LContext & ctx, XType * of_type, const XA::ExpressionTree & with_tree);
		XComputable * CreateComputable(LContext & ctx, IComputableProvider * provider);
		XComputable * CreateVariable(LContext & ctx, const string & name, const string & path, bool local, XType * type, XA::ObjectSize offset);
		LObject * UnwarpObject(LObject * object);
	}
}