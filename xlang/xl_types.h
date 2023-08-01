#pragma once

#include "xl_com.h"

namespace Engine
{
	namespace XL
	{
		class XType : public XObject
		{
		public:
			virtual Class GetClass(void) override;
			virtual LObject * GetType(void) override;
			virtual LObject * Invoke(int argc, LObject ** argv) override;
			virtual LContext & GetContext(void) = 0;
			virtual string GetCanonicalType(void) = 0;
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) = 0;
			virtual const XI::Module::TypeReference & GetTypeReference(void) = 0;
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) = 0;
			virtual LObject * GetConstructorInit(void) = 0;
			virtual LObject * GetConstructorCopy(void) = 0;
			virtual LObject * GetConstructorZero(void) = 0;
			virtual LObject * GetConstructorMove(void) = 0;
			virtual LObject * GetConstructorCast(XType * from_type) = 0;
			virtual LObject * GetCastMethod(XType * to_type) = 0;
			virtual LObject * GetDestructor(void) = 0;
			virtual void GetTypesConformsTo(ObjectArray<XType> & types) = 0;
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) = 0;
		};
		class XClass : public XType
		{
		public:
			virtual XI::Module::Class::Nature GetLanguageSemantics(void) = 0;
			virtual void OverrideArgumentSpecification(XA::ArgumentSpecification spec) = 0;
			virtual void OverrideLanguageSemantics(XI::Module::Class::Nature spec) = 0;
			virtual void UpdateInternals(void) = 0;
			virtual void AdoptParentClass(XClass * parent, bool alternate = true) = 0;
			virtual void AdoptInterface(XClass * interface) = 0;
			virtual void AdoptInterface(XClass * interface, int vft_index, XA::ObjectSize vft_offset) = 0;
			virtual XClass * GetParentClass(void) = 0;
			virtual int GetInterfaceCount(void) = 0;
			virtual XClass * GetInterface(int index) = 0;
			virtual XA::ObjectSize GetPrimaryVFT(void) = 0;
			virtual void SetPrimaryVFT(XA::ObjectSize offset) = 0;
			virtual VirtualFunctionDesc FindVirtualFunctionInfo(const string & name, const string & sign, uint & flags) = 0;
			virtual int SizeOfPrimaryVFT(void) = 0;
			virtual void GetRangeVFT(int & first, int & last) = 0;
			virtual void GetRangeVF(int vft, int & first, int & last) = 0;
			virtual XA::ObjectSize GetOffsetVFT(int vft) = 0;
			virtual LObject * GetCurrentImplementationForVF(int vft, int vf) = 0;
			virtual LObject * CreateVFT(int vft, ObjectArray<LObject> & init_seq) = 0;
		};
		class XArray : public XType
		{
		public:
			virtual XType * GetElementType(void) = 0;
			virtual int GetVolume(void) = 0;
		};
		class XPointer : public XType
		{
		public:
			virtual XType * GetElementType(void) = 0;
		};
		class XReference : public XType
		{
		public:
			virtual XType * GetElementType(void) = 0;
		};
		class XFunctionType : public XType
		{
		public:
			virtual int GetArgumentCount(void) = 0;
			virtual XType * GetArgument(int index) = 0;
			virtual XType * GetReturnValue(void) = 0;
		};

		XType * CreateType(const string & cn, LContext & ctx);
		XClass * CreateClass(const string & name, const string & path, bool local, LContext & ctx);

		int CheckCastPossibility(XType * dest, XType * src, int min_level);
		LObject * ConstructObject(XType * of_type, LObject * with_ctor, int argc, LObject ** argv);
		LObject * PerformTypeCast(XType * dest, LObject * src, int min_level, bool enforce_copy = false);
		LObject * InitInstance(LObject * instance, LObject * with_value);
		LObject * InitInstance(LObject * instance, int argc, LObject ** argv);
		LObject * ZeroInstance(LObject * instance);
		LObject * DestroyInstance(LObject * instance);
	}
}