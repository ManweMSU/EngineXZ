#pragma once

#include "../xasm/xa_types.h"

namespace Engine
{
	namespace XE
	{
		class Module : public Object
		{
		public:
			enum class ExecutionSubsystem { NoUI, ConsoleUI, GUI, Library };
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
				enum class Class { Boolean, SignedInteger, UnsignedInteger, FloatingPoint, String };
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
					FunctionClassNull	= 0x000000,

					FunctionClassXA		= 0x010000,
					FunctionXA_Abstract	= 0x000000,
					FunctionXA_Platform	= 0x000001,

					FunctionClassImport	= 0x020000,
					FunctionImportNear	= 0x000000,
					FunctionImportFar	= 0x000001,

					FunctionTypeMask	= 0x00FFFF,
					FunctionClassMask	= 0xFF0000,

					FunctionEntryPoint	= 0x01000000,
					FunctionInitialize	= 0x02000000,
					FunctionShutdown	= 0x04000000,
					FunctionMiscMask	= 0xFF000000,
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
				enum class Nature { Core, Standard, Interface };
			public:
				Nature class_nature;
				XA::ArgumentSpecification instance_spec;
				string parent_class_name; // FQN
				XA::ObjectSize parent_vft_pointer_offset;
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
			virtual ~Module(void) override;
			void Save(Streaming::Stream * dest);
		};
	}
}