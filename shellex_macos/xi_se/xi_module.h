#pragma once

#include "../ertxpc/runtime/EngineRuntime.h"

namespace Engine
{
	namespace XI
	{
		class Module : public Object
		{
		public:
			Volumes::ObjectDictionary<uint64, DataBlock> resources;	// TYPE:ID
		public:
			Module(void);
			Module(Streaming::Stream * source);
			virtual ~Module(void) override;
		};
		uint64 MakeResourceID(const string & type, int id);
		void ReadResourceID(uint64 rid, string & type, int & id);
	}
}