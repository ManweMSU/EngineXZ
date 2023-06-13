#pragma once

#include "../xasm/xa_types.h"

namespace Engine
{
	namespace XE
	{
		class Module : public Object
		{
		public:
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
				Class Contents;
				uint Length;
				union {
					bool DataBoolean;
					uint8 DataUInt8;
					int8 DataSInt8;
					uint16 DataUInt16;
					int16 DataSInt16;
					uint32 DataUInt32;
					int32 DataSInt32;
					uint64 DataUInt64;
					int64 DataSInt64;
					float DataFloat;
					double DataDouble;
				};
				string DataString;
			};
			class Variable
			{
			public:
				string TypeCanonicalName;
				XA::ObjectSize Offset, Size;
				Volumes::Dictionary<string, string> Attributes;
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
				uint CodeFlags;
				Point VFT_Index;
				SafePointer<DataBlock> Code;
				Volumes::Dictionary<string, string> Attributes;
			};
			class Property
			{
			public:
				Function Getter, Setter;
			};
			class Interface
			{
			public:
				string InterfaceName; // FQN
				XA::ObjectSize VFT_PointerOffset;
			};
			class Class
			{
			public:
				enum class Nature { Core, Standard, Interface };
			public:
				Nature ClassNature;
				XA::ObjectSize InstanceSize;
				string ParentClassName; // FQN
				XA::ObjectSize Parent_VFT_PointerOffset;
				Array<Interface> InterfacesImplements;
				Volumes::Dictionary<string, Variable> Fields;		// NAME
				Volumes::Dictionary<string, Property> Properties;	// NAME
				Volumes::Dictionary<string, Function> Methods;		// NAME:SCN
				Volumes::Dictionary<string, string> Attributes;

				Class(void);
			};
		public:
			Array<string> ModulesDependsOn;
			Volumes::Dictionary<string, Literal> Literals;			// FQN
			Volumes::Dictionary<string, Class> Classes;				// FQN
			Volumes::Dictionary<string, Variable> Variables;		// FQN
			Volumes::Dictionary<string, Function> Functions;		// FQN:SCN
			Volumes::Dictionary<string, string> Aliases;			// FQN
			Volumes::ObjectDictionary<string, DataBlock> Resources;	// TYPE:ID
			SafePointer<DataBlock> DataSegment;
		public:
			Module(void);
			Module(Streaming::Stream * source);
			virtual ~Module(void) override;
			void Save(Streaming::Stream * dest);
		};
	}
}