#include "xe_rtss.h"

namespace Engine
{
	namespace XE
	{
		SymbolSystem::SymbolSystem(void) {}
		SymbolSystem::~SymbolSystem(void) {}
		bool SymbolSystem::RegisterSymbol(SymbolObject * symbol, const string & name) noexcept { try { return _symbols.Append(name, symbol); } catch (...) { return false; } }
		const SymbolObject * SymbolSystem::FindSymbol(const string & name, bool follow_aliases, int max_jumps) const noexcept
		{
			const string * name_find = &name;
			int jump = 0;
			while (jump <= max_jumps) {
				auto smbl = _symbols.GetObjectByKey(*name_find);
				if (!smbl) return 0;
				if (smbl->GetSymbolType() == SymbolType::Alias && follow_aliases) {
					name_find = &static_cast<AliasSymbol *>(smbl)->GetDestination();
					jump++;
				} else return smbl;
			}
			return 0;
		}
		Volumes::ObjectDictionary<string, SymbolObject> & SymbolSystem::GetSymbolTable(void) noexcept { return _symbols; }
		const Volumes::ObjectDictionary<string, SymbolObject> & SymbolSystem::GetSymbolTable(void) const noexcept { return _symbols; }

		LiteralSymbol::LiteralSymbol(const void * pdata, Reflection::PropertyType data_type, const Volumes::Dictionary<string, string> & attrs) : _type(data_type), _attributes(attrs)
		{
			_data_64 = 0;
			if (data_type == Reflection::PropertyType::String) {
				_data_large = reinterpret_cast<const string *>(pdata)->EncodeSequence(Encoding::UTF32, true);
			} else if (data_type == Reflection::PropertyType::Double || data_type == Reflection::PropertyType::UInt64 || data_type == Reflection::PropertyType::Int64) {
				MemoryCopy(&_data_64, pdata, 8);
			} else if (data_type == Reflection::PropertyType::Float || data_type == Reflection::PropertyType::UInt32 || data_type == Reflection::PropertyType::Int32) {
				MemoryCopy(&_data_64, pdata, 4);
			} else if (data_type == Reflection::PropertyType::UInt16 || data_type == Reflection::PropertyType::Int16) {
				MemoryCopy(&_data_64, pdata, 2);
			} else if (data_type == Reflection::PropertyType::Boolean || data_type == Reflection::PropertyType::UInt8 || data_type == Reflection::PropertyType::Int8) {
				MemoryCopy(&_data_64, pdata, 1);
			} else throw InvalidArgumentException();
		}
		LiteralSymbol::~LiteralSymbol(void) {}
		SymbolType LiteralSymbol::GetSymbolType(void) const noexcept { return SymbolType::Literal; }
		void * LiteralSymbol::GetSymbolEntity(void) const noexcept { if (_data_large) return _data_large->GetBuffer(); else return const_cast<uint64 *>(&_data_64); }
		const Volumes::Dictionary<string, string> * LiteralSymbol::GetAttributes(void) const noexcept { return &_attributes; }
		Reflection::PropertyType LiteralSymbol::GetValueType(void) const noexcept { return _type; }

