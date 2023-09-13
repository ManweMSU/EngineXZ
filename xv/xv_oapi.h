#pragma once

#include "../xlang/xl_lal.h"
#include "../xlang/xl_code.h"

namespace Engine
{
	namespace XV
	{
		XL::LObject * CreateOperatorNew(XL::LContext & ctx);
		XL::LObject * CreateOperatorConstruct(XL::LContext & ctx);

		XL::LObject * CreateNew(XL::LContext & ctx, XL::LObject * type, int argc, XL::LObject ** argv);
		XL::LObject * CreateConstruct(XL::LContext & ctx, XL::LObject * at, int argc, XL::LObject ** argv);
		XL::LObject * CreateDelete(XL::LObject * object);
		XL::LObject * CreateFree(XL::LObject * object);
		XL::LObject * CreateDestruct(XL::LObject * object);
		XL::LObject * CreateDynamicCast(XL::LObject * object, XL::LObject * type_into);

		bool ClassImplements(XL::LObject * cls, const string & impl);
		bool IsValidEnumerationBase(XL::LObject * type);
		bool CreateEnumerationValue(Volumes::ObjectDictionary<string, XL::LObject> & enum_db, XL::LObject * enum_type, const string & name, XL::LObject * value);
		void CreateEnumerationRoutines(Volumes::ObjectDictionary<string, XL::LObject> & enum_db, XL::LObject * enum_type);
		void CreateTypeServiceRoutines(XL::LObject * cls);

		void BeginContextCapture(XL::LObject * base_class, XL::LObject ** vlist, int vlen, XL::LObject ** capture, XL::LObject ** function);
		void ConfigureContextCapture(XL::LObject * capture, XL::LObject * function, XL::LFunctionContext ** fctx);
		void EndContextCapture(XL::LObject * capture, ObjectArray<XL::LObject> & vft_init, XL::LObject ** task);

		string GetPurePath(XL::LContext & ctx, XL::LObject * object);
		string GetPurePath(XL::LContext & ctx, const string & object);
		string GetObjectFullName(XL::LContext & ctx, XL::LObject * object);
		string GetTypeCanonicalName(XL::LContext & ctx, XL::LObject * object);
		string GetTypeFullName(XL::LContext & ctx, XL::LObject * object);
		string GetLiteralValue(XL::LContext & ctx, XL::LObject * object);
		void GetFields(XL::LObject * cls, Array<string> & names);
		bool NameIsPrivate(const string & name);
	}
}