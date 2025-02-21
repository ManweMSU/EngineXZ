#include "xw_lang_ext.h"
#include "../xasm/xa_type_helper.h"
#include "../xlang/xl_types.h"
#include "../xlang/xl_func.h"
#include "../xlang/xl_var.h"
#include "../xlang/xl_synth.h"

namespace Engine
{
	namespace XW
	{
		class RecombinationProvider : public XL::IComputableProvider, public Object
		{
			XL::LContext & _ctx;
			uint _mask;
			uint _short_flag;
			SafePointer<XL::LObject> _vector;
			SafePointer<XL::XType> _src_type;
			SafePointer<XL::XType> _dest_type;
		public:
			RecombinationProvider(XL::LContext & ctx, XL::LObject * vector, XL::XType * src, const string & mask) : _ctx(ctx)
			{
				_vector.SetRetain(vector);
				_src_type.SetRetain(src);
				auto cname = src->GetFullName();
				int idim;
				string rname;
				if (cname == L"logicum2") {
					rname = L"logicum"; idim = 2; _short_flag = XA::ReferenceFlagShort;
				} else if (cname == L"logicum3") {
					rname = L"logicum"; idim = 3; _short_flag = XA::ReferenceFlagShort;
				} else if (cname == L"logicum4") {
					rname = L"logicum"; idim = 4; _short_flag = XA::ReferenceFlagShort;
				} else if (cname == L"int2") {
					rname = L"int"; idim = 2; _short_flag = 0;
				} else if (cname == L"int3") {
					rname = L"int"; idim = 3; _short_flag = 0;
				} else if (cname == L"int4") {
					rname = L"int"; idim = 4; _short_flag = 0;
				} else if (cname == L"nint2") {
					rname = L"nint"; idim = 2; _short_flag = 0;
				} else if (cname == L"nint3") {
					rname = L"nint"; idim = 3; _short_flag = 0;
				} else if (cname == L"nint4") {
					rname = L"nint"; idim = 4; _short_flag = 0;
				} else if (cname == L"frac2") {
					rname = L"frac"; idim = 2; _short_flag = 0;
				} else if (cname == L"frac3") {
					rname = L"frac"; idim = 3; _short_flag = 0;
				} else if (cname == L"frac4") {
					rname = L"frac"; idim = 4; _short_flag = 0;
				} else throw InvalidArgumentException();
				int odim = mask.Length();
				if (odim < 1 || odim > 4) throw InvalidArgumentException();
				_mask = 0;
				for (int i = 0; i < odim; i++) {
					int selector;
					auto w = mask[i];
					if (w == L'x' || w == L'X' || w == L'r' || w == L'R' || w == L'u' || w == L'U') selector = 0;
					else if (w == L'y' || w == L'Y' || w == L'g' || w == L'G' || w == L'v' || w == L'V') selector = 1;
					else if (w == L'z' || w == L'Z' || w == L'b' || w == L'B') selector = 2;
					else if (w == L'w' || w == L'W' || w == L'a' || w == L'A') selector = 3;
					else throw InvalidArgumentException();
					if (selector >= idim) throw InvalidArgumentException();
					_mask |= selector << (4 * i);
				}
				if (odim > 1) _dest_type = XL::CreateType(XI::Module::TypeReference::MakeClassReference(rname + string(odim)), _ctx);
				else _dest_type = XL::CreateType(XI::Module::TypeReference::MakeClassReference(rname), _ctx);
			}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XL::XType * ComputableGetType(void) override { _dest_type->Retain(); return _dest_type; }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				XA::ExpressionTree node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFloatRecombine, XA::ReferenceFlagInvoke | _short_flag));
				XA::TH::AddTreeInput(node, _vector->Evaluate(func, error_ctx), _src_type->GetArgumentSpecification());
				XA::TH::AddTreeInput(node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(_mask, 0));
				XA::TH::AddTreeOutput(node, _dest_type->GetArgumentSpecification());
				return node;
			}
		};
		XL::LObject * ProcessVectorRecombination(XL::LContext & ctx, XL::LObject * vector, const string & mask)
		{
			SafePointer<XL::LObject> type = vector->GetType();
			if (type->GetClass() == XL::Class::Type) {
				auto xtype = static_cast<XL::XType *>(type.Inner());
				auto tcn = xtype->GetCanonicalType();
				XI::Module::TypeReference tref(tcn);
				if (tref.GetReferenceClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
				SafePointer<RecombinationProvider> prov = new RecombinationProvider(xtype->GetContext(), vector, xtype, mask);
				return XL::CreateComputable(xtype->GetContext(), prov);
			} else throw InvalidStateException();
		}
		void CreateDefaultImplementation(XL::LObject * on_class, uint flags)
		{
			if (on_class->GetClass() != XL::Class::Type) throw InvalidArgumentException();
			if (static_cast<XL::XType *>(on_class)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto cls = static_cast<XL::XClass *>(on_class);
			auto & ctx = cls->GetContext();
			SafePointer<XL::LObject> class_ref = ctx.QueryTypeReference(on_class);
			SafePointer<XL::LObject> void_type = CreateType(XI::Module::TypeReference::MakeClassReference(XL::NameVoid), ctx);
			ObjectArray<XL::LObject> fields(0x80);
			ObjectArray<XL::XType> subj_types(0x80);
			cls->ListFields(fields);
			for (auto & f : fields) { SafePointer<XL::LObject> type = f.GetType(); subj_types.Append(static_cast<XL::XType *>(type.Inner())); }
			bool discard = false;
			bool standard = false;
			if (flags & XL::CreateMethodConstructorInit) {
				try {
					for (auto & s : subj_types) {
						SafePointer<XL::LObject> ctor = s.GetConstructorInit();
						auto & xfunc = *static_cast<XL::XFunctionOverload *>(ctor.Inner());
						if (!xfunc.GetImplementationDesc()._xw.IsEmpty()) standard = true;
					}
				} catch (...) { discard = true; }
			} else if (flags & XL::CreateMethodConstructorCopy) {
				try {
					for (auto & s : subj_types) {
						SafePointer<XL::LObject> ctor = s.GetConstructorCopy();
						auto & xfunc = *static_cast<XL::XFunctionOverload *>(ctor.Inner());
						if (!xfunc.GetImplementationDesc()._xw.IsEmpty()) standard = true;
					}
				} catch (...) { discard = true; }
			} else if (flags & XL::CreateMethodAssign) {
				try {
					for (auto & s : subj_types) {
						XL::XFunctionOverload * asgn;
						SafePointer<XL::LObject> asgn_fd = s.GetMember(XL::OperatorAssign);
						if (asgn_fd->GetClass() == XL::Class::Function) {
							auto et = s.GetCanonicalType();
							auto et_ref = XI::Module::TypeReference::MakeReference(et);
							try { asgn = static_cast<XL::XFunction *>(asgn_fd.Inner())->GetOverloadT(1, &et_ref, true); } catch (...) { asgn = 0; }
							if (!asgn) asgn = static_cast<XL::XFunction *>(asgn_fd.Inner())->GetOverloadT(1, &et, true);
						} else if (asgn_fd->GetClass() == XL::Class::FunctionOverload) {
							asgn = static_cast<XL::XFunctionOverload *>(asgn_fd.Inner());
						} else throw Exception();
						if (!asgn->GetImplementationDesc()._xw.IsEmpty()) standard = true;
					}
				} catch (...) { discard = true; }
			}
			if (discard) return;
			if (!standard) {
				ObjectArray<XL::LObject> fake_vft(1);
				XL::CreateDefaultImplementation(cls, flags, fake_vft);
				return;
			}
			int argc = 0;
			uint ff = XL::FunctionMethod | XL::FunctionThisCall;
			XL::LObject * func;
			if (flags & XL::CreateMethodAssign) {
				try {
					auto fd = ctx.CreateFunction(on_class, XL::OperatorAssign);
					func = ctx.CreateFunctionOverload(fd, class_ref, 1, class_ref.InnerRef(), ff);
				} catch (...) { return; }
				TranslationRules rules;
				rules.blocks.SetLength(3);
				rules.blocks[0].rule = Rule::InsertArgument;
				rules.blocks[1].rule = Rule::InsertString;
				rules.blocks[2].rule = Rule::InsertArgument;
				rules.blocks[0].index = 0;
				rules.blocks[1].index = 0;
				rules.blocks[2].index = 1;
				rules.blocks[1].text = L"=";
				auto & desc = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
				desc._is_xw = true;
				desc._xw.Append(ShaderLanguage::HLSL, rules);
				desc._xw.Append(ShaderLanguage::MSL, rules);
				desc._xw.Append(ShaderLanguage::GLSL, rules);
			} else {
				try {
					if (flags & XL::CreateMethodConstructorInit) {
						auto fd = ctx.CreateFunction(on_class, XL::NameConstructor);
						func = ctx.CreateFunctionOverload(fd, void_type, 0, 0, ff);
					} else if (flags & XL::CreateMethodConstructorCopy) {
						argc = 1;
						auto fd = ctx.CreateFunction(on_class, XL::NameConstructor);
						func = ctx.CreateFunctionOverload(fd, void_type, 1, class_ref.InnerRef(), ff);
					}
				} catch (...) { return; }
				TranslationRules rules;
				if (flags & XL::CreateMethodConstructorCopy) {
					rules.blocks.SetLength(1);
					rules.blocks[0].rule = Rule::InsertArgument;
					rules.blocks[0].index = 0;
				}
				auto & desc = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
				desc._is_xw = true;
				desc._xw.Append(ShaderLanguage::HLSL, rules);
				desc._xw.Append(ShaderLanguage::MSL, rules);
				desc._xw.Append(ShaderLanguage::GLSL, rules);
			}
		}
		void CreateDefaultImplementations(XL::LObject * on_class, uint flags)
		{
			if (flags & XL::CreateMethodConstructorInit) CreateDefaultImplementation(on_class, XL::CreateMethodConstructorInit);
			if (flags & XL::CreateMethodConstructorCopy) CreateDefaultImplementation(on_class, XL::CreateMethodConstructorCopy);
			if (flags & XL::CreateMethodAssign) CreateDefaultImplementation(on_class, XL::CreateMethodAssign);
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
		void AddArgumentSemantics(DynamicString & sword, const string & aname, const string & sname, int index) { sword << aname << L":" << sname << L"#" << string(index) << L";"; }
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
			if (tref.GetReferenceClass() == XI::Module::TypeReference::Class::Array) return false;
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
		void ReadFunctionInformation(const string & fcn, XI::Module::Function & func, FunctionDesc & desc)
		{
			int del = fcn.FindFirst(L':');
			desc.fname = fcn.Fragment(0, del);
			if (func.code_flags & XI::Module::Function::FunctionInstance) {
				int ord_begin = desc.fname.FindFirst(L"._@");
				int ord_end = desc.fname.FindLast(L".ordo.");
				if (ord_begin >= 0 && ord_end >= 0) {
					desc.instance_tcn = desc.fname.Fragment(ord_begin + 3, ord_end - ord_begin - 3);
					desc.constructor = desc.fname.FindLast(L".@32") <= ord_end;
				} else {
					int ord = desc.fname.FindLast(L".");
					desc.instance_tcn = XI::Module::TypeReference::MakeClassReference(desc.fname.Fragment(0, ord));
					desc.constructor = desc.fname.FindFirst(L"@crea") >= 0;
				}
			} else {
				desc.instance_tcn = L"";
				desc.constructor = false;
			}
			auto ftcn = fcn.Fragment(del + 1, -1);
			XI::Module::TypeReference ftype(ftcn);
			SafePointer< Array<XI::Module::TypeReference> > sign = ftype.GetFunctionSignature();
			string sdata;
			auto sdata_ptr = func.attributes[AttributeVertex];
			if (sdata_ptr) {
				sdata = *sdata_ptr;
				desc.fdes = FunctionDesignation::Vertex;
			} else {
				sdata_ptr = func.attributes[AttributePixel];
				if (sdata_ptr) {
					sdata = *sdata_ptr;
					desc.fdes = FunctionDesignation::Pixel;
				} else desc.fdes = FunctionDesignation::Service;
			}
			if (desc.instance_tcn.Length() && desc.constructor) {
				desc.rv.name = L"";
				desc.rv.tcn = desc.instance_tcn;
				desc.rv.inout = true;
				desc.rv.semantics = ArgumentSemantics::Undefined;
				desc.rv.index = 0;
			} else {
				desc.rv.name = L"";
				desc.rv.tcn = sign->ElementAt(0).QueryCanonicalName();
				desc.rv.inout = true;
				desc.rv.semantics = ArgumentSemantics::Undefined;
				desc.rv.index = 0;
			}
			desc.args = Array<ArgumentDesc>(0x30);
			auto sdata_parts = sdata.Split(L';');
			if (desc.instance_tcn.Length() && !desc.constructor) {
				ArgumentDesc arg;
				arg.name = L"ego";
				arg.tcn = desc.instance_tcn;
				arg.inout = true;
				arg.semantics = ArgumentSemantics::Undefined;
				arg.index = 0;
				desc.args << arg;
			}
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
				if (i < sdata_parts.Length() && desc.fdes != FunctionDesignation::Service) {
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
				desc.args << arg;
			}
			if (desc.fdes != FunctionDesignation::Service) {
				int counter = -1;
				for (auto & a : desc.args) {
					if (a.semantics == ArgumentSemantics::InterstageNI || a.semantics == ArgumentSemantics::InterstageIL || a.semantics == ArgumentSemantics::InterstageIP) {
						counter = max(counter, a.index);
					}
				}
				for (auto & a : desc.args) {
					if (a.semantics == ArgumentSemantics::InterstageNI || a.semantics == ArgumentSemantics::InterstageIL || a.semantics == ArgumentSemantics::InterstageIP) {
						if (a.index < 0) {
							counter++;
							a.index = counter;
						}
					}
				}
				counter = -1;
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Color) counter = max(counter, a.index);
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Color && a.index < 0) {
					counter++;
					a.index = counter;
				}
				counter = -1;
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Sampler) counter = max(counter, a.index);
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Sampler && a.index < 0) {
					counter++;
					a.index = counter;
				}
				counter = -1;
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Constant) counter = max(counter, a.index);
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Constant && a.index < 0) {
					counter++;
					a.index = counter;
				}
				// Counter is the last index assigned to a constant buffer now
				int tcounter = -1;
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Texture) tcounter = max(tcounter, a.index);
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Texture && a.index < 0) {
					tcounter++;
					a.index = tcounter;
				}
				// TCounter is the last index assigned to a texture now
				counter = max(counter, tcounter);
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Buffer) counter = max(counter, a.index);
				for (auto & a : desc.args) if (a.semantics == ArgumentSemantics::Buffer && a.index < 0) {
					counter++;
					a.index = counter;
				}
				for (auto & a : desc.args) if (a.index < 0) a.index = 0;
			}
		}
	}
}