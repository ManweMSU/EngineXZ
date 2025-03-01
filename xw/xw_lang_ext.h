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
		constexpr const widechar * AttributeService		= L"[servus]";
		constexpr const widechar * AttributeVertex		= L"[vertex]";
		constexpr const widechar * AttributePixel		= L"[punctum]";

		// Per generibus
		constexpr const widechar * AttributePrivate		= L"[privatus]";
		constexpr const widechar * AttributeResource	= L"[auxilium]";
		constexpr const widechar * AttributeAlignment	= L"[polire]";
		constexpr const widechar * AttributeMapXV		= L"[mappa_xv]";
		constexpr const widechar * AttributeMapCXX		= L"[mappa_cxx]";
		constexpr const widechar * AttributeMapHLSL		= L"[mappa_hlsl]";
		constexpr const widechar * AttributeMapMSL		= L"[mappa_msl]";
		constexpr const widechar * AttributeMapGLSL		= L"[mappa_glsl]";

		constexpr const widechar * TypeRetvalVertex		= L"vertex";
		constexpr const widechar * TypeRetvalPixel		= L"punctum";

		constexpr const widechar * SemanticVertex		= L"vertex";
		constexpr const widechar * SemanticInstance		= L"exemplum";
		constexpr const widechar * SemanticPosition		= L"positus";
		constexpr const widechar * SemanticInterstageNI	= L"interpolire_nulle";
		constexpr const widechar * SemanticInterstageIL	= L"interpolire_lineariter";
		constexpr const widechar * SemanticInterstageIP	= L"interpolire_perspective";
		constexpr const widechar * SemanticFrontFacing	= L"princeps";
		constexpr const widechar * SemanticColor		= L"color";
		constexpr const widechar * SemanticSecondColor	= L"color_secundus";
		constexpr const widechar * SemanticDepth		= L"altitudo";
		constexpr const widechar * SemanticStencil		= L"praeformae";
		constexpr const widechar * SemanticConstant		= L"constati";
		constexpr const widechar * SemanticBuffer		= L"series";
		constexpr const widechar * SemanticTexture		= L"textura";
		constexpr const widechar * SemanticSampler		= L"exceptor";
		constexpr const widechar * SemanticResponce		= L"responsum";

		XL::LObject * ProcessVectorRecombination(XL::LContext & ctx, XL::LObject * vector, const string & mask);
		void CreateDefaultImplementation(XL::LObject * on_class, uint flags);
		void CreateDefaultImplementations(XL::LObject * on_class, uint flags);
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

		constexpr uint SelectorLimitRenderTarget	= 8;
		constexpr uint SelectorLimitConstantBuffer	= 14;
		constexpr uint SelectorLimitSampler			= 16;
		constexpr uint SelectorLimitTexture			= 128;
		constexpr uint SelectorLimitBuffer			= 128;
		constexpr uint SelectorLimitInterstage		= 16;

		enum class FunctionDesignation { Service, Vertex, Pixel };
		enum class ArgumentSemantics {
			Undefined,
			VertexIndex,	// vertex in, nint
			InstanceIndex,	// vertex in, nint
			Position,		// vertex ex, punctum in, frac4
			InterstageNI,	// vertex ex, punctum in, fracN; index in [0, 15]
			InterstageIL,	// vertex ex, punctum in, fracN; index in [0, 15]
			InterstageIP,	// vertex ex, punctum in, fracN; index in [0, 15]
			IsFrontFacing,	// punctum in, logicum
			Color,			// punctum ex, frac2, frac3, frac4; index in [0, 7]
			SecondColor,	// punctum ex, frac2, frac3, frac4
			Depth,			// punctum ex, frac
			Stencil,		// punctum ex, nint
			Constant,		// auxilium in; index in [0, 13]
			Buffer,			// auxilium in; index in [0, 127]
			Texture,		// auxilium in; index in [0, 127]
			Sampler,		// auxilium in; index in [0, 15]
		};
		struct ArgumentDesc {
			string name;
			string tcn;
			bool inout, out;
			ArgumentSemantics semantics;
			int index;
		};
		struct FunctionDesc
		{
			string fname;
			string instance_tcn;
			bool constructor;
			FunctionDesignation fdes;
			ArgumentDesc rv;
			Array<ArgumentDesc> args;
		};
		void ReadFunctionInformation(const string & fcn, XI::Module::Function & func, FunctionDesc & desc);
	}
}