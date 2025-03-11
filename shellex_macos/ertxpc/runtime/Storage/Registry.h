#pragma once

#include "../EngineBase.h"
#include "../Streaming.h"

namespace Engine
{
	namespace Storage
	{
		enum class RegistryValueType { Unknown = 0, Integer = 1, Float = 2, Boolean = 3, String = 4 };
		class RegistryNode : public Object
		{
		public:
			virtual const Array<string> & GetSubnodes(void) const = 0;
			virtual const Array<string> & GetValues(void) const = 0;

			virtual RegistryNode * OpenNode(const string & path) = 0;
			virtual RegistryValueType GetValueType(const string & path) const = 0;

			virtual int32 GetValueInteger(const string & path) const = 0;
			virtual float GetValueFloat(const string & path) const = 0;
			virtual bool GetValueBoolean(const string & path) const = 0;
			virtual string GetValueString(const string & path) const = 0;
		};
		class Registry : public RegistryNode {};

		Registry * CreateRegistry(void);
		Registry * LoadRegistry(Streaming::Stream * source);
	}
}