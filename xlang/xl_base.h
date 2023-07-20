#pragma once

#include "xl_com.h"

namespace Engine
{
	namespace XL
	{
		class XAlias : public XObject
		{
		public:
			virtual bool IsTypeAlias(void) const = 0;
			virtual string GetDestination(void) const = 0;
		};

		XObject * CreateNull(void);
		XObject * CreateNamespace(const string & name, const string & path, LContext & ctx);
		XObject * CreateScope(void);
		XAlias * CreateAlias(const string & name, const string & path, const string & to, bool cn_alias, bool local);
	}
}