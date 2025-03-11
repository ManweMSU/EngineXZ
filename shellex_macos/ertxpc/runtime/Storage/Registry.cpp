#include "Registry.h"

namespace Engine
{
	namespace Storage
	{
		ENGINE_PACKED_STRUCTURE(EngineRegistryHeader)
			char Signature[8];  // "ecs.1.0"
			uint32 SignatureEx; // 0x80000004
			uint32 Version;     // 0
			uint32 DataOffset;
			uint32 DataSize;
			uint32 RootOffset;
		ENGINE_END_PACKED_STRUCTURE

		class RegistryValue
		{
		public:
			RegistryValueType type;
			union
			{
#ifdef ENGINE_X64
				struct {
					int32 value_int32;
					uint32 unused0[3];
				};
				struct {
					float value_float;
					uint32 unused1[3];
				};
				struct {
					uint8 * value_binary;
					uint64 value_binary_size;
				};
				struct {
					int64 value_int64;
					uint64 unused3;
				};
				struct {
					double value_double;
					uint64 unused4;
				};
				uint32 value[4];
#else
				struct {
					int32 value_int32;
					uint32 unused0;
				};
				struct {
					float value_float;
					uint32 unused1;
				};
				struct {
					uint8 * value_binary;
					uint32 value_binary_size;
				};
				struct {
					int64 value_int64;
				};
				struct {
					double value_double;
				};
				uint32 value[2];
#endif
			};

