#include "xm_types.h"

namespace Engine
{
	namespace XM
	{
		namespace Encoder
		{
			void Encode(Storage::RegistryNode * node, const string & name, const string & value)
			{
				node->CreateValue(name, Storage::RegistryValueType::String);
				node->SetValue(name, value);
			}
			void Encode(Storage::RegistryNode * node, const string & name, int value)
			{
				node->CreateValue(name, Storage::RegistryValueType::Integer);
				node->SetValue(name, value);
			}
			void Encode(Storage::RegistryNode * node, const string & name, uint value)
			{
				node->CreateValue(name, Storage::RegistryValueType::Integer);
				node->SetValue(name, int(value));
			}
			void Encode(Storage::RegistryNode * node, const string & name, bool value)
			{
				node->CreateValue(name, Storage::RegistryValueType::Boolean);
				node->SetValue(name, value);
			}
			void Encode(Storage::RegistryNode * node, const DefinitionLocation & value)
			{
				Encode(node, L"A", value.absolute);
				Encode(node, L"L", value.length);
				Encode(node, L"X", value.coord.x);
				Encode(node, L"Y", value.coord.y);
			}
			void Encode(Storage::RegistryNode * node, const string & name, const StatementLocation & value)
			{
				node->CreateNode(name);
				SafePointer<Storage::RegistryNode> sub = node->OpenNode(name);
				Encode(sub, value.location);
				Encode(sub, L"Symbolum", value.function_symbol);
				Encode(sub, L"Index", value.instruction_index);
			}
			void Encode(Storage::RegistryNode * node, const string & name, const VariableLocation & value)
			{
				node->CreateNode(name);
				SafePointer<Storage::RegistryNode> sub = node->OpenNode(name);
				Encode(sub, value.location);
				Encode(sub, L"Nomen", value.name);
				Encode(sub, L"Genus", value.tcn);
				Encode(sub, L"Globalis", value.global);
				if (!value.global) {
					Encode(sub, L"Symbolum", value.function_symbol);
					Encode(sub, L"Index", value.instruction_index);
					Encode(sub, L"Alligatura", uint(value.holder.ref_class));
					Encode(sub, L"IndexAlligaturae", value.holder.index);
				}
			}
			void Decode(Storage::RegistryNode * node, DefinitionLocation & value)
			{
				value.absolute = node->GetValueInteger(L"A");
				value.length = node->GetValueInteger(L"L");
				value.coord.x = node->GetValueInteger(L"X");
				value.coord.y = node->GetValueInteger(L"Y");
			}
			void Decode(Storage::RegistryNode * node, const string & name, StatementLocation & value)
			{
				SafePointer<Storage::RegistryNode> sub = node->OpenNode(name);
				if (!sub) throw InvalidArgumentException();
				Decode(sub, value.location);
				value.function_symbol = sub->GetValueString(L"Symbolum");
				value.instruction_index = sub->GetValueInteger(L"Index");
			}
			void Decode(Storage::RegistryNode * node, const string & name, VariableLocation & value)
			{
				SafePointer<Storage::RegistryNode> sub = node->OpenNode(name);
				if (!sub) throw InvalidArgumentException();
				Decode(sub, value.location);
				value.name = sub->GetValueString(L"Nomen");
				value.tcn = sub->GetValueString(L"Genus");
				value.global = sub->GetValueBoolean(L"Globalis");
				value.holder.qword = 0;
				if (value.global) {
					value.function_symbol = L"";
					value.instruction_index = 0;
				} else {
					value.function_symbol = sub->GetValueString(L"Symbolum");
					value.instruction_index = sub->GetValueInteger(L"Index");
					value.holder.ref_class = sub->GetValueInteger(L"Alligatura");
					value.holder.index = sub->GetValueInteger(L"IndexAlligaturae");
				}
			}
		}
		
		DebugData::DebugData(void) : statements(0x400), variables(0x400) {}
		void DebugData::Clear(void) { source_full_path = L""; statements.Clear(); variables.Clear(); }
		void DebugData::Save(Streaming::Stream * dest) const
		{
			SafePointer<Storage::Registry> reg = Storage::CreateRegistry();
			Encoder::Encode(reg, L"SemitaInitialis", source_full_path);
			reg->CreateNode(L"Praedicationes");
			SafePointer<Storage::RegistryNode> node = reg->OpenNode(L"Praedicationes");
			for (int i = 0; i < statements.Length(); i++) Encoder::Encode(node, string(uint(i), DecimalBase, 6), statements[i]);
			reg->CreateNode(L"Variabiles");
			node = reg->OpenNode(L"Variabiles");
			for (int i = 0; i < variables.Length(); i++) Encoder::Encode(node, string(uint(i), DecimalBase, 6), variables[i]);
			reg->Save(dest);
		}
		void DebugData::Load(Streaming::Stream * src)
		{
			Clear();
			SafePointer<Storage::Registry> reg = Storage::LoadRegistry(src);
			if (!reg) throw InvalidFormatException();
			source_full_path = reg->GetValueString(L"SemitaInitialis");
			SafePointer<Storage::RegistryNode> node = reg->OpenNode(L"Praedicationes");
			if (node) for (auto & n : node->GetSubnodes()) {
				StatementLocation s;
				Encoder::Decode(node, n, s);
				statements.Append(s);
			}
			node = reg->OpenNode(L"Variabiles");
			if (node) for (auto & n : node->GetSubnodes()) {
				VariableLocation v;
				Encoder::Decode(node, n, v);
				variables.Append(v);
			}
		}
		void AddModuleDebugData(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const DebugData & data)
		{
			Streaming::MemoryStream stream(0x10000);
			data.Save(&stream);
			stream.Seek(0, Streaming::Begin);
			SafePointer<DataBlock> sect = stream.ReadAll();
			rsrc.Append(XI::MakeResourceID(L"XM", 1), sect);
		}
		bool LoadModuleDebugData(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, DebugData & data)
		{
			auto sect = rsrc.GetObjectByKey(XI::MakeResourceID(L"XM", 1));
			if (sect) {
				Streaming::MemoryStream stream(sect->GetBuffer(), sect->Length());
				data.Load(&stream);
				return true;
			} else return false;
		}
	}
}