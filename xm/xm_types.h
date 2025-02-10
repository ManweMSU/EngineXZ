#pragma once

#include "../ximg/xi_module.h"
#include "../xasm/xa_types.h"

namespace Engine
{
	namespace XM
	{
		struct DefinitionLocation
		{
			int absolute, length;
			Point coord;
		};
		struct StatementLocation
		{
			DefinitionLocation location;
			string function_symbol;
			int instruction_index;
		};
		struct VariableLocation
		{
			string name, tcn;
			DefinitionLocation location;
			bool global;
			string function_symbol;
			int instruction_index;
			XA::ObjectReference holder;
		};
		struct DebugData
		{
			string source_full_path;
			Array<StatementLocation> statements;
			Array<VariableLocation> variables;

			DebugData(void);
			void Clear(void);
			void Save(Streaming::Stream * dest) const;
			void Load(Streaming::Stream * src);
		};

		void AddModuleDebugData(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const DebugData & data);
		bool LoadModuleDebugData(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, DebugData & data);
	}
}