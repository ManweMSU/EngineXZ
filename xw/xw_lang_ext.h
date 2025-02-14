#pragma once

#include "xw_types.h"
#include "../xlang/xl_lal.h"
#include "../xlang/xl_code.h"

namespace Engine
{
	namespace XW
	{
		// Per functionibus
		constexpr const widechar * AttributeRules		= L"[xw]";
		constexpr const widechar * AttributeVertex		= L"[vertex]";
		constexpr const widechar * AttributePixel		= L"[punctum]";

		// Per generibus
		constexpr const widechar * AttributePrivate		= L"[privatus]";
		constexpr const widechar * AttributeAlignment	= L"[polire]";
		constexpr const widechar * AttributeMapXV		= L"[mappa_xv]";
		constexpr const widechar * AttributeMapCXX		= L"[mappa_cxx]";
		constexpr const widechar * AttributeMapHLSL		= L"[mappa_hlsl]";
		constexpr const widechar * AttributeMapMSL		= L"[mappa_msl]";
		constexpr const widechar * AttributeMapGLSL		= L"[mappa_glsl]";

		constexpr const widechar * TypeRetvalVertex		= L"vertex";
		constexpr const widechar * TypeRetvalPixel		= L"punctum";

		XL::LObject * ProcessVectorRecombination(XL::LContext & ctx, XL::LObject * vector, const string & mask);
		ShaderLanguage ProcessShaderLanguage(const string & name);
		void SetXWImplementation(XL::LObject * func);
		void SetXWImplementation(XL::LObject * func, ShaderLanguage lang, const TranslationRules & rules);
		void AddArgumentSemantics(DynamicString & sword, const string & aname, const string & sname, int index);
		void AddArgumentSemantics(DynamicString & sword);
		bool ValidateArgumentSemantics(const string & name);
		bool ValidateArgumentSemantics(const string & name, int index);
		bool ValidateVariableType(XL::LObject * type, bool allow_ref);
		void ListArgumentSemantics(Array<string> & names);
		void ListShaderLanguages(Array<string> & names);
		void MakeAssemblerHint(XL::LFunctionContext & fctx, uint hint);
	}
}