#pragma once

#include "../ertxpc/runtime/EngineRuntime.h"

namespace Engine
{
	namespace XI
	{
		class Module : public Object
		{
		public:
			class Version
			{
			public:
				uint major, minor, subver, build;
			};
		public:
			string module_import_name, assembler_name;
			Version assembler_version;
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