		ClassSymbol::ClassSymbol(const string & name, XA::ArgumentSemantics semantics, uint size, const Volumes::Dictionary<string, string> & attrs) :
			_class_name(name), _semantics(semantics), _instance_size(size), _interface_list(0x10), _field_list(0x10), _prop_list(0x10), _method_list(0x10), _attributes(attrs) {}
		ClassSymbol::~ClassSymbol(void) {}
		SymbolType ClassSymbol::GetSymbolType(void) const noexcept { return SymbolType::Class; }
		void * ClassSymbol::GetSymbolEntity(void) const noexcept { return const_cast<ClassSymbol *>(this); }
		const Volumes::Dictionary<string, string> * ClassSymbol::GetAttributes(void) const noexcept { return &_attributes; }
		void ClassSymbol::AddInterface(const string & name, int vft_offset, int cast_offset)
		{
			_interface_desc desc;
			desc.name = name;
			desc.vft_offset = vft_offset;
			desc.cast_to_offset = cast_offset;
			_interface_list << desc;
		}
		void ClassSymbol::AddField(const string & name, const string & type, uint offset, uint size, const Volumes::Dictionary<string, string> & attrs)
		{
			_field_desc desc;
			desc.name = name;
			desc.type_cn = type;
			desc.offset = offset;
			desc.size = size;
			desc.attributes = attrs;
			_field_list << desc;
		}
		void ClassSymbol::AddProperty(const string & name, const string & type, const string & setter, const string & getter, const Volumes::Dictionary<string, string> & attrs)
		{
			_prop_desc desc;
			desc.name = name;
			desc.type_cn = type;
			desc.setter = setter;
			desc.getter = getter;
			desc.attributes = attrs;
			_prop_list << desc;
		}
		void ClassSymbol::AddMethod(const string & name, const string & function, int vft_id, int vf_id, const Volumes::Dictionary<string, string> & attrs)
		{
			_method_desc desc;
			desc.name = name;
			desc.func_name = function;
			desc.vft_index = vft_id;
			desc.vf_index = vf_id;
			desc.attributes = attrs;
			_method_list << desc;
		}
		const string & ClassSymbol::GetClassName(void) const noexcept { return _class_name; }
		XA::ArgumentSemantics ClassSymbol::GetClassSemantics(void) const noexcept { return _semantics; }
		uint ClassSymbol::GetInstanceSize(void) const noexcept { return _instance_size; }
		Array<string> * ClassSymbol::ListInterfaces(void) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _interface_list) result->Append(desc.name);
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListFields(void) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _field_list) result->Append(desc.name);
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListFields(const string & with_attr) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _field_list) if (desc.attributes.ElementExists(with_attr)) result->Append(desc.name);
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListFields(const string & with_attr, const string & of_value) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _field_list) {
					auto value = desc.attributes.GetElementByKey(with_attr);
					if (value && *value == of_value) result->Append(desc.name);
				}
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListProperties(void) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _prop_list) result->Append(desc.name);
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListProperties(const string & with_attr) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _prop_list) if (desc.attributes.ElementExists(with_attr)) result->Append(desc.name);
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListProperties(const string & with_attr, const string & of_value) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _prop_list) {
					auto value = desc.attributes.GetElementByKey(with_attr);
					if (value && *value == of_value) result->Append(desc.name);
				}
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListMethods(void) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _method_list) result->Append(desc.name);
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListMethods(const string & with_attr) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _method_list) if (desc.attributes.ElementExists(with_attr)) result->Append(desc.name);
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Array<string> * ClassSymbol::ListMethods(const string & with_attr, const string & of_value) const noexcept
		{
			try {
				SafePointer< Array<string> > result = new Array<string>(0x10);
				for (auto & desc : _method_list) {
					auto value = desc.attributes.GetElementByKey(with_attr);
					if (value && *value == of_value) result->Append(desc.name);
				}
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		void * ClassSymbol::GetInterfaceVFT(const string & name, void * instance) const noexcept
		{
			for (auto & desc : _interface_list) if (desc.name == name) return reinterpret_cast<char *>(instance) + desc.vft_offset;
			return 0;
		}
		void * ClassSymbol::CastToInterface(const string & name, void * instance) const noexcept
		{
			for (auto & desc : _interface_list) if (desc.name == name) return reinterpret_cast<char *>(instance) + desc.cast_to_offset;
			return 0;
		}
		void * ClassSymbol::CastFromInterface(const string & name, void * instance) const noexcept
		{
			for (auto & desc : _interface_list) if (desc.name == name) return reinterpret_cast<char *>(instance) - desc.cast_to_offset;
			return 0;
		}
		const string * ClassSymbol::GetFieldType(const string & name) const noexcept
		{
			for (auto & desc : _field_list) if (desc.name == name) return &desc.type_cn;
			return 0;
		}
		void * ClassSymbol::GetFieldAddress(const string & name, void * instance) const noexcept
		{
			for (auto & desc : _field_list) if (desc.name == name) return reinterpret_cast<char *>(instance) + desc.offset;
			return 0;
		}
		uint ClassSymbol::GetFieldSize(const string & name) const noexcept
		{
			for (auto & desc : _field_list) if (desc.name == name) return desc.size;
			return 0;
		}
		const Volumes::Dictionary<string, string> * ClassSymbol::GetFieldAttributes(const string & name) const noexcept
		{
			for (auto & desc : _field_list) if (desc.name == name) return &desc.attributes;
			return 0;
		}
		const string * ClassSymbol::GetPropertyType(const string & name) const noexcept
		{
			for (auto & desc : _prop_list) if (desc.name == name) return &desc.type_cn;
			return 0;
		}
		void ClassSymbol::GetPropertyMethods(const string & name, string * get, string * set) const noexcept
		{
			try { for (auto & desc : _prop_list) if (desc.name == name) { if (get) *get = desc.getter; if (set) *set = desc.setter; return; } } catch (...) {}
			if (get) *get = L"";
			if (set) *set = L"";
		}
		const Volumes::Dictionary<string, string> * ClassSymbol::GetPropertyAttributes(const string & name) const noexcept
		{
			for (auto & desc : _prop_list) if (desc.name == name) return &desc.attributes;
			return 0;
		}
		const string * ClassSymbol::GetMethodImplementation(const string & name) const noexcept
		{
			for (auto & desc : _method_list) if (desc.name == name) return &desc.func_name;
			return 0;
		}
		void ClassSymbol::GetMethodVFI(const string & name, int * vft_id, int * vf_id) const noexcept
		{
			for (auto & desc : _method_list) if (desc.name == name) { if (vft_id) *vft_id = desc.vft_index; if (vf_id) *vf_id = desc.vf_index; return; }
			if (vft_id) *vft_id = -1;
			if (vf_id) *vf_id = -1;
		}
		const Volumes::Dictionary<string, string> * ClassSymbol::GetMethodAttributes(const string & name) const noexcept
		{
			for (auto & desc : _method_list) if (desc.name == name) return &desc.attributes;
			return 0;
		}

		VariableSymbol::VariableSymbol(void * at, uint32 offset, uint32 size, const string & type, const Volumes::Dictionary<string, string> & attrs) :
			_attributes(attrs), _size(size), _type(type) { _address = reinterpret_cast<char *>(at) + offset; }
		VariableSymbol::~VariableSymbol(void) {}
		SymbolType VariableSymbol::GetSymbolType(void) const noexcept { return SymbolType::Variable; }
		void * VariableSymbol::GetSymbolEntity(void) const noexcept { return _address; }
		const Volumes::Dictionary<string, string> * VariableSymbol::GetAttributes(void) const noexcept { return &_attributes; }
		uint32 VariableSymbol::GetSize(void) const noexcept { return _size; }
		const string & VariableSymbol::GetType(void) const noexcept { return _type; }

		FunctionSymbol::FunctionSymbol(void * code, uint32 flags, const string & type, const Volumes::Dictionary<string, string> & attrs) :
			_code(code), _flags(flags), _type(type), _attributes(attrs) {}
		FunctionSymbol::~FunctionSymbol(void) {}
		SymbolType FunctionSymbol::GetSymbolType(void) const noexcept { return SymbolType::Function; }
		void * FunctionSymbol::GetSymbolEntity(void) const noexcept { return _code; }
		const Volumes::Dictionary<string, string> * FunctionSymbol::GetAttributes(void) const noexcept { return &_attributes; }
		uint32 FunctionSymbol::GetFlags(void) const noexcept { return _flags; }
		const string & FunctionSymbol::GetType(void) const noexcept { return _type; }

		AliasSymbol::AliasSymbol(const string & to) : _to(to) {}
		AliasSymbol::~AliasSymbol(void) {}
		SymbolType AliasSymbol::GetSymbolType(void) const noexcept { return SymbolType::Alias; }
		void * AliasSymbol::GetSymbolEntity(void) const noexcept { return 0; }
		const Volumes::Dictionary<string, string> * AliasSymbol::GetAttributes(void) const noexcept { return 0; }
		const string & AliasSymbol::GetDestination(void) const noexcept { return _to; }
	}
}