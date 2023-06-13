#pragma once

#include "xasm/xa_types.h"

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
			};
		public:
			Array<string> ModuleDependsOn;
			Volumes::Dictionary<string, Literal> Literals;

			// TODO: IMPLEMENT
			//   CLASS TYPE DATABASE AND METADATA
			//   SYMBOL DATABASE (VARIABLES AND FUNCTIONS)

			//   DATA SEGMENT

			Volumes::Dictionary<string, Variable> Variables;
			Volumes::Dictionary<string, string> Aliases;
			SafePointer<DataBlock> DataSegment;

			
			//   RESOURCE SEGMENT
		public:
			Module(void);
			Module(Streaming::Stream * source);
			virtual ~Module(void) override;
		};
	}
}