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
			if (name == SemanticVertex) return true;
			if (name == SemanticInstance) return true;
			if (name == SemanticPosition) return true;
			if (name == SemanticInterstageNI) return true;
			if (name == SemanticInterstageIL) return true;
			if (name == SemanticInterstageIP) return true;
			if (name == SemanticFrontFacing) return true;
			if (name == SemanticColor) return true;
			if (name == SemanticSecondColor) return true;
			if (name == SemanticDepth) return true;
			if (name == SemanticStencil) return true;
			if (name == SemanticConstant) return true;
			if (name == SemanticBuffer) return true;
			if (name == SemanticTexture) return true;
			if (name == SemanticSampler) return true;
			return false;
		}
		bool ValidateArgumentSemantics(const string & name, int index)
		{
			if (name == SemanticVertex) {
				return index >= -1 && index < 1;
			}
			if (name == SemanticInstance) {
				return index >= -1 && index < 1;
			}
			if (name == SemanticPosition) {
				return index >= -1 && index < 1;
			}
			if (name == SemanticInterstageNI) {
				return index >= -1 && index < SelectorLimitInterstage;
			}
			if (name == SemanticInterstageIL) {
				return index >= -1 && index < SelectorLimitInterstage;
			}
			if (name == SemanticInterstageIP) {
				return index >= -1 && index < SelectorLimitInterstage;
			}
			if (name == SemanticFrontFacing) {
				return index >= -1 && index < 1;
			}
			if (name == SemanticColor) {
				return index >= -1 && index < SelectorLimitRenderTarget;
			}
			if (name == SemanticSecondColor) {
				return index >= -1 && index < 1;
			}
			if (name == SemanticDepth) {
				return index >= -1 && index < 1;
			}
			if (name == SemanticStencil) {
				return index >= -1 && index < 1;
			}
			if (name == SemanticConstant) {
				return index >= -1 && index < SelectorLimitConstantBuffer;
			}
			if (name == SemanticBuffer) {
				return index >= -1 && index < SelectorLimitBuffer;
			}
			if (name == SemanticTexture) {
				return index >= -1 && index < SelectorLimitTexture;
			}
			if (name == SemanticSampler) {
				return index >= -1 && index < SelectorLimitSampler;
			}
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
			names << SemanticVertex << SemanticInstance << SemanticPosition << SemanticInterstageNI
				<< SemanticInterstageIL << SemanticInterstageIP << SemanticFrontFacing << SemanticColor
				<< SemanticSecondColor << SemanticDepth << SemanticStencil << SemanticConstant
				<< SemanticBuffer << SemanticTexture << SemanticSampler;
		}
		void ListShaderLanguages(Array<string> & names) { names << L"hlsl" << L"msl" << L"glsl"; }
		void MakeAssemblerHint(XL::LFunctionContext & fctx, uint hint)
		{
			auto statement = XA::TH::MakeStatementNOP();
			statement.opcode = hint;
			fctx.GetDestination().instset << statement;
		}
		void ReadFunctionInformation(const string & fcn, XI::Module::Function & func, string & fname, FunctionDesignation & fdes, ArgumentDesc & rv, Array<ArgumentDesc> & args)
		{
			int del = fcn.FindFirst(L':');
			fname = fcn.Fragment(0, del);
			auto ftcn = fcn.Fragment(del + 1, -1);
			XI::Module::TypeReference ftype(ftcn);
			SafePointer< Array<XI::Module::TypeReference> > sign = ftype.GetFunctionSignature();
			string sdata;
			auto sdata_ptr = func.attributes[AttributeVertex];
			if (sdata_ptr) {
				sdata = *sdata_ptr;
				fdes = FunctionDesignation::Vertex;
			} else {
				sdata_ptr = func.attributes[AttributePixel];
				if (sdata_ptr) {
					sdata = *sdata_ptr;
					fdes = FunctionDesignation::Pixel;
				} else fdes = FunctionDesignation::Service;
			}
			rv.name = L"";
			rv.tcn = sign->ElementAt(0).QueryCanonicalName();
			rv.inout = true;
			rv.semantics = ArgumentSemantics::Undefined;
			rv.index = 0;
			args.Clear();
			auto sdata_parts = sdata.Split(L';');
			for (int i = 0; i < sign->Length() - 1; i++) {
				ArgumentDesc arg;
				auto & type = sign->ElementAt(1 + i);
				if (type.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
					arg.inout = true;
					arg.tcn = type.GetReferenceDestination().QueryCanonicalName();
				} else {
					arg.inout = false;
					arg.tcn = type.QueryCanonicalName();
				}
				if (i < sdata_parts.Length() && fdes != FunctionDesignation::Service) {
					int cindex = sdata_parts[i].FindFirst(L':');
					int sindex = sdata_parts[i].FindFirst(L'#');
					if (cindex >= 0 && sindex >= 0) {
						auto sname = sdata_parts[i].Fragment(cindex + 1, sindex - cindex - 1);
						auto ssel = sdata_parts[i].Fragment(sindex + 1, -1).ToInt32();
						arg.name = sdata_parts[i].Fragment(0, cindex);
						arg.index = ssel;
						if (sname == SemanticVertex) arg.semantics = ArgumentSemantics::VertexIndex;
						else if (sname == SemanticInstance) arg.semantics = ArgumentSemantics::InstanceIndex;
						else if (sname == SemanticPosition) arg.semantics = ArgumentSemantics::Position;
						else if (sname == SemanticInterstageNI) arg.semantics = ArgumentSemantics::InterstageNI;
						else if (sname == SemanticInterstageIL) arg.semantics = ArgumentSemantics::InterstageIL;
						else if (sname == SemanticInterstageIP) arg.semantics = ArgumentSemantics::InterstageIP;
						else if (sname == SemanticFrontFacing) arg.semantics = ArgumentSemantics::IsFrontFacing;
						else if (sname == SemanticColor) arg.semantics = ArgumentSemantics::Color;
						else if (sname == SemanticSecondColor) arg.semantics = ArgumentSemantics::SecondColor;
						else if (sname == SemanticDepth) arg.semantics = ArgumentSemantics::Depth;
						else if (sname == SemanticStencil) arg.semantics = ArgumentSemantics::Stencil;
						else if (sname == SemanticConstant) arg.semantics = ArgumentSemantics::Constant;
						else if (sname == SemanticBuffer) arg.semantics = ArgumentSemantics::Buffer;
						else if (sname == SemanticTexture) arg.semantics = ArgumentSemantics::Texture;
						else if (sname == SemanticSampler) arg.semantics = ArgumentSemantics::Sampler;
						else arg.semantics = ArgumentSemantics::Undefined;
					} else {
						arg.name = L"";
						arg.semantics = ArgumentSemantics::InterstageIP;
						arg.index = -1;
					}
				} else {
					arg.name = L"";
					arg.semantics = ArgumentSemantics::Undefined;
					arg.index = 0;
				}
				args << arg;
			}
			if (fdes != FunctionDesignation::Service) {
				int counter = -1;
				for (auto & a : args) {
					if (a.semantics == ArgumentSemantics::InterstageNI || a.semantics == ArgumentSemantics::InterstageIL || a.semantics == ArgumentSemantics::InterstageIP) {
						counter = max(counter, a.index);
					}
				}
				for (auto & a : args) {
					if (a.semantics == ArgumentSemantics::InterstageNI || a.semantics == ArgumentSemantics::InterstageIL || a.semantics == ArgumentSemantics::InterstageIP) {
						if (a.index < 0) {
							counter++;
							a.index = counter;
						}
					}
				}
				counter = -1;
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Color) counter = max(counter, a.index);
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Color && a.index < 0) {
					counter++;
					a.index = counter;
				}
				counter = -1;
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Sampler) counter = max(counter, a.index);
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Sampler && a.index < 0) {
					counter++;
					a.index = counter;
				}
				counter = -1;
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Constant) counter = max(counter, a.index);
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Constant && a.index < 0) {
					counter++;
					a.index = counter;
				}
				// Counter is the last index assigned to a constant buffer now
				int tcounter = -1;
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Texture) tcounter = max(tcounter, a.index);
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Texture && a.index < 0) {
					tcounter++;
					a.index = tcounter;
				}
				// TCounter is the last index assigned to a texture now
				counter = max(counter, tcounter);
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Buffer) counter = max(counter, a.index);
				for (auto & a : args) if (a.semantics == ArgumentSemantics::Buffer && a.index < 0) {
					counter++;
					a.index = counter;
				}
				for (auto & a : args) if (a.index < 0) a.index = 0;
			}
		}
	}
}