#pragma once

#include "../xasm/xa_types.h"

namespace Engine
{
	namespace XI
	{
		class Module : public Object
		{
		public:
			enum class ModuleLoadFlags {
				LoadAll			= 0x00,
				LoadExecute		= 0x01,
				LoadLink		= 0x02,
				LoadResources	= 0x03,
			};
			enum class ExecutionSubsystem { NoUI = 0, ConsoleUI = 1, GUI = 2, Library = 3 };
			class TypeReference
			{
			public:
				enum class Class { Unknown, Class, AbstractPlaceholder, Array, Pointer, Reference, Function, AbstractInstance };
			private:
				const string & _desc;
				int _from, _length;
			public:
				TypeReference(const string & src);
				TypeReference(const string & src, int from, int length);

				Class GetReferenceClass(void) const;
				string GetClassName(void) const;
				int GetAbstractPlaceholderIndex(void) const;
				int GetArrayVolume(void) const;
				TypeReference GetArrayElement(void) const;
				TypeReference GetPointerDestination(void) const;
				TypeReference GetReferenceDestination(void) const;
				Array<TypeReference> * GetFunctionSignature(void) const;
				TypeReference GetAbstractInstanceBase(void) const;
				int GetAbstractInstanceParameterIndex(void) const;
				TypeReference GetAbstractInstanceParameterType(void) const;

				static string MakeClassReference(const string & class_name);
				static string MakeAbstractPlaceholder(int param_index);
				static string MakeArray(const string & cn, int volume);
				static string MakePointer(const string & cn);
				static string MakeReference(const string & cn);
				static string MakeFunction(const string & rv_cn, const Array<string> * args_cn);
				static string MakeInstance(const string & cn_of, int param_index, const string & cn_with);

				string ToString(void) const;
			};
			class Literal
			{
			public:
				enum class Class { Boolean = 1, SignedInteger = 2, UnsignedInteger = 3, FloatingPoint = 4, String = 5 };
			public:
				Class contents;
				uint length;
				union {
					bool data_boolean;
					uint8 data_uint8;
					int8 data_sint8;
					uint16 data_uint16;
					int16 data_sint16;
					uint32 data_uint32;
					int32 data_sint32;
					uint64 data_uint64;
					int64 data_sint64;
					float data_float;
					double data_double;
				};
				string data_string;
				Volumes::Dictionary<string, string> attributes;
			};
			class Variable
			{
			public:
				string type_canonical_name;
				XA::ObjectSize offset, size;
				Volumes::Dictionary<string, string> attributes;
			};
			class Function
			{
			public:
				enum FunctionCodeFlags : uint {
					FunctionClassNull	= 0x00,

					FunctionClassXA		= 0x10,
					FunctionXA_Abstract	= 0x00,
					FunctionXA_Platform	= 0x01,

					FunctionClassImport	= 0x20,
					FunctionImportNear	= 0x00,
					FunctionImportFar	= 0x01,

					FunctionTypeMask	= 0x0F,
					FunctionClassMask	= 0xF0,

					FunctionEntryPoint	= 0x00000100,
					FunctionInitialize	= 0x00000200,
					FunctionShutdown	= 0x00000400,

					FunctionInstance	= 0x00000800,
					FunctionThrows		= 0x00001000,
					FunctionThisCall	= 0x00002000,

					FunctionMiscMask	= 0xFFFFFF00,
				};
			public:
				uint code_flags;
				Point vft_index; // Index of VFT : Index of function in VFT
				SafePointer<DataBlock> code;
				Volumes::Dictionary<string, string> attributes;
			};
			class Property
			{
			public:
				string type_canonical_name, getter_interface, setter_interface;
				Function getter, setter;
				Volumes::Dictionary<string, string> attributes;
			};
			class Interface
			{
			public:
				string interface_name; // FQN
				XA::ObjectSize vft_pointer_offset;
			};
			class Class
			{
			public:
				enum class Nature { Core = 0, Standard = 1, Interface = 2 };
			public:
				Nature class_nature;
				XA::ArgumentSpecification instance_spec;
				Interface parent_class;
				Array<Interface> interfaces_implements;
				Volumes::Dictionary<string, Variable> fields;		// NAME
				Volumes::Dictionary<string, Property> properties;	// NAME
				Volumes::Dictionary<string, Function> methods;		// NAME:SCN
				Volumes::Dictionary<string, string> attributes;

				Class(void);
			};
			class Version
			{
			public:
				uint major, minor, subver, build;
			};
		public:
			ExecutionSubsystem subsystem;
			string module_import_name, assembler_name;
			Version assembler_version;
			Array<string> modules_depends_on;
			Volumes::Dictionary<string, Literal> literals;			// FQN
			Volumes::Dictionary<string, Class> classes;				// FQN
			Volumes::Dictionary<string, Variable> variables;		// FQN
			Volumes::Dictionary<string, Function> functions;		// FQN:SCN
			Volumes::Dictionary<string, string> aliases;			// FQN
			Volumes::ObjectDictionary<string, DataBlock> resources;	// TYPE:ID
			SafePointer<DataBlock> data;
		public:
			Module(void);
			Module(Streaming::Stream * source);
			Module(Streaming::Stream * source, ModuleLoadFlags flags);
			virtual ~Module(void) override;
			void Save(Streaming::Stream * dest);
		};
	}
}