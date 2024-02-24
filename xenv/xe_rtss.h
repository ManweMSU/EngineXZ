#pragma once

#include "../xasm/xa_types.h"

namespace Engine
{
	namespace XE
	{
		enum class SymbolType { Literal, Class, Variable, Function, Alias };

		class SymbolObject : public Object
		{
		public:
			virtual SymbolType GetSymbolType(void) const noexcept = 0;
			virtual void * GetSymbolEntity(void) const noexcept = 0;
			virtual const Volumes::Dictionary<string, string> * GetAttributes(void) const noexcept = 0;
		};
		class SymbolSystem : public Object
		{
			Volumes::ObjectDictionary<string, SymbolObject> _symbols;
		public:
			SymbolSystem(void);
			virtual ~SymbolSystem(void) override;
			bool RegisterSymbol(SymbolObject * symbol, const string & name) noexcept;
			const SymbolObject * FindSymbol(const string & name, bool follow_aliases = true, int max_jumps = 10) const noexcept;
			Volumes::ObjectDictionary<string, SymbolObject> & GetSymbolTable(void) noexcept;
			const Volumes::ObjectDictionary<string, SymbolObject> & GetSymbolTable(void) const noexcept;
		};

		class LiteralSymbol : public SymbolObject
		{
			uint64 _data_64;
			SafePointer<DataBlock> _data_large;
			Reflection::PropertyType _type;
			Volumes::Dictionary<string, string> _attributes;
		public:
			LiteralSymbol(const void * pdata, Reflection::PropertyType data_type, const Volumes::Dictionary<string, string> & attrs);
			virtual ~LiteralSymbol(void) override;
			virtual SymbolType GetSymbolType(void) const noexcept override;
			virtual void * GetSymbolEntity(void) const noexcept override;
			virtual const Volumes::Dictionary<string, string> * GetAttributes(void) const noexcept override;
			Reflection::PropertyType GetValueType(void) const noexcept;
		};
		class ClassSymbol : public SymbolObject
		{
			struct _interface_desc {
				string name;
				int vft_offset;
				int cast_to_offset;
			};
			struct _field_desc {
				string name, type_cn;
				uint offset, size;
				Volumes::Dictionary<string, string> attributes;
			};
			struct _prop_desc {
				string name, type_cn, setter, getter;
				Volumes::Dictionary<string, string> attributes;
			};
			struct _method_desc {
				string name, func_name;
				int vft_index, vf_index;
				Volumes::Dictionary<string, string> attributes;
			};

			string _class_name;
			XA::ArgumentSemantics _semantics;
			uint _instance_size;
			_interface_desc _parent_class;
			Array<_interface_desc> _interface_list;
			Array<_field_desc> _field_list;
			Array<_prop_desc> _prop_list;
			Array<_method_desc> _method_list;
			Volumes::Dictionary<string, string> _attributes;
		public:
			ClassSymbol(const string & name, XA::ArgumentSemantics semantics, uint size, const Volumes::Dictionary<string, string> & attrs);
			virtual ~ClassSymbol(void) override;
			virtual SymbolType GetSymbolType(void) const noexcept override;
			virtual void * GetSymbolEntity(void) const noexcept override;
			virtual const Volumes::Dictionary<string, string> * GetAttributes(void) const noexcept override;
			void AddParentClass(const string & name, int vft_offset, int cast_offset);
			void AddInterface(const string & name, int vft_offset, int cast_offset);
			void AddField(const string & name, const string & type, uint offset, uint size, const Volumes::Dictionary<string, string> & attrs);
			void AddProperty(const string & name, const string & type, const string & setter, const string & getter, const Volumes::Dictionary<string, string> & attrs);
			void AddMethod(const string & name, const string & function, int vft_id, int vf_id, const Volumes::Dictionary<string, string> & attrs);
			const string & GetClassName(void) const noexcept;
			XA::ArgumentSemantics GetClassSemantics(void) const noexcept;
			uint GetInstanceSize(void) const noexcept;
			uint GetVFTOffset(void) const noexcept;
			const string & GetParentClassName(void) const noexcept;
			Array<string> * ListInterfaces(void) const noexcept;
			Array<string> * ListFields(void) const noexcept;
			Array<string> * ListFields(const string & with_attr) const noexcept;
			Array<string> * ListFields(const string & with_attr, const string & of_value) const noexcept;
			Array<string> * ListProperties(void) const noexcept;
			Array<string> * ListProperties(const string & with_attr) const noexcept;
			Array<string> * ListProperties(const string & with_attr, const string & of_value) const noexcept;
			Array<string> * ListMethods(void) const noexcept;
			Array<string> * ListMethods(const string & with_attr) const noexcept;
			Array<string> * ListMethods(const string & with_attr, const string & of_value) const noexcept;
			void * GetInterfaceVFT(const string & name, void * instance) const noexcept;
			void * CastToInterface(const string & name, void * instance) const noexcept;
			void * CastFromInterface(const string & name, void * instance) const noexcept;
			const string * GetFieldType(const string & name) const noexcept;
			void * GetFieldAddress(const string & name, void * instance) const noexcept;
			uint GetFieldSize(const string & name) const noexcept;
			const Volumes::Dictionary<string, string> * GetFieldAttributes(const string & name) const noexcept;
			const string * GetPropertyType(const string & name) const noexcept;
			void GetPropertyMethods(const string & name, string * get, string * set) const noexcept;
			const Volumes::Dictionary<string, string> * GetPropertyAttributes(const string & name) const noexcept;
			const string * GetMethodImplementation(const string & name) const noexcept;
			void GetMethodVFI(const string & name, int * vft_id, int * vf_id) const noexcept;
			const Volumes::Dictionary<string, string> * GetMethodAttributes(const string & name) const noexcept;
		};
		class VariableSymbol : public SymbolObject
		{
			void * _address;
			uint _size;
			string _type;
			Volumes::Dictionary<string, string> _attributes;
		public:
			VariableSymbol(void * at, uint32 offset, uint32 size, const string & type, const Volumes::Dictionary<string, string> & attrs);
			virtual ~VariableSymbol(void) override;
			virtual SymbolType GetSymbolType(void) const noexcept override;
			virtual void * GetSymbolEntity(void) const noexcept override;
			virtual const Volumes::Dictionary<string, string> * GetAttributes(void) const noexcept override;
			uint32 GetSize(void) const noexcept;
			const string & GetType(void) const noexcept;
		};
		class FunctionSymbol : public SymbolObject
		{
			void * _code;
			uint32 _flags;
			string _type;
			Volumes::Dictionary<string, string> _attributes;
		public:
			FunctionSymbol(void * code, uint32 flags, const string & type, const Volumes::Dictionary<string, string> & attrs);
			virtual ~FunctionSymbol(void) override;
			virtual SymbolType GetSymbolType(void) const noexcept override;
			virtual void * GetSymbolEntity(void) const noexcept override;
			virtual const Volumes::Dictionary<string, string> * GetAttributes(void) const noexcept override;
			uint32 GetFlags(void) const noexcept;
			const string & GetType(void) const noexcept;
			void SetFlags(uint32 flags) noexcept;
		};
		class AliasSymbol : public SymbolObject
		{
			string _to;
		public:
			AliasSymbol(const string & to);
			virtual ~AliasSymbol(void) override;
			virtual SymbolType GetSymbolType(void) const noexcept override;
			virtual void * GetSymbolEntity(void) const noexcept override;
			virtual const Volumes::Dictionary<string, string> * GetAttributes(void) const noexcept override;
			const string & GetDestination(void) const noexcept;
		};
	}
}