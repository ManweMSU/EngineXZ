#include "xw_lang_ext.h"
#include "../xasm/xa_type_helper.h"
#include "../xlang/xl_types.h"
#include "../xlang/xl_func.h"

namespace Engine
{
	namespace XW
	{
		XL::LObject * ProcessVectorRecombination(XL::LContext & ctx, XL::LObject * vector, const string & mask)
		{
			// TODO: IMPLEMENT
			throw Exception();
		}
		ShaderLanguage ProcessShaderLanguage(const string & name)
		{
			if (name == L"hlsl") return ShaderLanguage::HLSL;
			else if (name == L"msl") return ShaderLanguage::MSL;
			else if (name == L"glsl") return ShaderLanguage::GLSL;
			else throw InvalidArgumentException();
		}
		void SetXWImplementation(XL::LObject * func)
		{
			if (func->GetClass() != XL::Class::FunctionOverload) throw InvalidArgumentException();
			auto & desc = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
			desc._is_xw = true;
			desc._xw.Clear();
		}
		void SetXWImplementation(XL::LObject * func, ShaderLanguage lang, const TranslationRules & rules)
		{
			if (func->GetClass() != XL::Class::FunctionOverload) throw InvalidArgumentException();
			auto & desc = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
			desc._is_xw = true;
			desc._xw.Append(lang, rules);
		}
		void AddArgumentSemantics(DynamicString & sword, const string & aname, const string & sname, int index) { sword << aname << L":" << sname << L"#" << index << L";"; }
		void AddArgumentSemantics(DynamicString & sword) { sword << L";"; }
		bool ValidateArgumentSemantics(const string & name)
		{
			return (name == L"test_semantic" || name == L"test_trait");

			// TODO: IMPLEMENT
			return false;
		}
		bool ValidateArgumentSemantics(const string & name, int index)
		{
			return (name == L"test_semantic" || name == L"test_trait") && index < 10;

			// TODO: IMPLEMENT
			return false;
		}
		bool ValidateVariableType(XL::LObject * type, bool allow_ref)
		{
			if (type->GetClass() != XL::Class::Type) return false;
			auto tcn = static_cast<XL::XType *>(type)->GetCanonicalType();
			XI::Module::TypeReference tref(tcn);
			if (tref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) return false;
			if (tref.GetReferenceClass() == XI::Module::TypeReference::Class::Function) return false;
			if (tref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference && !allow_ref) return false;
			return true;
		}
		void ListArgumentSemantics(Array<string> & names)
		{
			names << L"test_semantic";
			names << L"test_trait";

			// TODO: IMPLEMENT
		}
		void ListShaderLanguages(Array<string> & names) { names << L"hlsl" << L"msl" << L"glsl"; }
		void MakeAssemblerHint(XL::LFunctionContext & fctx, uint hint)
		{
			auto statement = XA::TH::MakeStatementNOP();
			statement.opcode = hint;
			fctx.GetDestination().instset << statement;
		}
	}
}