			RegistryValue(void) : RegistryValue(RegistryValueType::Integer) {}
			RegistryValue(RegistryValueType value_type)
			{
				type = value_type;
				ZeroMemory(value, sizeof(value));
			}
			RegistryValue(const RegistryValue & src)
			{
				type = src.type;
				if (type == RegistryValueType::String) {
					value_binary_size = src.value_binary_size;
					value_binary = reinterpret_cast<uint8 *>(malloc(value_binary_size));
					if (!value_binary && value_binary_size) throw OutOfMemoryException();
					MemoryCopy(value_binary, src.value_binary, value_binary_size);
				} else {
					MemoryCopy(value, src.value, sizeof(value));
				}
			}
			RegistryValue(RegistryValue && src)
			{
				type = src.type;
				MemoryCopy(value, src.value, sizeof(value));
				ZeroMemory(src.value, sizeof(value));
			}
			~RegistryValue(void) { if (type == RegistryValueType::String) free(value_binary); }
			RegistryValue & operator = (const RegistryValue & src)
			{
				if (this == &src) return *this;
				if (type == RegistryValueType::String) free(value_binary);
				type = src.type;
				if (type == RegistryValueType::String) {
					value_binary_size = src.value_binary_size;
					value_binary = reinterpret_cast<uint8 *>(malloc(value_binary_size));
					if (!value_binary && value_binary_size) throw OutOfMemoryException();
					MemoryCopy(value_binary, src.value_binary, value_binary_size);
				} else {
					MemoryCopy(value, src.value, sizeof(value));
				}
				return *this;
			}
			RegistryValue & operator = (RegistryValue && src)
			{
				if (type == RegistryValueType::String) free(value_binary);
				type = src.type;
				MemoryCopy(value, src.value, sizeof(value));
				ZeroMemory(src.value, sizeof(value));
			}
			void SetBinarySize(int size)
			{
				if (type == RegistryValueType::String) {
					uint8 * reset = reinterpret_cast<uint8 *>(realloc(value_binary, size));
					if (!reset && size) throw OutOfMemoryException();
					value_binary = reset;
					value_binary_size = size;
				}
			}
			void Set(int32 new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = new_value;
				else if (type == RegistryValueType::Float) value_float = float(new_value);
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
			}
			void Set(float new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = int32(new_value);
				else if (type == RegistryValueType::Float) value_float = new_value;
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
			}
			void Set(bool new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::Float) value_float = new_value ? 1.0f : 0.0f;
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
			}
			void Set(const string & new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = new_value.ToInt32();
				else if (type == RegistryValueType::Float) value_float = new_value.ToFloat();
				else if (type == RegistryValueType::Boolean) value_int32 = new_value.ToBoolean();
				else if (type == RegistryValueType::String) {
					if (new_value.Length()) {
						SetBinarySize((new_value.GetEncodedLength(Encoding::UTF16) + 1) * 2);
						new_value.Encode(value_binary, Encoding::UTF16, true);
					} else SetBinarySize(0);
				}
			}
			int32 GetInteger(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean) return value_int32;
				else if (type == RegistryValueType::Float) return int32(value_float);
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToInt32() : 0;
				else return 0;
			}
			float GetFloat(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean) return float(value_int32);
				else if (type == RegistryValueType::Float) return value_float;
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToFloat() : 0.0f;
				else return 0.0f;
			}
			bool GetBoolean(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean) return value_int32 ? true : false;
				else if (type == RegistryValueType::Float) return value_float ? true : false;
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToBoolean() : false;
				else return false;
			}
			string GetString(void) const
			{
				if (type == RegistryValueType::Integer) return string(value_int32);
				else if (type == RegistryValueType::Float) return string(value_float);
				else if (type == RegistryValueType::Boolean) return string(value_int32 ? true : false);
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16) : L"";
				else return L"";
			}
		};
		class RegistryNodeImplementation : public RegistryNode
		{
			Array<string> NodeNames;
			Array<string> ValueNames;
			ObjectArray<RegistryNodeImplementation> Nodes;
			Array<RegistryValue> Values;
		public:
			RegistryNodeImplementation(void) : NodeNames(0x10), ValueNames(0x10), Nodes(0x10), Values(0x10) {}
			virtual ~RegistryNodeImplementation(void) override {}

			RegistryNodeImplementation * FindNode(const string & name) const
			{
				for (int i = 0; i < NodeNames.Length(); i++) if (string::Compare(name, NodeNames[i]) == 0) {
					return Nodes.ElementAt(i);
				}
				return 0;
			}
			int FindNodeIndex(const string & name) const
			{
				for (int i = 0; i < NodeNames.Length(); i++) if (string::Compare(name, NodeNames[i]) == 0) {
					return i;
				}
				return -1;
			}
			RegistryValue * FindValue(const string & name)
			{
				for (int i = 0; i < ValueNames.Length(); i++) if (string::Compare(name, ValueNames[i]) == 0) {
					return &Values[i];
				}
				return 0;
			}
			const RegistryValue * FindValue(const string & name) const
			{
				for (int i = 0; i < ValueNames.Length(); i++) if (string::Compare(name, ValueNames[i]) == 0) {
					return &Values[i];
				}
				return 0;
			}
			int FindValueIndex(const string & name) const
			{
				for (int i = 0; i < ValueNames.Length(); i++) if (string::Compare(name, ValueNames[i]) == 0) {
					return i;
				}
				return -1;
			}
			RegistryNodeImplementation * ResolveNodeByPath(const string & path)
			{
				auto result = this;
				int pos = 0;
				while (pos < path.Length()) {
					int ep = pos;
					while (ep < path.Length() && path[ep] != L'\\' && path[ep] != L'/') ep++;
					string name = path.Fragment(pos, ep - pos);
					if (name.Length()) {
						result = result->FindNode(name);
						if (!result) return 0;
					}
					pos = ep + 1;
				}
				return result;
			}
			const RegistryNodeImplementation * ResolveNodeByPath(const string & path) const
			{
				auto result = this;
				int pos = 0;
				while (pos < path.Length()) {
					int ep = pos;
					while (ep < path.Length() && path[ep] != L'\\' && path[ep] != L'/') ep++;
					string name = path.Fragment(pos, ep - pos);
					if (name.Length()) {
						result = result->FindNode(name);
						if (!result) return 0;
					}
					pos = ep + 1;
				}
				return result;
			}
			RegistryValue * ResolveValueByPath(const string & path)
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					return at->FindValue(IO::Path::GetFileName(path));
				} else return 0;
			}
			const RegistryValue * ResolveValueByPath(const string & path) const
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					return at->FindValue(IO::Path::GetFileName(path));
				} else return 0;
			}
			int CreateRawNode(const string & name, RegistryNodeImplementation * attach = 0)
			{
				if (!name.Length() || name.FindFirst(L'\\') != -1 || name.FindFirst(L'/') != -1) return -1;
				int at = NodeNames.Length();
				for (int i = 0; i < NodeNames.Length(); i++) {
					int comp = string::Compare(name, NodeNames[i]);
					if (!comp) return i;
					if (comp < 0) {
						at = i;
						break;
					}
				}
				if (attach) {
					Nodes.Insert(attach, at);
				} else {
					RegistryNodeImplementation * node = new RegistryNodeImplementation;
					Nodes.Insert(node, at);
					node->Release();
				}
				NodeNames.Insert(name, at);
				return at;
			}
			int CreateRawValue(const string & name, RegistryValueType type)
			{
				if (!name.Length() || name.FindFirst(L'\\') != -1 || name.FindFirst(L'/') != -1) return -1;
				if (static_cast<int>(type) < 1 || static_cast<int>(type) > 9) return -1;
				int at = ValueNames.Length();
				for (int i = 0; i < ValueNames.Length(); i++) {
					int comp = string::Compare(name, ValueNames[i]);
					if (!comp) return -1;
					if (comp < 0) {
						at = i;
						break;
					}
				}
				Values.Insert(RegistryValue(type), at);
				ValueNames.Insert(name, at);
				return at;
			}

			virtual const Array<string>& GetSubnodes(void) const override { return NodeNames; }
			virtual const Array<string>& GetValues(void) const override { return ValueNames; }
			virtual RegistryNode * OpenNode(const string & path) override
			{
				auto node = ResolveNodeByPath(path);
				if (node) {
					node->Retain();
					return node;
				} else return 0;
			}
			virtual RegistryValueType GetValueType(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) {
					return storage->type;
				} else return RegistryValueType::Unknown;
			}
			virtual int32 GetValueInteger(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetInteger() : 0;
			}
			virtual float GetValueFloat(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetFloat() : 0.0f;
			}
			virtual bool GetValueBoolean(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetBoolean() : false;
			}
			virtual string GetValueString(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetString() : L"";
			}
			
			void FillFromMemoryData(const uint8 * data, int size, int data_at)
			{
				int32 NodeCount, ValueCount;
				NodeCount = *reinterpret_cast<const int32 *>(data + data_at);
				ValueCount = *reinterpret_cast<const int32 *>(data + data_at + 4);
				for (int i = 0; i < NodeCount; i++) {
					int32 NodeOffset = *reinterpret_cast<const int32 *>(data + data_at + 8 + 4 * i);
					int32 NodeNameOffset = *reinterpret_cast<const int32 *>(data + data_at + 8 + 4 * NodeCount + 4 * i);
					auto subnode = new RegistryNodeImplementation;
					subnode->FillFromMemoryData(data, size, NodeOffset);
					CreateRawNode(string(data + NodeNameOffset, -1, Encoding::UTF16), subnode);
					subnode->Release();
				}
				for (int i = 0; i < ValueCount; i++) {
					int32 ValueOffset = *reinterpret_cast<const int32 *>(data + data_at + 8 + 8 * NodeCount + 4 * i);
					int32 ValueNameOffset = *reinterpret_cast<const int32 *>(data + ValueOffset);
					int32 ValueTypeCode = *reinterpret_cast<const int32 *>(data + ValueOffset + 4);
					int index = CreateRawValue(string(data + ValueNameOffset, -1, Encoding::UTF16), static_cast<RegistryValueType>(ValueTypeCode));
					auto & value = Values[index];
					if (value.type == RegistryValueType::Integer || value.type == RegistryValueType::Float || value.type == RegistryValueType::Boolean) {
						int32 short_value = *reinterpret_cast<const int32 *>(data + ValueOffset + 8);
						value.value_int32 = short_value;
					} else if (value.type == RegistryValueType::String) {
						int32 offset = *reinterpret_cast<const int32 *>(data + ValueOffset + 8);
						value.Set(string(data + offset, -1, Encoding::UTF16));
					}
				}
			}
		};
		class RegistryImplementation : public Registry
		{
			SafePointer<RegistryNodeImplementation> Root;
		public:
			RegistryImplementation(void) { Root.SetReference(new RegistryNodeImplementation); }
			RegistryImplementation(RegistryNodeImplementation * node) { Root.SetReference(node); }
			virtual ~RegistryImplementation(void) override {}

			virtual const Array<string>& GetSubnodes(void) const override { return Root->GetSubnodes(); }
			virtual const Array<string>& GetValues(void) const override { return Root->GetValues(); }
			virtual RegistryNode * OpenNode(const string & path) override { return Root->OpenNode(path); }
			virtual RegistryValueType GetValueType(const string & path) const override { return Root->GetValueType(path); }
			virtual int32 GetValueInteger(const string & path) const override { return Root->GetValueInteger(path); }
			virtual float GetValueFloat(const string & path) const override { return Root->GetValueFloat(path); }
			virtual bool GetValueBoolean(const string & path) const override { return Root->GetValueBoolean(path); }
			virtual string GetValueString(const string & path) const override { return Root->GetValueString(path); }
		};

		Registry * CreateRegistry(void) { return new RegistryImplementation; }
		Registry * LoadRegistry(Streaming::Stream * source)
		{
			try {
				EngineRegistryHeader hdr;
				source->Read(&hdr, sizeof(hdr));
				if (MemoryCompare(hdr.Signature, "ecs.1.0", 8) != 0 || hdr.SignatureEx != 0x80000004 || hdr.Version != 0)
					throw InvalidFormatException();
				Array<uint8> data(hdr.DataSize);
				data.SetLength(hdr.DataSize);
				source->Seek(hdr.DataOffset, Streaming::Begin);
				source->Read(data.GetBuffer(), hdr.DataSize);
				RegistryNodeImplementation * root = new RegistryNodeImplementation;
				root->FillFromMemoryData(data.GetBuffer(), data.Length(), hdr.RootOffset);
				return new RegistryImplementation(root);
			} catch (...) { return 0; }
		}
	}
}