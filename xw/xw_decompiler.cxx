#include "xw_decompiler.h"
#include "xw_lang_ext.h"
#include "../ximg/xi_resources.h"
#include "../xlang/xl_lal.h"
#include "../xlang/xl_types.h"
#include "../xasm/xa_type_helper.h"

namespace Engine
{
	namespace XW
	{
		void SetError(DecompilerStatusDesc & error, DecompilerStatus status, ShaderLanguage lang, const string & object, const string & addendum)
		{
			error.status = status;
			error.language = lang;
			error.object = object;
			error.addendum = addendum;
		}
		void SetError(DecompilerStatusDesc & error, DecompilerStatus status, ShaderLanguage lang, const string & object)
		{
			error.status = status;
			error.language = lang;
			error.object = object;
			error.addendum = L"";
		}
		void SetError(DecompilerStatusDesc & error, DecompilerStatus status, const string & object, const string & addendum)
		{
			error.status = status;
			error.language = ShaderLanguage::Unknown;
			error.object = object;
			error.addendum = addendum;
		}
		void SetError(DecompilerStatusDesc & error, DecompilerStatus status, const string & object)
		{
			error.status = status;
			error.language = ShaderLanguage::Unknown;
			error.object = object;
			error.addendum = L"";
		}
		void SetError(DecompilerStatusDesc & error, DecompilerStatus status, ShaderLanguage lang)
		{
			error.status = status;
			error.language = lang;
			error.object = error.addendum = L"";
		}
		void SetError(DecompilerStatusDesc & error, DecompilerStatus status)
		{
			error.status = status;
			error.language = ShaderLanguage::Unknown;
			error.object = error.addendum = L"";
		}

		class ListDecompilerCallback : public IDecompilerCallback
		{
			Array<string> _xmdl, _wmdl;
			SafePointer<IDecompilerCallback> _dropback;
		public:
			ListDecompilerCallback(const string * xmdl_pv, int xmdl_pc, const string * wmdl_pv, int wmdl_pc, IDecompilerCallback * dropback) : _xmdl(xmdl_pc), _wmdl(wmdl_pc)
			{
				_dropback.SetRetain(dropback);
				if (xmdl_pc) _xmdl.Append(xmdl_pv, xmdl_pc);
				if (wmdl_pc) _wmdl.Append(wmdl_pv, wmdl_pc);
			}
			virtual ~ListDecompilerCallback(void) override {}
			virtual Streaming::Stream * QueryXModuleFileStream(const string & module_name) override
			{
				for (auto & path : _xmdl) try {
					return new Streaming::FileStream(path + L"/" + module_name + L"." + XI::FileExtensionLibrary, Streaming::AccessRead, Streaming::OpenExisting);
				} catch (...) {}
				if (_dropback) return _dropback->QueryXModuleFileStream(module_name);
				throw IO::FileAccessException(IO::Error::FileNotFound);
			}
			virtual Streaming::Stream * QueryWModuleFileStream(const string & module_name) override
			{
				for (auto & path : _wmdl) try {
					return new Streaming::FileStream(path + L"/" + module_name + L"." + XI::FileExtensionGPLibrary, Streaming::AccessRead, Streaming::OpenExisting);
				} catch (...) {}
				if (_dropback) return _dropback->QueryWModuleFileStream(module_name);
				throw IO::FileAccessException(IO::Error::FileNotFound);
			}
			virtual string ToString(void) const override { return L"ListDecompilerCallback"; }
		};
		class OutputPortion : public IOutputPortion
		{
			string _pref, _ext;
			SafePointer<Streaming::Stream> _stream;
		public:
			OutputPortion(const string & pref, const string & ext, Streaming::Stream * data) : _pref(pref), _ext(ext) { _stream.SetRetain(data); }
			virtual ~OutputPortion(void) override {}
			virtual string GetPortionPostfix(void) const override { return _pref; }
			virtual string GetPortionExtension(void) const override { return _ext; }
			virtual Streaming::Stream * GetPortionData(void) const override { return _stream; }
			virtual string ToString(void) const override { return L"OutputPortion"; }
		};
		
		struct AssemblyDesc
		{
			string rootname;
			Volumes::Dictionary<string, XI::AssemblyVersionInformation> modules;
			Volumes::Dictionary<string, XI::Module::Literal> literals;
			Volumes::Dictionary<string, XI::Module::Class> classes;
			Volumes::Dictionary<string, XI::Module::Function> functions;
			Volumes::ObjectDictionary<uint64, DataBlock> resources;
			XI::AssemblyVersionInformation versioninfo;
		};
		struct Artifact : public Object
		{
			uint designation;
			FunctionDesignation type;
			string entry_point_effective, entry_point_nominal;
			SafePointer<DataBlock> data;
		};
		struct SymbolArtifact : public Object
		{
			uint symbol_class; // 0 - type, 1 - function, 2 - constructor, 3 - insertion
			uint class_alignment, class_size;
			bool private_class;
			string symbol_xw, symbol_refer, symbol_refer_publically, code_inject;
			XW::TranslationRules rules;
			FunctionDesc fdesc;
			Volumes::Dictionary<int, Volumes::KeyValuePair<string, string> > fields; // XO offset -> refer, tcn
		};
		struct FunctionSynthesisInformation
		{
			FunctionDesignation fdes;
			string nominal_name, effective_name;
		};
		struct DecompilerContext
		{
			class IStructureGenerator
			{
			public:
				virtual ShaderLanguage GetLanguage(void) = 0;
				virtual bool OpenClass(DecompilerContext & ctx, XI::Module::Class & cls, const string & vname) = 0;
				virtual bool CreateField(DecompilerContext & ctx, XI::Module::Variable & fld, const string & vname) = 0;
				virtual bool EndClass(DecompilerContext & ctx) = 0;
			};
			class IFunctionGenerator
			{
			public:
				virtual bool IsHumanReadable(void) const noexcept = 0;
				virtual ShaderLanguage GetLanguage(void) const noexcept = 0;
				virtual bool ProcessFunction(DecompilerContext & ctx, XI::Module::Function & func, const string & vname, FunctionDesc & fdesc, XA::Function & xasm, string & effective_name) = 0;
			};
			class FunctionLoader : public IFunctionLoader
			{
				int _state;
				ShaderLanguage _lang;
				XA::Function _xasm;
				XW::TranslationRules _rules;
			public:
				FunctionLoader(ShaderLanguage lang) : _state(0), _lang(lang) {}
				virtual ShaderLanguage GetLanguage(void) noexcept override { return _lang; }
				virtual void HandleFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override { try { _xasm.Load(fout); _state = 1; } catch (...) { _state = 3; } }
				virtual void HandleRule(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override { try { _rules.Load(fout); _state = 2; } catch (...) { _state = 3; } }
				virtual void HandleLoadError(const string & symbol, const XI::Module::Function & fin, XI::LoadFunctionError error) noexcept override { _state = 3; }
				bool IsXA(void) const noexcept { return _state == 1; }
				bool IsXW(void) const noexcept { return _state == 2; }
				bool IsError(void) const noexcept { return _state == 3 || _state == 0; }
				XA::Function & GetXA(void) noexcept { return _xasm; }
				XW::TranslationRules & GetXW(void) noexcept { return _rules; }
			};

			DecompilerStatusDesc & status;
			AssemblyDesc & adata;
			ObjectArray<Artifact> & dest;
			ObjectArray<SymbolArtifact> symbol_cache;
			Volumes::Dictionary<string, int> symbol_map;

			DecompilerContext(DecompilerStatusDesc & st, AssemblyDesc & asmdata, ObjectArray<Artifact> & dst) : status(st), adata(asmdata), dest(dst), symbol_cache(0x400) {}
			bool ProcessStructure(const string & clsname, IStructureGenerator & hdlr)
			{
				if (symbol_map.ElementExists(clsname)) return true;
				auto cls = adata.classes[clsname];
				if (!cls) {
					SetError(status, DecompilerStatus::SymbolNotFound, hdlr.GetLanguage(), clsname);
					return false;
				}
				if (!hdlr.OpenClass(*this, *cls, clsname)) return false;
				typedef Volumes::KeyValuePair<string, XI::Module::Variable> fdata;
				Array<fdata> orded(0x40);
				for (auto & f : cls->fields) orded << f;
				SortArray(orded, [](const fdata & a, const fdata & b) -> int {
					int la = a.value.offset.num_words * 4 + a.value.offset.num_bytes;
					int lb = b.value.offset.num_words * 4 + b.value.offset.num_bytes;
					return la - lb;
				});
				for (auto & f : orded) if (!hdlr.CreateField(*this, f.value, f.key)) return false;
				if (!hdlr.EndClass(*this)) return false;
				return true;
			}
			bool ProcessStructures(IStructureGenerator & hdlr)
			{
				for (auto & cls : adata.classes) {
					if (cls.key[0] == L'_' && cls.key[1] == L'@') continue;
					if (!ProcessStructure(cls.key, hdlr)) return false;
				}
				return true;
			}
			bool ProcessFunction(const string & funcname, IFunctionGenerator & hdlr, FunctionSynthesisInformation * fsi = 0)
			{
				if (symbol_map.ElementExists(funcname)) return true;
				auto func = adata.functions[funcname];
				if (!func) {
					int del = funcname.Fragment(0, funcname.FindFirst(L':')).FindLast(L'.');
					auto cls = adata.classes[funcname.Fragment(0, del)];
					if (!cls) {
						SetError(status, DecompilerStatus::SymbolNotFound, hdlr.GetLanguage(), funcname);
						return false;
					}
					func = cls->methods[funcname.Fragment(del + 1, -1)];
					if (!func) {
						SetError(status, DecompilerStatus::SymbolNotFound, hdlr.GetLanguage(), funcname);
						return false;
					}
				}
				FunctionDesc fdesc;
				FunctionLoader fldr(hdlr.GetLanguage());
				ReadFunctionInformation(funcname, *func, fdesc);
				LoadFunction(funcname, *func, &fldr);
				if (fldr.IsError() && hdlr.GetLanguage() != ShaderLanguage::Unknown) {
					SetError(status, DecompilerStatus::NotSupported, hdlr.GetLanguage(), funcname);
					return false;
				}
				if (fsi) {
					fsi->fdes = fdesc.fdes;
					fsi->nominal_name = fdesc.fname;
					fsi->effective_name = L"";
				}
				if (fldr.IsXW()) {
					SafePointer<SymbolArtifact> art = new SymbolArtifact;
					if (fdesc.constructor) art->symbol_class = 2; else art->symbol_class = 1;
					art->class_alignment = art->class_size = 0;
					art->symbol_xw = funcname;
					art->rules = fldr.GetXW();
					art->fdesc = fdesc;
					AddArtifact(art);
				}
				if (fldr.IsXA()) {
					auto & xa = fldr.GetXA();
					for (auto & eref : xa.extrefs) {
						if (eref[0] == L'S' && eref[1] == L':') {
							auto smbl = eref.Fragment(2, -1);
							if (symbol_map.ElementExists(smbl)) continue;
							auto lit = adata.literals[smbl];
							if (lit) {
								SafePointer<SymbolArtifact> art = new SymbolArtifact;
								art->symbol_class = 3;
								art->class_alignment = art->class_size = 0;
								art->private_class = true;
								art->symbol_xw = smbl;
								art->rules.blocks.SetLength(1);
								art->rules.blocks[0].rule = Rule::InsertString;
								if (lit->contents == XI::Module::Literal::Class::Boolean) {
									art->rules.blocks[0].text = lit->data_boolean ? L"true" : L"false";
								} else if (lit->contents == XI::Module::Literal::Class::UnsignedInteger) {
									art->rules.blocks[0].text = lit->data_uint32;
								} else if (lit->contents == XI::Module::Literal::Class::SignedInteger) {
									art->rules.blocks[0].text = lit->data_sint32;
								} else if (lit->contents == XI::Module::Literal::Class::FloatingPoint) {
									if (isfinite(lit->data_float)) {
										art->rules.blocks[0].text = string(lit->data_float);
										if (art->rules.blocks[0].text.FindFirst(L'.') < 0) art->rules.blocks[0].text += L".0";
										art->rules.blocks[0].text += string(L"f");
									} else art->rules.blocks[0].text = L"0.0f";
								} else {
									SetError(status, DecompilerStatus::NotSupported, hdlr.GetLanguage(), funcname, eref);
									return false;
								}
								AddArtifact(art);
							} else if (!ProcessFunction(smbl, hdlr)) return false;
						} else {
							SetError(status, DecompilerStatus::SymbolNotFound, hdlr.GetLanguage(), funcname, eref);
							return false;
						}
					}
					string fxn;
					if (!hdlr.ProcessFunction(*this, *func, funcname, fdesc, xa, fxn)) return false;
					if (fsi) fsi->effective_name = fxn;
				}
				return true;
			}
			bool ProcessFunctions(IFunctionGenerator & hdlr, bool separately_per_shader, ObjectArray<Artifact> & artifacts)
			{
				for (auto & func : adata.functions) {
					if (func.value.attributes.ElementExists(AttributeVertex) || func.value.attributes.ElementExists(AttributePixel)) {
						ObjectArray<SymbolArtifact> revert_symbol_cache;
						Volumes::Dictionary<string, int> revert_symbol_map;
						if (separately_per_shader) {
							revert_symbol_cache = symbol_cache;
							revert_symbol_map = symbol_map;
						}
						FunctionSynthesisInformation fsi;
						if (!ProcessFunction(func.key, hdlr, &fsi)) return false;
						if (separately_per_shader) {
							DynamicString output;
							if (hdlr.IsHumanReadable()) MakeMachineGeneratedDisclamer(output);
							for (auto & a : symbol_cache) output << a.code_inject;
							SafePointer<Artifact> art = new Artifact;
							if (hdlr.GetLanguage() == ShaderLanguage::HLSL) art->designation = DecompilerFlagProduceHLSL;
							else if (hdlr.GetLanguage() == ShaderLanguage::MSL) art->designation = DecompilerFlagProduceMSL;
							else if (hdlr.GetLanguage() == ShaderLanguage::GLSL) art->designation = DecompilerFlagProduceGLSL;
							else art->designation = 0;
							art->type = fsi.fdes;
							art->data = output.ToString().EncodeSequence(Encoding::ANSI, false);
							art->entry_point_nominal = fsi.nominal_name;
							art->entry_point_effective = fsi.effective_name;
							artifacts.Append(art);
							symbol_cache = revert_symbol_cache;
							symbol_map = revert_symbol_map;
						}
					}
				}
				return true;
			}
			bool ValidateSemantics(const FunctionDesc & fdesc)
			{
				if (fdesc.fdes != FunctionDesignation::Vertex && fdesc.fdes != FunctionDesignation::Pixel) return true;
				bool position_out = false;
				bool color_out = false;
				for (int i = 0; i < fdesc.args.Length(); i++) {
					auto & a1 = fdesc.args[i];
					auto a1s = a1.semantics;
					if (a1s == ArgumentSemantics::InterstageIL || a1s == ArgumentSemantics::InterstageIP) a1s = ArgumentSemantics::InterstageNI;
					if (a1.semantics == ArgumentSemantics::Undefined) {
						SetError(status, DecompilerStatus::InvalidSemantics, fdesc.fname, a1.name);
						return false;
					}
					for (int j = 0; j < i; j++) {
						auto & a2 = fdesc.args[j];
						auto a2s = a2.semantics;
						if (a2s == ArgumentSemantics::InterstageIL || a2s == ArgumentSemantics::InterstageIP) a2s = ArgumentSemantics::InterstageNI;
						if (a2s == a1s && a2.index == a1.index) {
							SetError(status, DecompilerStatus::InvalidSemantics, fdesc.fname, a1.name + L", " + a2.name);
							return false;
						}
						if (a1s == ArgumentSemantics::SecondColor && a2s == ArgumentSemantics::Color && a2.index) {
							SetError(status, DecompilerStatus::InvalidSemantics, fdesc.fname, a1.name + L", " + a2.name);
							return false;
						}
						if (a2s == ArgumentSemantics::SecondColor && a1s == ArgumentSemantics::Color && a1.index) {
							SetError(status, DecompilerStatus::InvalidSemantics, fdesc.fname, a1.name + L", " + a2.name);
							return false;
						}
					}
					bool bad = false;
					if (a1s == ArgumentSemantics::VertexIndex || a1s == ArgumentSemantics::InstanceIndex) {
						if (a1.tcn != L"Cnint32" || a1.inout || a1.index || fdesc.fdes != FunctionDesignation::Vertex) bad = true;
					} else if (a1s == ArgumentSemantics::Position) {
						if (a1.tcn != L"Cfrac4" || a1.index) bad = true;
						if (fdesc.fdes == FunctionDesignation::Vertex && !a1.inout) bad = true;
						if (fdesc.fdes == FunctionDesignation::Pixel && a1.inout) bad = true;
						if (fdesc.fdes == FunctionDesignation::Vertex) position_out = true;
					} else if (a1s == ArgumentSemantics::InterstageNI) {
						if (a1.tcn != L"Cfrac" && a1.tcn != L"Cfrac2" && a1.tcn != L"Cfrac3" && a1.tcn != L"Cfrac4") bad = true;
						if (a1.index >= SelectorLimitInterstage) bad = true;
						if (fdesc.fdes == FunctionDesignation::Vertex && !a1.inout) bad = true;
						if (fdesc.fdes == FunctionDesignation::Pixel && a1.inout) bad = true;
					} else if (a1s == ArgumentSemantics::IsFrontFacing) {
						if (a1.tcn != L"Clogicum" || a1.inout || a1.index || fdesc.fdes != FunctionDesignation::Pixel) bad = true;
					} else if (a1s == ArgumentSemantics::Color) {
						if (a1.tcn != L"Cfrac2" && a1.tcn != L"Cfrac3" && a1.tcn != L"Cfrac4") bad = true;
						if (a1.index >= SelectorLimitRenderTarget) bad = true;
						if (fdesc.fdes != FunctionDesignation::Pixel || !a1.inout) bad = true;
						if (a1.index == 0) color_out = true;
					} else if (a1s == ArgumentSemantics::SecondColor) {
						if (a1.tcn != L"Cfrac2" && a1.tcn != L"Cfrac3" && a1.tcn != L"Cfrac4") bad = true;
						if (fdesc.fdes != FunctionDesignation::Pixel || !a1.inout || a1.index) bad = true;
					} else if (a1s == ArgumentSemantics::Depth) {
						if (a1.tcn != L"Cfrac") bad = true;
						if (fdesc.fdes != FunctionDesignation::Pixel || !a1.inout || a1.index) bad = true;
					} else if (a1s == ArgumentSemantics::Stencil) {
						if (a1.tcn != L"Cnint32") bad = true;
						if (fdesc.fdes != FunctionDesignation::Pixel || !a1.inout || a1.index) bad = true;
					} else if (a1s == ArgumentSemantics::Constant) {
						try {
							auto index = symbol_map[XI::Module::TypeReference(a1.tcn).GetClassName()];
							if (!index) throw InvalidStateException();
							auto & cls = symbol_cache[*index];
							if (cls.private_class) bad = true;
						} catch (...) { bad = true; }
						if (a1.index >= SelectorLimitConstantBuffer || a1.inout) bad = true;
					} else if (a1s == ArgumentSemantics::Buffer) {
						try {
							auto cname = XI::Module::TypeReference(a1.tcn).GetClassName();
							if (cname.Fragment(0, 12) != L"@praeformae.") throw InvalidArgumentException();
							int i = 12, l = 0;
							while (i < cname.Length()) {
								if (cname[i] == L'(') l++;
								else if (cname[i] == L')') l--;
								else if (cname[i] == L'.' && l == 0) break;
								i++;
							}
							auto pcn = cname.Fragment(12, i - 12);
							auto ref = XI::Module::TypeReference(pcn);
							auto base = ref.GetAbstractInstanceBase().GetClassName();
							if (base != L"series") bad = true;
						} catch (...) { bad = true; }
						if (a1.index >= SelectorLimitBuffer || a1.inout) bad = true;
					} else if (a1s == ArgumentSemantics::Texture) {
						try {
							auto cname = XI::Module::TypeReference(a1.tcn).GetClassName();
							if (cname.Fragment(0, 12) != L"@praeformae.") throw InvalidArgumentException();
							int i = 12, l = 0;
							while (i < cname.Length()) {
								if (cname[i] == L'(') l++;
								else if (cname[i] == L')') l--;
								else if (cname[i] == L'.' && l == 0) break;
								i++;
							}
							auto pcn = cname.Fragment(12, i - 12);
							auto ref = XI::Module::TypeReference(pcn);
							auto base = ref.GetAbstractInstanceBase().GetClassName();
							if (base != L"textura_1d" && base != L"textura_2d" && base != L"textura_cubica" && base != L"textura_3d" &&
								base != L"ordo_texturarum_1d" && base != L"ordo_texturarum_2d" && base != L"ordo_texturarum_cubicarum") bad = true;
						} catch (...) { bad = true; }
						if (a1.index >= SelectorLimitTexture || a1.inout) bad = true;
					} else if (a1s == ArgumentSemantics::Sampler) {
						if (a1.tcn != L"Cexceptor") bad = true;
						if (a1.index >= SelectorLimitSampler || a1.inout) bad = true;
					} else bad = true;
					if (bad) {
						SetError(status, DecompilerStatus::InvalidSemantics, fdesc.fname, a1.name);
						return false;
					}
				}
				if (fdesc.fdes == FunctionDesignation::Vertex && !position_out) {
					SetError(status, DecompilerStatus::NoMandatorySemantics, fdesc.fname, SemanticPosition);
					return false;
				}
				if (fdesc.fdes == FunctionDesignation::Pixel && !color_out) {
					SetError(status, DecompilerStatus::NoMandatorySemantics, fdesc.fname, SemanticColor);
					return false;
				}
				return true;
			}
			void MakeMachineGeneratedDisclamer(DynamicString & output)
			{
				SafePointer< Volumes::Dictionary<string, string> > mdata = XI::LoadModuleMetadata(adata.resources);
				output << L"// HAEC PROGRAMMA AB XW DECOMPILATORE PRODUCTA EST\n";
				auto value = mdata->GetElementByKey(XI::MetadataKeyModuleName);
				if (value) output << L"// MODULUS ORIGINALIS " << value->UpperCase() << L"\n";
				else output << L"// MODULUS ORIGINALIS " << adata.rootname.UpperCase() << L"\n";
				value = mdata->GetElementByKey(XI::MetadataKeyCompanyName);
				if (value) output << L"// AB \'" << value->UpperCase() << L"\' CREATUS EST\n";
				value = mdata->GetElementByKey(XI::MetadataKeyCopyright);
				if (value) output << L"// " << value->UpperCase().Replace(L'©', L"(C)") << L"\n";
				value = mdata->GetElementByKey(XI::MetadataKeyVersion);
				if (value) output << L"// VERSIO " << value->UpperCase() << L"\n";
				output << L"\n";
			}
			void AddArtifact(SymbolArtifact * art)
			{
				auto index = symbol_cache.Length();
				symbol_cache.Append(art);
				symbol_map.Append(art->symbol_xw, index);
			}
		};
		int AddressAlign(int address, int align) { return (address + align - 1) / align * align; }
		int AddressAlign(const XA::ObjectSize & address, int align) { return AddressAlign(address.num_words * 4 + address.num_bytes, align); }
		void DecomposeTCN(const string & tcn, string & vclsname, Array<int> & alvls, bool & byref)
		{
			byref = false;
			XI::Module::TypeReference ref(tcn);
			if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
				byref = true;
				auto rs = ref.GetReferenceDestination();
				MemoryCopy(&ref, &rs, sizeof(rs));
			}
			while (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Array) {
				alvls << ref.GetArrayVolume();
				auto rs = ref.GetArrayElement();
				MemoryCopy(&ref, &rs, sizeof(rs));
			}
			if (ref.GetReferenceClass() != XI::Module::TypeReference::Class::Class) throw InvalidStateException();
			vclsname = ref.GetClassName();
		}
		bool IsCIdentifier(const string & text)
		{
			if (text[0] != L'_' && (text[0] < L'A' || text[0] > L'Z') && (text[0] < L'a' || text[0] > L'z')) return false;
			for (int i = 1; i < text.Length(); i++) if (text[i] != L'_' && (text[i] < L'A' || text[i] > L'Z') && (text[i] < L'a' || text[i] > L'z') && (text[i] < L'0' || text[i] > L'9')) return false;
			return true;
		}
		bool LoadModuleW(AssemblyDesc & init, DecompileDesc & desc, const string & name);
		bool LoadModuleW(AssemblyDesc & init, DecompileDesc & desc, const string & name, Streaming::Stream * data, bool with_rsrc)
		{
			SafePointer<XI::Module> mdl;
			XI::AssemblyVersionInformation vinfo;
			try {
				mdl = new XI::Module(data, XI::Module::ModuleLoadFlags::LoadAll);
				if ((name.Length() && mdl->module_import_name != name) || mdl->subsystem != XI::Module::ExecutionSubsystem::XW) throw InvalidFormatException();
				if (!XI::LoadModuleVersionInformation(mdl->resources, vinfo)) vinfo.ThisModuleVersion = 0xFFFFFFFF;
			} catch (...) {
				SetError(desc.status, DecompilerStatus::InvalidImage, name);
				return false;
			}
			for (auto & dep : mdl->modules_depends_on) if (!LoadModuleW(init, desc, dep)) return false;
			for (auto & vreq : vinfo.ModuleVersionsNeeded) {
				auto vi = init.modules[vreq.key];
				if (!vi) {
					SetError(desc.status, DecompilerStatus::InvalidImage, mdl->module_import_name);
					return false;
				}
				if (vi->ThisModuleVersion == 0xFFFFFFFF) {
					SetError(desc.status, DecompilerStatus::WModuleIncompatible, mdl->module_import_name);
					return false;
				}
				bool vok = false;
				if (vi->ThisModuleVersion == vreq.value) vok = true;
				if (!vok) for (auto & vs : vi->ReplacesVersions) if ((vreq.value & vs.Mask) == vs.MustBe) { vok = true; break; }
				if (!vok) {
					SetError(desc.status, DecompilerStatus::WModuleIncompatible, mdl->module_import_name);
					return false;
				}
			}
			init.modules.Append(mdl->module_import_name, vinfo);
			for (auto & o : mdl->literals) init.literals.Append(o.key, o.value);
			for (auto & o : mdl->classes) init.classes.Append(o.key, o.value);
			for (auto & o : mdl->functions) init.functions.Append(o.key, o.value);
			if (with_rsrc) for (auto & o : mdl->resources) init.resources.Append(o.key, o.value);
			if (with_rsrc) init.versioninfo = vinfo;
			if (with_rsrc) init.rootname = mdl->module_import_name;
			return true;
		}
		bool LoadModuleW(AssemblyDesc & init, DecompileDesc & desc, const string & name)
		{
			SafePointer<Streaming::Stream> input;
			try { input = desc.callback->QueryWModuleFileStream(name); } catch (...) {
				SetError(desc.status, DecompilerStatus::WModuleNotFound, name);
				return false;
			}
			return LoadModuleW(init, desc, name, input, false);
		}
		bool LoadAssembly(AssemblyDesc & init, DecompileDesc & desc)
		{
			SafePointer<Streaming::MemoryStream> main = new Streaming::MemoryStream(desc.root_module->GetBuffer(), desc.root_module->Length());
			return LoadModuleW(init, desc, L"", main, true);
		}
		class TextLanguageStructureGenerator : public DecompilerContext::IStructureGenerator
		{
			struct _dynamic_state : public Object
			{
			public:
				uint _field_counter;
				uint _num_namespaces;
				bool _public;
				SafePointer<SymbolArtifact> _current;
				DynamicString _body;
			};
		private:
			uint _proc_mask;
			bool _human_readable;
			uint _name_counter;
			SafePointer<_dynamic_state> _state;
		private:
			void _align_to_boundary(int align)
			{
				if (!_state->_public) return;
				if (align % 4) throw InvalidStateException();
				if (_state->_current->class_size % 4) throw InvalidStateException();
				if (_state->_current->class_size % align) {
					int rem = (align - _state->_current->class_size % align) / 4;
					if (_human_readable) _state->_body << string(L'\t', _state->_num_namespaces + 1);
					string fname = L"__politio" + string(_state->_field_counter, HexadecimalBase, 6);
					_state->_body << L"float " << fname << L"[" << string(rem) << "]" << L";";
					if (_human_readable) _state->_body << L"\n";
					_state->_current->class_size += rem * 4;
					_state->_field_counter++;
				}
			}
		public:
			TextLanguageStructureGenerator(uint proc_mask, bool human_readable) : _proc_mask(proc_mask), _human_readable(human_readable), _name_counter(0) {}
			virtual ShaderLanguage GetLanguage(void) override
			{
				if (_proc_mask == DecompilerFlagProduceHLSL) return ShaderLanguage::HLSL;
				else if (_proc_mask == DecompilerFlagProduceMSL) return ShaderLanguage::MSL;
				else if (_proc_mask == DecompilerFlagProduceGLSL) return ShaderLanguage::GLSL;
				else return ShaderLanguage::Unknown;
			}
			virtual bool OpenClass(DecompilerContext & ctx, XI::Module::Class & cls, const string & vname) override
			{
				_state = new _dynamic_state;
				_state->_field_counter = 0;
				_state->_current = new SymbolArtifact;
				_state->_current->symbol_class = 0;
				_state->_current->class_alignment = _state->_current->class_size = 0;
				_state->_current->symbol_xw = vname;
				bool prvt = cls.attributes.ElementExists(AttributePrivate);
				_state->_public = !prvt;
				_state->_current->private_class = prvt;
				string alias_attr;
				if (_proc_mask == DecompilerFlagProduceHLSL) alias_attr = AttributeMapHLSL;
				else if (_proc_mask == DecompilerFlagProduceMSL) alias_attr = AttributeMapMSL;
				else if (_proc_mask == DecompilerFlagProduceGLSL) alias_attr = AttributeMapGLSL;
				else if (_proc_mask == DecompilerFlagProduceCXX) alias_attr = AttributeMapCXX;
				auto alias = alias_attr.Length() ? cls.attributes[alias_attr] : 0;
				auto align = cls.attributes[AttributeAlignment];
				if (align) _state->_current->class_alignment = align->ToInt32();
				if (_proc_mask == DecompilerFlagProduceCXX && prvt) {
					_state.SetReference(0);
					return true;
				}
				if (alias) {
					_state->_current->symbol_refer = *alias;
					if (vname.Fragment(0, 12) == L"@praeformae.") {
						int i = 12, l = 0;
						while (i < vname.Length()) {
							if (vname[i] == L'(') l++;
							else if (vname[i] == L')') l--;
							else if (vname[i] == L'.' && l == 0) break;
							i++;
						}
						auto pcn = vname.Fragment(12, i - 12);
						auto ref = XI::Module::TypeReference(pcn);
						auto arg = ref.GetAbstractInstanceParameterType().GetClassName();
						auto state = _state;
						if (!ctx.ProcessStructure(arg, *this)) return false;
						_state = state;
						auto index = ctx.symbol_map[arg];
						if (!index) { _state.SetReference(0); return true; }
						auto & tdata = ctx.symbol_cache[*index];
						if (tdata.symbol_class != 0) throw InvalidStateException();
						if (_state->_current->symbol_refer.FindFirst(L"$!") >= 0 && tdata.private_class) {
							SetError(ctx.status, DecompilerStatus::NotSupported, GetLanguage(), vname, arg);
							return false;
						}
						_state->_current->symbol_refer = _state->_current->symbol_refer.Replace(L"$E", tdata.symbol_refer_publically);
						_state->_current->symbol_refer = _state->_current->symbol_refer.Replace(L"$I", tdata.symbol_refer);
						_state->_current->symbol_refer = _state->_current->symbol_refer.Replace(L"$!", L"");
					}
					int index = _state->_current->symbol_refer.FindFirst(L'|');
					if (index >= 0) {
						_state->_current->symbol_refer_publically = _state->_current->symbol_refer.Fragment(index + 1, -1);
						_state->_current->symbol_refer = _state->_current->symbol_refer.Fragment(0, index);
					} else _state->_current->symbol_refer_publically = _state->_current->symbol_refer;
					if (!_state->_current->class_alignment) _state->_current->class_alignment = 4;
					_state->_current->class_size = AddressAlign(cls.instance_spec.size, _state->_current->class_alignment);
					ctx.AddArtifact(_state->_current);
					_state.SetReference(0);
					return true;
				} else {
					if (_proc_mask == DecompilerFlagProduceCXX) {
						auto way = vname.Split(L'.');
						_state->_num_namespaces = way.Length() - 1;
						for (auto & w : way) if (!IsCIdentifier(w)) {
							_state.SetReference(0);
							return true;
						}
						for (int i = 0; i < way.Length() - 1; i++) {
							if (_human_readable && i) _state->_body << string(L'\t', i);
							_state->_body << L"namespace " << way[i];
							if (_human_readable) _state->_body << L" ";
							_state->_body << L"{";
							if (_human_readable) _state->_body << L"\n";
							_state->_current->symbol_refer += L"::" + way[i];
						}
						_state->_current->symbol_refer += L"::" + way.LastElement();
						if (_human_readable && _state->_num_namespaces) _state->_body << string(L'\t', _state->_num_namespaces);
						_state->_body << L"struct " << way.LastElement();
					} else {
						_state->_current->symbol_refer = L"struct" + string(_name_counter, HexadecimalBase, 6);
						_name_counter++;
						_state->_num_namespaces = 0;
						_state->_body << L"struct " << _state->_current->symbol_refer;
					}
					if (_human_readable) _state->_body << L" ";
					_state->_body << L"{";
					if (_human_readable) _state->_body << L"\n";
					_state->_current->symbol_refer_publically = _state->_current->symbol_refer;
					return true;
				}
			}
			virtual bool CreateField(DecompilerContext & ctx, XI::Module::Variable & fld, const string & vname) override
			{
				if (_state && _state->_current) {
					bool byref;
					string vcls;
					Array<int> alvl(0x10);
					DecomposeTCN(fld.type_canonical_name, vcls, alvl, byref);
					auto state = _state;
					if (!ctx.ProcessStructure(vcls, *this)) return false;
					_state = state;
					auto index = ctx.symbol_map[vcls];
					if (!index) { _state->_body.Clear(); _state->_current.SetReference(0); return true; }
					auto & tdata = ctx.symbol_cache[*index];
					if (tdata.symbol_class != 0) throw InvalidStateException();
					_align_to_boundary(tdata.class_alignment);
					if (tdata.class_alignment > _state->_current->class_alignment) _state->_current->class_alignment = tdata.class_alignment;
					int volume = 1;
					for (auto & v : alvl) volume *= v;
					_state->_current->class_size += tdata.class_size * volume;
					if (_human_readable) _state->_body << string(L'\t', _state->_num_namespaces + 1);
					string fname;
					if (_proc_mask == DecompilerFlagProduceCXX) {
						if (IsCIdentifier(vname)) fname = vname;
						else fname = L"__valor" + string(_state->_field_counter, HexadecimalBase, 6);
					} else {
						fname = L"valor" + string(_state->_field_counter, HexadecimalBase, 6);
					}
					_state->_body << (_state->_public ? tdata.symbol_refer_publically : tdata.symbol_refer) << L" " << fname;
					for (auto & v : alvl) _state->_body << L"[" << string(v) << L"]";
					_state->_body << L";";
					if (_human_readable) _state->_body << L"\n";
					_state->_current->fields.Append(fld.offset.num_words * 4 + fld.offset.num_bytes, Volumes::KeyValuePair<string, string>(fname, fld.type_canonical_name));
					_state->_field_counter++;
				}
				return true;
			}
			virtual bool EndClass(DecompilerContext & ctx) override
			{
				if (_state && _state->_current) {
					if (!_state->_current->class_alignment) _state->_current->class_alignment = 4;
					_align_to_boundary(_state->_current->class_alignment);
					if (_human_readable && _state->_num_namespaces) _state->_body << string(L"\t", _state->_num_namespaces);
					_state->_body << L"};";
					if (_human_readable) _state->_body << L"\n";
					if (_state->_num_namespaces) for (int i = _state->_num_namespaces; i > 0; i--) {
						if (_human_readable) _state->_body << string(L"\t", i - 1);
						_state->_body << L"}";
						if (_human_readable) _state->_body << L"\n";
					}
					_state->_current->code_inject = _state->_body.ToString();
					_state->_body.Clear();
					ctx.AddArtifact(_state->_current);
				}
				_state.SetReference(0);
				return true;
			}
		};
		class ShaderLanguageFunctionGenerator : public DecompilerContext::IFunctionGenerator
		{
			class FunctionGenerator
			{
				struct expression_node
				{
					uint hint; // 1 - non-trivial, 2 - init object, 4 - retval object, 8 - lvalue
					string clsname;
					string code;
					expression_node(void) {}
					expression_node(const string & cdx, const string & cls, uint h = 1) : hint(h), clsname(cls), code(cdx) {}
					expression_node(const string & cdx, const XI::Module::TypeReference & tref, uint h = 1) : hint(h), code(cdx)
					{
						if (tref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
							clsname = tref.GetReferenceDestination().GetClassName();
						} else clsname = tref.GetClassName();
					}
				};
				static uint32 RegisterID(uint regcls, uint regindex) noexcept { return regcls | (regindex << 8); }
				static void ReadRegisterID(uint32 key, uint & regcls, uint & regindex) noexcept { regcls = key & 0xFF; regindex = key >> 8; }
			private:
				ShaderLanguageFunctionGenerator & _gen;
				uint _variable_counter, _temp_counter, _local_register;
				string _return_statement;
				expression_node _split_node;
				Volumes::Dictionary<uint32, expression_node> _register_mapping;
				DynamicString _output;
			private:
				string _get_type_name(DecompilerContext & ctx, const string & tcn)
				{
					auto index = ctx.symbol_map[XI::Module::TypeReference(tcn).GetClassName()];
					if (!index) throw InvalidStateException();
					auto & cls = ctx.symbol_cache[*index];
					return cls.symbol_refer;
				}
				bool _hr(void) const noexcept { return _gen._human_readable; }
				void _traits_for_semantic(ArgumentDesc & desc, string & prefix, string & postfix)
				{
					if (_gen.GetLanguage() == ShaderLanguage::HLSL) {
						if (desc.inout) prefix = L"out "; else prefix = L"in ";
						if (desc.semantics == ArgumentSemantics::VertexIndex) {
							postfix = L" : SV_VertexID";
						} else if (desc.semantics == ArgumentSemantics::InstanceIndex) {
							postfix = L" : SV_InstanceID";
						} else if (desc.semantics == ArgumentSemantics::Position) {
							postfix = L" : SV_Position";
						} else if (desc.semantics == ArgumentSemantics::InterstageNI) {
							prefix += L"nointerpolation ";
							if (desc.index < 8) postfix = L" : TEXCOORD" + string(desc.index);
							else postfix = L" : COLOR" + string(desc.index);
						} else if (desc.semantics == ArgumentSemantics::InterstageIL) {
							prefix += L"noperspective ";
							if (desc.index < 8) postfix = L" : TEXCOORD" + string(desc.index);
							else postfix = L" : COLOR" + string(desc.index);
						} else if (desc.semantics == ArgumentSemantics::InterstageIP) {
							prefix += L"linear ";
							if (desc.index < 8) postfix = L" : TEXCOORD" + string(desc.index);
							else postfix = L" : COLOR" + string(desc.index);
						} else if (desc.semantics == ArgumentSemantics::IsFrontFacing) {
							postfix = L" : SV_IsFrontFace";
						} else if (desc.semantics == ArgumentSemantics::Color) {
							postfix = L" : SV_Target" + string(desc.index);
						} else if (desc.semantics == ArgumentSemantics::SecondColor) {
							postfix = L" : SV_Target1";
						} else if (desc.semantics == ArgumentSemantics::Depth) {
							postfix = L" : SV_Depth";
						} else if (desc.semantics == ArgumentSemantics::Stencil) {
							postfix = L" : SV_StencilRef";
						}
					} else if (_gen.GetLanguage() == ShaderLanguage::MSL) {
						if (desc.semantics == ArgumentSemantics::VertexIndex) {
							postfix = L" [[vertex_id]]";
						} else if (desc.semantics == ArgumentSemantics::InstanceIndex) {
							postfix = L" [[instance_id]]";
						} else if (desc.semantics == ArgumentSemantics::Position) {
							postfix = L" [[position]]";
						} else if (desc.semantics == ArgumentSemantics::InterstageNI) {
							postfix = L" [[user(i" + string(desc.index) + L")]] [[flat]]";
						} else if (desc.semantics == ArgumentSemantics::InterstageIL) {
							postfix = L" [[user(i" + string(desc.index) + L")]] [[center_no_perspective]]";
						} else if (desc.semantics == ArgumentSemantics::InterstageIP) {
							postfix = L" [[user(i" + string(desc.index) + L")]] [[center_perspective]]";
						} else if (desc.semantics == ArgumentSemantics::IsFrontFacing) {
							postfix = L" [[front_facing]]";
						} else if (desc.semantics == ArgumentSemantics::Color) {
							postfix = L" [[color(" + string(desc.index) + L")]]";
						} else if (desc.semantics == ArgumentSemantics::SecondColor) {
							postfix = L" [[color(0), index(1)]]";
						} else if (desc.semantics == ArgumentSemantics::Depth) {
							postfix = L" [[depth_argument(any)]]";
						} else if (desc.semantics == ArgumentSemantics::Stencil) {
							postfix = L" [[stencil]]";
						} else if (desc.semantics == ArgumentSemantics::Constant) {
							prefix = L"constant ";
							postfix = L" [[buffer(" + string(desc.index) + L")]]";
						} else if (desc.semantics == ArgumentSemantics::Buffer) {
							prefix = L"constant ";
							postfix = L" [[buffer(" + string(desc.index) + L")]]";
						} else if (desc.semantics == ArgumentSemantics::Texture) {
							postfix = L" [[texture(" + string(desc.index) + L")]]";
						} else if (desc.semantics == ArgumentSemantics::Sampler) {
							postfix = L" [[sampler(" + string(desc.index) + L")]]";
						}
					}
				}
				static uint _reg(const XA::ObjectReference & ref) noexcept { return uint(ref.ref_class) | (ref.index << 8); }
				bool _invoke(DecompilerContext & ctx, FunctionDesc & fdesc, XA::Function & xasm, const SymbolArtifact & func, int argc, const XA::ExpressionTree * argv, expression_node & out, int & indent)
				{
					Array<expression_node> inputs(0x20);
					for (int i = 0; i < argc; i++) {
						expression_node in;
						if (!_process_expression(ctx, fdesc, xasm, argv[i], XI::Module::TypeReference(func.fdesc.args[i].tcn).GetClassName(), in, indent)) throw Exception();
						if (!(in.hint & 1)) throw Exception();
						inputs << in;
					}
					out.clsname = XI::Module::TypeReference(func.fdesc.rv.tcn).GetClassName();
					if (func.symbol_refer.Length()) {
						DynamicString call;
						call << func.symbol_refer << L"(";
						for (int i = 0; i < inputs.Length(); i++) {
							if (i) call << L", ";
							if (!(inputs[i].hint & 8) && func.fdesc.args[i].inout) {
								auto tname = L"temp" + string(_temp_counter, HexadecimalBase, 6);
								_temp_counter++;
								if (_hr()) _output << string(L'\t', indent);
								_output << _get_type_name(ctx, L"C" + inputs[i].clsname) << L" " << tname << L";";
								if (_hr()) _output << L'\n';
								call << L"(" << tname << L" = " << inputs[i].code << L")";
							} else call << inputs[i].code;
						}
						call << L")";
						out.hint = 1;
						out.code = call.ToString();
					} else if (func.rules.blocks.Length()) {
						bool read_only = false;
						for (auto & e : func.rules.extrefs) if (e == L"L") { read_only = true; break; }
						DynamicString call;
						for (auto & r : func.rules.blocks) {
							if (r.rule == Rule::InsertArgument) {
								if (r.index >= inputs.Length()) throw Exception();
								if (!(inputs[r.index].hint & 8) && func.fdesc.args[r.index].inout && !read_only) {
									auto tname = L"temp" + string(_temp_counter, HexadecimalBase, 6);
									_temp_counter++;
									if (_hr()) _output << string(L'\t', indent);
									_output << _get_type_name(ctx, L"C" + inputs[r.index].clsname) << L" " << tname << L";";
									if (_hr()) _output << L'\n';
									call << L"(" << tname << L" = " << inputs[r.index].code << L")";
								} else call << inputs[r.index].code;
							} else if (r.rule == Rule::InsertString) {
								call << r.text;
							} else throw Exception();
						}
						out.hint = 1;
						out.code = call.ToString();
						for (auto & e : func.rules.extrefs) if (e == L"A") { out.hint |= 8; break; }
					} else {
						out.hint = 0;
						out.code = L"";
					}
					return true;
				}
				bool _process_expression(DecompilerContext & ctx, FunctionDesc & fdesc, XA::Function & xasm, const XA::ExpressionTree & node, const string & expect_rvc, expression_node & out, int & indent)
				{
					if (node.self.ref_flags & XA::ReferenceFlagInvoke) {
						if (node.self.ref_class == XA::ReferenceExternal) {
							if (node.self.index >= xasm.extrefs.Length()) throw InvalidFormatException();
							auto smbl = xasm.extrefs[node.self.index].Fragment(2, -1);
							auto index = ctx.symbol_map[smbl];
							if (!index) throw InvalidFormatException();
							auto & func = ctx.symbol_cache[*index];
							if (func.symbol_class == 1) {
								return _invoke(ctx, fdesc, xasm, func, node.inputs.Length(), node.inputs.GetBuffer(), out, indent);
							} else if (func.symbol_class == 2) {
								if (node.inputs.Length() < 1) throw InvalidFormatException();
								expression_node init;
								if (!_invoke(ctx, fdesc, xasm, func, node.inputs.Length() - 1, node.inputs.GetBuffer() + 1, init, indent)) return false;
								expression_node subject;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], XI::Module::TypeReference(func.fdesc.instance_tcn).GetClassName(), subject, indent)) return false;
								if (subject.hint & 6) {
									if (init.hint & 1) {
										out = init;
									} else {
										out.hint = 0;
										out.code = L"";
										out.clsname = XI::Module::TypeReference(func.fdesc.instance_tcn).GetClassName();
									}
								} else if (subject.hint & 1) {
									if (init.hint & 1) {
										out.hint = subject.hint & 9;
										out.code = subject.code + L" = " + init.code;
										out.clsname = XI::Module::TypeReference(func.fdesc.instance_tcn).GetClassName();
									} else {
										out.hint = 0;
										out.code = L"";
										out.clsname = XI::Module::TypeReference(func.fdesc.instance_tcn).GetClassName();
									}
								} else {
									out.hint = 0;
									out.code = L"";
									out.clsname = XI::Module::TypeReference(func.fdesc.instance_tcn).GetClassName();
								}
							} else throw InvalidFormatException();
						} else if (node.self.ref_class == XA::ReferenceTransform) {
							if (node.self.index == XA::TransformFollowPointer || node.self.index == XA::TransformTakePointer) {
								if (node.inputs.Length() != 1) throw InvalidFormatException();
								return _process_expression(ctx, fdesc, xasm, node.inputs[0], expect_rvc, out, indent);
							} else if (node.self.index == XA::TransformAddressOffset) {
								if (node.inputs.Length() != 2) throw InvalidFormatException();
								if (node.inputs[1].self.ref_class != XA::ReferenceLiteral) throw InvalidFormatException();
								expression_node subject;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], L"", subject, indent)) return false;
								if (!(subject.hint & 1)) throw InvalidFormatException();
								auto index = ctx.symbol_map[subject.clsname];
								if (!index) throw InvalidFormatException();
								auto & cls = ctx.symbol_cache[*index];
								if (cls.symbol_class != 0) throw InvalidFormatException();
								int offset = node.input_specs[1].size.num_bytes + 4 * node.input_specs[1].size.num_words;
								auto field = cls.fields[offset];
								if (!field) throw InvalidFormatException();
								out.hint = subject.hint & 9;
								out.code = L"(" + subject.code + L")." + field->key;
								out.clsname = XI::Module::TypeReference(field->value).GetClassName();
							} else if (node.self.index == XA::TransformFloatRecombine) {
								if (node.inputs.Length() != 2) throw InvalidFormatException();
								if (node.inputs[1].self.ref_class != XA::ReferenceLiteral) throw InvalidFormatException();
								expression_node subject;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], L"", subject, indent)) return false;
								if (!(subject.hint & 1)) throw InvalidFormatException();
								uint coordsize = node.self.ref_flags & XA::ReferenceFlagShort ? 1 : 4;
								uint mask = node.input_specs[1].size.num_bytes;
								uint idim = (node.input_specs[0].size.num_bytes + 4 * node.input_specs[0].size.num_words) / coordsize;
								uint odim = (node.retval_spec.size.num_bytes + 4 * node.retval_spec.size.num_words) / coordsize;
								out.hint = subject.hint & 9;
								out.code = L"(" + subject.code + L").";
								uint used_mask = 0;
								for (uint i = 0; i < odim; i++) {
									auto idx = (mask >> (4 * i)) & 0xF;
									uint coord_mask = 0;
									if (idx == 0) { out.code += L"x"; coord_mask = 1; }
									else if (idx == 1) { out.code += L"y"; coord_mask = 2; }
									else if (idx == 2) { out.code += L"z"; coord_mask = 4; }
									else if (idx == 3) { out.code += L"w"; coord_mask = 8; }
									else throw InvalidArgumentException();
									if (used_mask & coord_mask) out.hint &= 1;
									used_mask |= coord_mask;
								}
								if (subject.clsname.Fragment(0, 7) == L"logicum") {
									if (odim == 1) out.clsname = L"logicum";
									else if (odim == 2) out.clsname = L"logicum2";
									else if (odim == 3) out.clsname = L"logicum3";
									else if (odim == 4) out.clsname = L"logicum4";
									else throw InvalidArgumentException();
								} else if (subject.clsname.Fragment(0, 3) == L"int") {
									if (odim == 1) out.clsname = L"int32";
									else if (odim == 2) out.clsname = L"int2";
									else if (odim == 3) out.clsname = L"int3";
									else if (odim == 4) out.clsname = L"int4";
									else throw InvalidArgumentException();
								} else if (subject.clsname.Fragment(0, 4) == L"nint") {
									if (odim == 1) out.clsname = L"nint32";
									else if (odim == 2) out.clsname = L"nint2";
									else if (odim == 3) out.clsname = L"nint3";
									else if (odim == 4) out.clsname = L"nint4";
									else throw InvalidArgumentException();
								} else if (subject.clsname.Fragment(0, 4) == L"frac") {
									if (odim == 1) out.clsname = L"frac";
									else if (odim == 2) out.clsname = L"frac2";
									else if (odim == 3) out.clsname = L"frac3";
									else if (odim == 4) out.clsname = L"frac4";
									else throw InvalidArgumentException();
								} else throw InvalidArgumentException();
							} else if (node.self.index == XA::TransformTemporary) {
								if (node.inputs.Length() != 1) throw InvalidFormatException();
								return _process_expression(ctx, fdesc, xasm, node.inputs[0], L"", out, indent);
							} else if (node.self.index == XA::TransformMove) {
								if (node.inputs.Length() != 2) throw InvalidFormatException();
								expression_node asgn;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], expect_rvc, out, indent)) return false;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[1], expect_rvc, asgn, indent)) return false;
								if ((out.hint & 6) && (asgn.hint & 1)) {
									out = asgn;
								} else if (out.hint & 1) {
									out.code = L"(" + out.code + L" = " + asgn.code + L")";
									out.hint & 9;
								} else {
									out.hint = 0;
									out.clsname = out.code = L"";
								}
							} else if (node.self.index == XA::TransformSplit) {
								if (node.inputs.Length() != 1) throw InvalidFormatException();
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], expect_rvc, out, indent)) return false;
								_split_node = out;
							} else if (node.self.index == XA::TransformLogicalAnd) {
								if (node.inputs.Length() < 1) throw InvalidFormatException();
								DynamicString text;
								text << L"(";
								for (int i = 0; i < node.inputs.Length(); i++) {
									if (i) text << L" && ";
									expression_node cond;
									if (!_process_expression(ctx, fdesc, xasm, node.inputs[i], expect_rvc, cond, indent)) return false;
									if (!(cond.hint & 1)) throw InvalidFormatException();
									out.clsname = cond.clsname;
									text << cond.code;
								}
								text << L")";
								out.hint = 1;
								out.code = text.ToString();
							} else if (node.self.index == XA::TransformLogicalOr) {
								if (node.inputs.Length() < 1) throw InvalidFormatException();
								DynamicString text;
								text << L"(";
								for (int i = 0; i < node.inputs.Length(); i++) {
									if (i) text << L" || ";
									expression_node cond;
									if (!_process_expression(ctx, fdesc, xasm, node.inputs[i], expect_rvc, cond, indent)) return false;
									if (!(cond.hint & 1)) throw InvalidFormatException();
									if (!i) out.clsname = cond.clsname;
									text << cond.code;
								}
								text << L")";
								out.hint = 1;
								out.code = text.ToString();
							} else if (node.self.index == XA::TransformLogicalFork) {
								if (node.inputs.Length() != 3) throw InvalidFormatException();
								expression_node cond, a, b;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], expect_rvc, cond, indent)) return false;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[1], expect_rvc, a, indent)) return false;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[2], expect_rvc, b, indent)) return false;
								if (!(cond.hint & 1)) throw InvalidFormatException();
								if (!(a.hint & 1)) throw InvalidFormatException();
								if (!(b.hint & 1)) throw InvalidFormatException();
								out.hint = 1;
								out.clsname = a.clsname;
								out.code = L"((" + cond.code + L") ? (" + a.code + L") : (" + b.code + L"))";
							} else if (node.self.index == XA::TransformLogicalNot) {
								if (node.inputs.Length() != 1) throw InvalidFormatException();
								expression_node cond;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], expect_rvc, cond, indent)) return false;
								if (!(cond.hint & 1)) throw InvalidFormatException();
								out.hint = 1;
								out.clsname = cond.clsname;
								out.code = L"!(" + cond.code + L")";
							} else if (node.self.index == XA::TransformLogicalSame) {
								if (node.inputs.Length() != 2) throw InvalidFormatException();
								expression_node a, b;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], expect_rvc, a, indent)) return false;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[1], expect_rvc, b, indent)) return false;
								if (!(a.hint & 1)) throw InvalidFormatException();
								if (!(b.hint & 1)) throw InvalidFormatException();
								out.hint = 1;
								out.clsname = a.clsname;
								out.code = L"((" + a.code + L" == 0) == (" + b.code + L" == 0))";
							} else if (node.self.index == XA::TransformLogicalNotSame) {
								if (node.inputs.Length() != 2) throw InvalidFormatException();
								expression_node a, b;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[0], expect_rvc, a, indent)) return false;
								if (!_process_expression(ctx, fdesc, xasm, node.inputs[1], expect_rvc, b, indent)) return false;
								if (!(a.hint & 1)) throw InvalidFormatException();
								if (!(b.hint & 1)) throw InvalidFormatException();
								out.hint = 1;
								out.clsname = a.clsname;
								out.code = L"((" + a.code + L" == 0) != (" + b.code + L" == 0))";
							} else throw InvalidFormatException();
						} else throw InvalidFormatException();
					} else {
						if (node.self.ref_class == XA::ReferenceNull) {
							out.hint = 0;
							out.code = out.clsname = L"";
						} else if (node.self.ref_class == XA::ReferenceExternal) {
							if (node.self.index >= xasm.extrefs.Length()) throw InvalidFormatException();
							auto smbl = xasm.extrefs[node.self.index].Fragment(2, -1);
							auto index = ctx.symbol_map[smbl];
							if (!index) throw InvalidFormatException();
							auto & lit = ctx.symbol_cache[*index];
							if (lit.symbol_class != 3) throw InvalidFormatException();
							DynamicString text;
							text << L"(";
							for (auto & r : lit.rules.blocks) if (r.rule == Rule::InsertString) text << r.text;
							text << L")";
							out.hint = 1;
							out.code = text.ToString();
							out.clsname = L"";
						} else if (node.self.ref_class == XA::ReferenceData) {
							if (expect_rvc == L"logicum") {
								if (node.self.index >= xasm.data.Length()) throw InvalidFormatException();
								out.hint = 1;
								out.code = *reinterpret_cast<bool *>(xasm.data.GetBuffer() + node.self.index) ? L"(true)" : L"(false)";
								out.clsname = expect_rvc;
							} else if (expect_rvc == L"nint32") {
								if (node.self.index + 4 > xasm.data.Length()) throw InvalidFormatException();
								out.hint = 1;
								out.code = L"(" + string(*reinterpret_cast<uint32 *>(xasm.data.GetBuffer() + node.self.index)) + L")";
								out.clsname = expect_rvc;
							} else if (expect_rvc == L"int32") {
								if (node.self.index + 4 > xasm.data.Length()) throw InvalidFormatException();
								out.hint = 1;
								out.code = L"(" + string(*reinterpret_cast<int32 *>(xasm.data.GetBuffer() + node.self.index)) + L")";
								out.clsname = expect_rvc;
							} else if (expect_rvc == L"frac") {
								if (node.self.index + 4 > xasm.data.Length()) throw InvalidFormatException();
								auto data = *reinterpret_cast<float *>(xasm.data.GetBuffer() + node.self.index);
								if (isfinite(data)) {
									auto text = string(data);
									if (text.FindFirst(L'.') < 0) text += L".0";
									out.hint = 1;
									out.code = L"(" + text + L"f)";
									out.clsname = expect_rvc;
								} else {
									out.hint = 1;
									out.code = L"(0.0f)";
									out.clsname = expect_rvc;
								}
							} else throw InvalidFormatException();
						} else if (node.self.ref_class == XA::ReferenceArgument) {
							auto nd = _register_mapping[RegisterID(node.self.ref_class, node.self.index)];
							if (!nd) throw InvalidFormatException();
							out = *nd;
						} else if (node.self.ref_class == XA::ReferenceRetVal) {
							out.hint = 12;
							out.code = out.clsname = L"";
						} else if (node.self.ref_class == XA::ReferenceLocal) {
							auto nd = _register_mapping[RegisterID(node.self.ref_class, node.self.index)];
							if (!nd) throw InvalidFormatException();
							out = *nd;
						} else if (node.self.ref_class == XA::ReferenceInit) {
							out.hint = 10;
							out.code = out.clsname = L"";
						} else if (node.self.ref_class == XA::ReferenceSplitter) {
							out = _split_node;
						} else throw InvalidFormatException();
					}
					return true;
				}
				bool _process_operator_range(DecompilerContext & ctx, FunctionDesc & fdesc, XA::Function & xasm, int from, int count, int & indent)
				{
					for (int i = 0; i < count; i++) {
						auto & in = xasm.instset[from + i];
						if (in.opcode == XA::OpcodeNOP) {
						} else if (in.opcode == XA::OpcodeTrap) {
						} else if (in.opcode == XA::OpcodeOpenScope) {
							uint lreg = _local_register;
							uint depth = 0;
							int j;
							for (j = i; j < count; j++) {
								if (xasm.instset[from + j].opcode == XA::OpcodeOpenScope) depth++;
								else if (xasm.instset[from + j].opcode == XA::OpcodeCloseScope) { depth--; if (!depth) break; }
							}
							if (depth) throw InvalidFormatException();
							if (!_process_operator_range(ctx, fdesc, xasm, from + i + 1, j - i - 1, indent)) return false;
							_local_register = lreg;
							auto current = _register_mapping.GetFirst();
							while (current) {
								auto next = current->GetNext();
								auto & value = current->GetValue();
								uint reg, index;
								ReadRegisterID(value.key, reg, index);
								if (reg == XA::ReferenceLocal && index >= _local_register) _register_mapping.Remove(value.key);
								current = next;
							}
							i = j;
						} else if (in.opcode == XA::OpcodeCloseScope) {
							throw InvalidStateException();
						} else if (in.opcode == XA::OpcodeExpression) {
							expression_node result;
							if (!_process_expression(ctx, fdesc, xasm, in.tree, L"", result, indent)) return false;
							if (result.hint & 1) {
								if (_hr()) _output << string(L'\t', indent);
								_output << result.code << L";";
								if (_hr()) _output << L'\n';
							}
						} else if (in.opcode == XA::OpcodeNewLocal) {
							auto name = L"var" + string(_variable_counter, HexadecimalBase, 6);
							_variable_counter++;
							expression_node init;
							if (!_process_expression(ctx, fdesc, xasm, in.tree, L"", init, indent)) return false;
							if (!init.clsname.Length()) throw InvalidFormatException();
							auto index = ctx.symbol_map[init.clsname];
							if (!index) throw InvalidFormatException();
							auto & cls = ctx.symbol_cache[*index];
							if (cls.symbol_class != 0) throw InvalidFormatException();
							if (_hr()) _output << string(L'\t', indent);
							_output << cls.symbol_refer << L" " << name;
							if (init.hint & 1) _output << L" = " << init.code;
							_output << L";";
							if (_hr()) _output << L'\n';
							auto reg = RegisterID(XA::ReferenceLocal, _local_register);
							_local_register++;
							expression_node refer(name, init.clsname, 9);
							_register_mapping.Append(reg, refer);
						} else if (in.opcode == XA::OpcodeUnconditionalJump) {
							throw InvalidStateException();
						} else if (in.opcode == XA::OpcodeConditionalJump) {
							throw InvalidStateException();
						} else if (in.opcode == XA::OpcodeControlReturn) {
							expression_node init;
							if (!_process_expression(ctx, fdesc, xasm, in.tree, L"", init, indent)) return false;
							if (_return_statement.Length()) {
								if (_hr()) _output << string(L'\t', indent);
								_output << _return_statement;
								if (_hr()) _output << L'\n';
							} else if (init.hint & 1) {
								if (_hr()) _output << string(L'\t', indent);
								_output << L"return " << init.code << L";";
								if (_hr()) _output << L'\n';
							} else {
								if (_hr()) _output << string(L'\t', indent);
								_output << L"return;";
								if (_hr()) _output << L'\n';
							}
						} else if (in.opcode == HintScope) {
							if (_hr()) _output << string(L'\t', indent);
							_output << L'{';
							if (_hr()) _output << L'\n';
							indent++;
						} else if (in.opcode == HintEndScope) {
							indent--;
							if (_hr()) _output << string(L'\t', indent);
							_output << L'}';
							if (_hr()) _output << L'\n';
						} else if (in.opcode == HintIf) {
							if (i + 1 >= count) throw InvalidFormatException();
							auto & cj = xasm.instset[from + i + 1];
							if (cj.opcode != XA::OpcodeConditionalJump) throw InvalidFormatException();
							int skip_to = i + 2 + cj.attachment.num_bytes;
							expression_node cond;
							if (cj.tree.self.ref_class == XA::ReferenceTransform && cj.tree.self.index == XA::TransformLogicalNot) {
								if (cj.tree.inputs.Length() != 1) throw InvalidFormatException();
								if (!_process_expression(ctx, fdesc, xasm, cj.tree.inputs[0], L"logicum", cond, indent)) return false;
							} else {
								if (!_process_expression(ctx, fdesc, xasm, cj.tree, L"logicum", cond, indent)) return false;
								cond.code = L"!(" + cond.code + L")";
							}
							if (!(cond.hint & 1)) throw InvalidFormatException();
							if (_hr()) _output << string(L'\t', indent);
							_output << L"if (" << cond.code << L") {";
							if (_hr()) _output << L'\n';
							indent++;
							if (skip_to < i + 2) throw InvalidFormatException();
							bool has_else;
							for (int j = skip_to - 1; j >= i + 2; j--) {
								if (xasm.instset[from + j].opcode == HintElse) { has_else = true; break; }
								else if (xasm.instset[from + j].opcode == HintEndIf) { has_else = false; break; }
							}
							if (has_else) {
								if (!_process_operator_range(ctx, fdesc, xasm, from + i + 2, skip_to - i - 3, indent)) return false;
								auto & cj2 = xasm.instset[from + skip_to - 1];
								if (cj2.opcode != XA::OpcodeUnconditionalJump) throw InvalidFormatException();
								int skip_to_2 = skip_to + cj2.attachment.num_bytes;
								if (_hr()) _output << string(L'\t', indent - 1);
								_output << L"} else {";
								if (_hr()) _output << L'\n';
								if (!_process_operator_range(ctx, fdesc, xasm, from + skip_to, skip_to_2 - skip_to, indent)) return false;
								indent--;
								if (_hr()) _output << string(L'\t', indent);
								_output << L"}";
								if (_hr()) _output << L'\n';
								i = skip_to_2 - 1;
							} else {
								if (!_process_operator_range(ctx, fdesc, xasm, from + i + 2, skip_to - i - 2, indent)) return false;
								indent--;
								if (_hr()) _output << string(L'\t', indent);
								_output << L"}";
								if (_hr()) _output << L'\n';
								i = skip_to - 1;
							}
						} else if (in.opcode == HintElse) {
						} else if (in.opcode == HintEndIf) {
						} else if (in.opcode == HintWhile) {
							if (i + 1 >= count) throw InvalidFormatException();
							auto & cj = xasm.instset[from + i + 1];
							if (cj.opcode != XA::OpcodeConditionalJump) throw InvalidFormatException();
							int skip_to = i + 2 + cj.attachment.num_bytes;
							expression_node cond;
							if (cj.tree.self.ref_class == XA::ReferenceTransform && cj.tree.self.index == XA::TransformLogicalNot) {
								if (cj.tree.inputs.Length() != 1) throw InvalidFormatException();
								if (!_process_expression(ctx, fdesc, xasm, cj.tree.inputs[0], L"logicum", cond, indent)) return false;
							} else {
								if (!_process_expression(ctx, fdesc, xasm, cj.tree, L"logicum", cond, indent)) return false;
								cond.code = L"!(" + cond.code + L")";
							}
							if (!(cond.hint & 1)) throw InvalidFormatException();
							if (_hr()) _output << string(L'\t', indent);
							_output << L"while (" << cond.code << L") {";
							if (_hr()) _output << L'\n';
							indent++;
							if (skip_to < i + 2) throw InvalidFormatException();
							if (!_process_operator_range(ctx, fdesc, xasm, from + i + 2, skip_to - i - 3, indent)) return false;
							indent--;
							if (_hr()) _output << string(L'\t', indent);
							_output << L"}";
							if (_hr()) _output << L'\n';
							i = skip_to - 1;
						} else if (in.opcode == HintDo) {
							int j = i + 1, l = 1;
							while (j < count) {
								auto & ht = xasm.instset[from + j];
								if (ht.opcode == HintWhile || ht.opcode == HintDo || ht.opcode == HintFor) l++;
								else if (ht.opcode == HintEndLoop) l--;
								if (!l) break;
								j++;
							}
							if (l) throw InvalidFormatException();
							while (j < count) {
								auto & ht = xasm.instset[from + j];
								if (ht.opcode == XA::OpcodeConditionalJump) break;
								j++;
							}
							if (j == count) throw InvalidFormatException();
							auto & cj = xasm.instset[from + j];
							expression_node cond;
							if (!_process_expression(ctx, fdesc, xasm, cj.tree, L"logicum", cond, indent)) return false;
							if (!(cond.hint & 1)) throw InvalidFormatException();
							if (_hr()) _output << string(L'\t', indent);
							_output << L"do {";
							if (_hr()) _output << L'\n';
							indent++;
							if (!_process_operator_range(ctx, fdesc, xasm, from + i + 1, j - i - 1, indent)) return false;
							indent--;
							if (_hr()) _output << string(L'\t', indent);
							_output << L"} while (" << cond.code << L");";
							if (_hr()) _output << L'\n';
							i = j;
						} else if (in.opcode == HintForInit) {
							i++;
							if (i >= count) throw InvalidFormatException();
							string init;
							expression_node cond, step;
							if (xasm.instset[from + i].opcode == XA::OpcodeExpression) {
								expression_node init_node;
								if (!_process_expression(ctx, fdesc, xasm, xasm.instset[from + i].tree, L"", init_node, indent)) return false;
								if (init_node.hint & 1) init = init_node.code;
								else init = L"";
							} else if (xasm.instset[from + i].opcode == XA::OpcodeNewLocal) {
								auto name = L"var" + string(_variable_counter, HexadecimalBase, 6);
								_variable_counter++;
								expression_node init_node;
								if (!_process_expression(ctx, fdesc, xasm, xasm.instset[from + i].tree, L"", init_node, indent)) return false;
								if (!init_node.clsname.Length()) throw InvalidFormatException();
								auto index = ctx.symbol_map[init_node.clsname];
								if (!index) throw InvalidFormatException();
								auto & cls = ctx.symbol_cache[*index];
								if (cls.symbol_class != 0) throw InvalidFormatException();
								init = cls.symbol_refer + L" " + name;
								if (init_node.hint & 1) init += L" = " + init_node.code;
								auto reg = RegisterID(XA::ReferenceLocal, _local_register);
								_local_register++;
								expression_node refer(name, init_node.clsname, 9);
								_register_mapping.Append(reg, refer);
							} else throw InvalidFormatException();
							i++;
							if (i >= count) throw InvalidFormatException();
							if (xasm.instset[from + i].opcode != HintFor) throw InvalidFormatException();
							i++;
							if (i >= count) throw InvalidFormatException();
							auto & cj = xasm.instset[from + i];
							if (cj.opcode != XA::OpcodeConditionalJump) throw InvalidFormatException();
							int skip_to = i + 1 + cj.attachment.num_bytes;
							if (cj.tree.self.ref_class == XA::ReferenceTransform && cj.tree.self.index == XA::TransformLogicalNot) {
								if (cj.tree.inputs.Length() != 1) throw InvalidFormatException();
								if (!_process_expression(ctx, fdesc, xasm, cj.tree.inputs[0], L"logicum", cond, indent)) return false;
							} else {
								if (!_process_expression(ctx, fdesc, xasm, cj.tree, L"logicum", cond, indent)) return false;
								cond.code = L"!(" + cond.code + L")";
							}
							if (!(cond.hint & 1)) throw InvalidFormatException();
							if (skip_to < i + 1) throw InvalidFormatException();
							i++;
							if (xasm.instset[from + skip_to - 1].opcode != XA::OpcodeUnconditionalJump) throw InvalidFormatException();
							if (xasm.instset[from + skip_to - 2].opcode != XA::OpcodeExpression) throw InvalidFormatException();
							if (!_process_expression(ctx, fdesc, xasm, xasm.instset[from + skip_to - 2].tree, L"", step, indent)) return false;
							if (!(step.hint & 1)) throw InvalidFormatException();
							if (_hr()) _output << string(L'\t', indent);
							_output << L"for (" << init << L";" << cond.code << L";" << step.code << L") {";
							if (_hr()) _output << L'\n';
							indent++;
							if (!_process_operator_range(ctx, fdesc, xasm, from + i, skip_to - i - 2, indent)) return false;
							indent--;
							if (_hr()) _output << string(L'\t', indent);
							_output << L"}";
							if (_hr()) _output << L'\n';
							i = skip_to - 1;
						} else if (in.opcode == HintFor) {
						} else if (in.opcode == HintEndLoop) {
						} else if (in.opcode == HintBreak) {
							if (_hr()) _output << string(L'\t', indent);
							_output << L"break;";
							if (_hr()) _output << L'\n';
							i++;
						} else if (in.opcode == HintContinue) {
							if (_hr()) _output << string(L'\t', indent);
							_output << L"continue;";
							if (_hr()) _output << L'\n';
							i++;
						} else if (in.opcode == HintReturn) {
						} else throw InvalidFormatException();
					}
					return true;
				}
			public:
				FunctionGenerator(ShaderLanguageFunctionGenerator & gen) : _gen(gen), _variable_counter(0), _temp_counter(0), _local_register(0) {}
				bool ProcessFunction(DecompilerContext & ctx, const string & vname, FunctionDesc & fdesc, XA::Function & xasm, string & effective_name)
				{
					if (!ctx.ValidateSemantics(fdesc)) return false;
					SafePointer<SymbolArtifact> art = new SymbolArtifact;
					if (fdesc.constructor) art->symbol_class = 2; else art->symbol_class = 1;
					art->class_alignment = art->class_size = 0;
					art->private_class = true;
					art->fdesc = fdesc;
					art->symbol_xw = vname;
					effective_name = art->symbol_refer = art->symbol_refer_publically = L"functio" + string(_gen._name_counter, HexadecimalBase, 6);
					_gen._name_counter++;
					if (fdesc.fdes != FunctionDesignation::Service) {
						if (!IsCIdentifier(fdesc.fname)) {
							SetError(ctx.status, DecompilerStatus::BadShaderName, _gen.GetLanguage(), vname, fdesc.fname);
							return false;
						}
					}
					if (fdesc.fdes == FunctionDesignation::Service) {
						int da = fdesc.constructor ? 1 : 0;
						_output << _get_type_name(ctx, fdesc.rv.tcn) << L" " << art->symbol_refer << L"(";
						for (int i = 0; i < fdesc.args.Length(); i++) {
							if (i) {
								_output << L",";
								if (_hr()) _output << L" ";
							}
							if (_gen.GetLanguage() == ShaderLanguage::HLSL) {
								if (fdesc.args[i].inout) _output << L"inout ";
								else _output << L"in ";
							} else if (_gen.GetLanguage() == ShaderLanguage::MSL && fdesc.args[i].inout) {
								_output << L"thread ";
							}
							_output << _get_type_name(ctx, fdesc.args[i].tcn);
							if (_gen.GetLanguage() == ShaderLanguage::MSL && fdesc.args[i].inout) {
								_output << L" &";
							}
							auto aname = L"arg" + string(uint(i), HexadecimalBase, 3);
							_output << L" " << aname;
							_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i + da)), expression_node(aname, XI::Module::TypeReference(fdesc.args[i].tcn), fdesc.args[i].inout ? 9 : 1));
						}
						_output << L")";
						if (_hr()) _output << L" ";
						_output << L"{";
						if (_hr()) _output << L"\n";
						if (fdesc.constructor) {
							if (_hr()) _output << L"\t";
							_output << _get_type_name(ctx, fdesc.instance_tcn) << L" xw_cr;";
							if (_hr()) _output << L"\n";
							_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, 0)), expression_node(L"xw_cr", XI::Module::TypeReference(fdesc.instance_tcn), 9));
							_return_statement = L"return xw_cr;";
						}
					} else {
						bool msl_interstage_in = false;
						if (_gen.GetLanguage() == ShaderLanguage::HLSL) {
							for (int i = 0; i < fdesc.args.Length(); i++) {
								auto & arg = fdesc.args[i];
								auto aname = L"arg" + string(uint(i), HexadecimalBase, 3);
								auto anode = expression_node(aname, XI::Module::TypeReference(arg.tcn), 1);
								if (arg.semantics == ArgumentSemantics::Constant) {
									_output << _get_type_name(ctx, arg.tcn) << L" " << aname;
									_output << L" : register(b[" << string(arg.index) << L"]);";
									if (_hr()) _output << L"\n";
									_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i)), anode);
								} else if (arg.semantics == ArgumentSemantics::Buffer || arg.semantics == ArgumentSemantics::Texture) {
									_output << _get_type_name(ctx, arg.tcn) << L" " << aname;
									_output << L" : register(t[" << string(arg.index) << L"]);";
									if (_hr()) _output << L"\n";
									_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i)), anode);
								} else if (arg.semantics == ArgumentSemantics::Sampler) {
									_output << _get_type_name(ctx, arg.tcn) << L" " << aname;
									_output << L" : register(s[" << string(arg.index) << L"]);";
									if (_hr()) _output << L"\n";
									_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i)), anode);
								}
							}
						} else if (_gen.GetLanguage() == ShaderLanguage::MSL) {
							_output << L"struct ex_" << art->symbol_refer;
							if (_hr()) _output << L" ";
							_output << L"{";
							if (_hr()) _output << L"\n";
							for (int i = 0; i < fdesc.args.Length(); i++) {
								if (fdesc.args[i].inout) {
									if (_hr()) _output << L"\t";
									_output << _get_type_name(ctx, fdesc.args[i].tcn);
									auto aname = L"arg" + string(uint(i), HexadecimalBase, 3);
									auto anode = expression_node(L"xw_ex." + aname, XI::Module::TypeReference(fdesc.args[i].tcn), 9);
									_output << L" " << aname;
									_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i)), anode);
									string pr, pf;
									_traits_for_semantic(fdesc.args[i], pr, pf);
									_output << pf;
									_output << L";";
									if (_hr()) _output << L"\n";
								} else {
									if (fdesc.args[i].semantics == ArgumentSemantics::InterstageNI) msl_interstage_in = true;
									else if (fdesc.args[i].semantics == ArgumentSemantics::InterstageIL) msl_interstage_in = true;
									else if (fdesc.args[i].semantics == ArgumentSemantics::InterstageIP) msl_interstage_in = true;
								}
							}
							_output << L"};";
							if (_hr()) _output << L"\n";
							if (msl_interstage_in) {
								_output << L"struct in_" << art->symbol_refer;
								if (_hr()) _output << L" ";
								_output << L"{";
								if (_hr()) _output << L"\n";
								for (int i = 0; i < fdesc.args.Length(); i++) {
									bool is = false;
									if (fdesc.args[i].semantics == ArgumentSemantics::InterstageNI) is = true;
									else if (fdesc.args[i].semantics == ArgumentSemantics::InterstageIL) is = true;
									else if (fdesc.args[i].semantics == ArgumentSemantics::InterstageIP) is = true;
									if (!fdesc.args[i].inout && is) {
										if (_hr()) _output << L"\t";
										_output << _get_type_name(ctx, fdesc.args[i].tcn);
										auto aname = L"arg" + string(uint(i), HexadecimalBase, 3);
										auto anode = expression_node(L"xw_in." + aname, XI::Module::TypeReference(fdesc.args[i].tcn), 1);
										_output << L" " << aname;
										_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i)), anode);
										string pr, pf;
										_traits_for_semantic(fdesc.args[i], pr, pf);
										_output << pf;
										_output << L";";
										if (_hr()) _output << L"\n";
									}
								}
								_output << L"};";
								if (_hr()) _output << L"\n";
							}
							_return_statement = L"return xw_ex;";
						}
						if (_gen.GetLanguage() == ShaderLanguage::HLSL) {
							_output << L"void";
						} else if (_gen.GetLanguage() == ShaderLanguage::MSL) {
							_output << L"[[host_name(\"" << fdesc.fname << L"\")]] ";
							if (fdesc.fdes == FunctionDesignation::Vertex) _output << L"vertex";
							else if (fdesc.fdes == FunctionDesignation::Pixel) _output << L"fragment";
							_output << L" ex_" << art->symbol_refer;
						}
						_output << L" " << art->symbol_refer << L"(";
						for (int i = 0; i < fdesc.args.Length(); i++) {
							if (_gen.GetLanguage() == ShaderLanguage::MSL) {
								if (fdesc.args[i].inout) continue;
								if (fdesc.args[i].semantics == ArgumentSemantics::InterstageNI) continue;
								else if (fdesc.args[i].semantics == ArgumentSemantics::InterstageIL) continue;
								else if (fdesc.args[i].semantics == ArgumentSemantics::InterstageIP) continue;
							} else if (_gen.GetLanguage() == ShaderLanguage::HLSL) {
								if (fdesc.args[i].semantics == ArgumentSemantics::Constant) continue;
								else if (fdesc.args[i].semantics == ArgumentSemantics::Buffer) continue;
								else if (fdesc.args[i].semantics == ArgumentSemantics::Texture) continue;
								else if (fdesc.args[i].semantics == ArgumentSemantics::Sampler) continue;
							}
							if (_output[_output.Length() - 1] != L'(') {
								_output << L",";
								if (_hr()) _output << L" ";
							}
							string pr, pf;
							_traits_for_semantic(fdesc.args[i], pr, pf);
							_output << pr << _get_type_name(ctx, fdesc.args[i].tcn);
							auto aname = L"arg" + string(uint(i), HexadecimalBase, 3);
							if (_gen.GetLanguage() == ShaderLanguage::MSL && fdesc.args[i].semantics == ArgumentSemantics::Constant) {
								_output << L" * " << aname;
								_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i)),
									expression_node(L"(*" + aname + L")", XI::Module::TypeReference(fdesc.args[i].tcn), fdesc.args[i].inout ? 9 : 1));
							} else {
								_output << L" " << aname;
								_register_mapping.Append(_reg(XA::TH::MakeRef(XA::ReferenceArgument, i)),
									expression_node(aname, XI::Module::TypeReference(fdesc.args[i].tcn), fdesc.args[i].inout ? 9 : 1));
							}
							_output << pf;
						}
						if (msl_interstage_in) {
							if (_output[_output.Length() - 1] != L'(') {
								_output << L",";
								if (_hr()) _output << L" ";
							}
							_output << L"in_" << art->symbol_refer << L" xw_in [[stage_in]]";
						}
						_output << L")";
						if (_hr()) _output << L" ";
						_output << L"{";
						if (_hr()) _output << L"\n";
						if (_gen.GetLanguage() == ShaderLanguage::MSL) {
							if (_hr()) _output << L"\t";
							_output << L"ex_" << art->symbol_refer << L" xw_ex;";
							if (_hr()) _output << L"\n";
						}
					}
					try {
						int indent = 1;
						if (!_process_operator_range(ctx, fdesc, xasm, 0, xasm.instset.Length(), indent)) return false;
					} catch (...) {
						SetError(ctx.status, DecompilerStatus::DecompilationError, _gen.GetLanguage(), vname);
						return false;
					}
					_output << L"}";
					if (_hr()) _output << L"\n";
					art->code_inject = _output.ToString();
					ctx.AddArtifact(art);
					return true;
				}
			};
			friend class FunctionGenerator;
		private:
			uint _proc_mask;
			bool _human_readable;
			uint _name_counter;
		public:
			ShaderLanguageFunctionGenerator(uint proc_mask, bool human_readable) : _proc_mask(proc_mask), _human_readable(human_readable), _name_counter(0) {}
			virtual bool IsHumanReadable(void) const noexcept override { return _human_readable; }
			virtual ShaderLanguage GetLanguage(void) const noexcept override
			{
				if (_proc_mask == DecompilerFlagProduceHLSL) return ShaderLanguage::HLSL;
				else if (_proc_mask == DecompilerFlagProduceMSL) return ShaderLanguage::MSL;
				else if (_proc_mask == DecompilerFlagProduceGLSL) return ShaderLanguage::GLSL;
				else return ShaderLanguage::Unknown;
			}
			virtual bool ProcessFunction(DecompilerContext & ctx, XI::Module::Function & func, const string & vname, FunctionDesc & fdesc, XA::Function & xasm, string & effective_name) override
			{
				FunctionGenerator gen(*this);
				return gen.ProcessFunction(ctx, vname, fdesc, xasm, effective_name);
			}
		};
		class CLanguageFunctionGenerator : public DecompilerContext::IFunctionGenerator
		{
			bool _human_readable;
		public:
			CLanguageFunctionGenerator(bool human_readable) : _human_readable(human_readable) {}
			virtual bool IsHumanReadable(void) const noexcept override { return _human_readable; }
			virtual ShaderLanguage GetLanguage(void) const noexcept override { return ShaderLanguage::Unknown; }
			virtual bool ProcessFunction(DecompilerContext & ctx, XI::Module::Function & func, const string & vname, FunctionDesc & fdesc, XA::Function & xasm, string & effective_name) override
			{
				if (fdesc.fdes == FunctionDesignation::Service) return true;
				DynamicString output;
				if (!ctx.ValidateSemantics(fdesc)) return false;
				SafePointer<SymbolArtifact> art = new SymbolArtifact;
				art->symbol_class = 1;
				art->class_alignment = art->class_size = 0;
				art->private_class = false;
				art->fdesc = fdesc;
				art->symbol_xw = vname;
				if (!IsCIdentifier(fdesc.fname)) {
					SetError(ctx.status, DecompilerStatus::BadShaderName, GetLanguage(), vname, fdesc.fname);
					return false;
				}
				effective_name = art->symbol_refer = art->symbol_refer_publically = fdesc.fname;
				output << L"namespace " << art->symbol_refer;
				if (_human_readable) output << L' ';
				output << L'{';
				if (_human_readable) output << L'\n';
				if (_human_readable) output << L'\t';
				output << L"constexpr const Engine::widechar * name = L\"" << fdesc.fname << L"\";";
				if (_human_readable) output << L'\n';
				for (auto & arg : fdesc.args) if (arg.name.Length() && IsCIdentifier(arg.name)) {
					if (arg.semantics == ArgumentSemantics::Constant || arg.semantics == ArgumentSemantics::Buffer ||
						arg.semantics == ArgumentSemantics::Texture || arg.semantics == ArgumentSemantics::Sampler) {
						string sname;
						if (arg.name[0] == L'_') sname = L"selector" + arg.name;
						else if (arg.name[0] >= L'A' && arg.name[0] <= L'a') sname = L"Selector" + arg.name;
						else sname = L"selector_" + arg.name;
						if (_human_readable) output << L'\t';
						output << L"constexpr int " << sname << L" = " << string(arg.index) << L';';
						if (_human_readable) output << L'\n';
					}
				}
				output << L'}';
				if (_human_readable) output << L'\n';
				art->code_inject = output.ToString();
				ctx.AddArtifact(art);
				return true;
			}
		};
		class ObjectStructureGenerator : public DecompilerContext::IStructureGenerator
		{
			struct _dynamic_state : public Object
			{
			public:
				SafePointer<SymbolArtifact> _current;
				XL::LObject * _class;
			};
		private:
			XL::LContext & _ctx;
			SafePointer<_dynamic_state> _state;
		public:
			ObjectStructureGenerator(XL::LContext & ctx) : _ctx(ctx) {}
			virtual ShaderLanguage GetLanguage(void) override { return ShaderLanguage::Unknown; }
			virtual bool OpenClass(DecompilerContext & ctx, XI::Module::Class & cls, const string & vname) override
			{
				_state = new _dynamic_state;
				_state->_current = new SymbolArtifact;
				_state->_current->symbol_class = 0;
				_state->_current->class_alignment = _state->_current->class_size = 0;
				_state->_current->symbol_xw = vname;
				bool prvt = cls.attributes.ElementExists(AttributePrivate);
				if (prvt) {
					_state.SetReference(0);
					return true;
				}
				_state->_current->private_class = false;
				auto alias = cls.attributes[AttributeMapXV];
				auto align = cls.attributes[AttributeAlignment];
				if (align) _state->_current->class_alignment = align->ToInt32();
				if (alias) {
					_state->_current->symbol_refer = *alias;
					if (vname.Fragment(0, 12) == L"@praeformae.") {
						int i = 12, l = 0;
						while (i < vname.Length()) {
							if (vname[i] == L'(') l++;
							else if (vname[i] == L')') l--;
							else if (vname[i] == L'.' && l == 0) break;
							i++;
						}
						auto pcn = vname.Fragment(12, i - 12);
						auto ref = XI::Module::TypeReference(pcn);
						auto arg = ref.GetAbstractInstanceParameterType().GetClassName();
						auto state = _state;
						if (!ctx.ProcessStructure(arg, *this)) return false;
						_state = state;
						auto index = ctx.symbol_map[arg];
						if (!index) { _state.SetReference(0); return true; }
						auto & tdata = ctx.symbol_cache[*index];
						if (tdata.symbol_class != 0) throw InvalidStateException();
						if (_state->_current->symbol_refer.FindFirst(L"$!") >= 0 && tdata.private_class) {
							SetError(ctx.status, DecompilerStatus::NotSupported, GetLanguage(), vname, arg);
							return false;
						}
						_state->_current->symbol_refer = _state->_current->symbol_refer.Replace(L"$E", tdata.symbol_refer_publically);
						_state->_current->symbol_refer = _state->_current->symbol_refer.Replace(L"$I", tdata.symbol_refer);
						_state->_current->symbol_refer = _state->_current->symbol_refer.Replace(L"$!", L"");
					}
					int index = _state->_current->symbol_refer.FindFirst(L'|');
					if (index >= 0) {
						_state->_current->symbol_refer_publically = _state->_current->symbol_refer.Fragment(index + 1, -1);
						_state->_current->symbol_refer = _state->_current->symbol_refer.Fragment(0, index);
					} else _state->_current->symbol_refer_publically = _state->_current->symbol_refer;
					if (!_state->_current->class_alignment) _state->_current->class_alignment = 4;
					_state->_current->class_size = AddressAlign(cls.instance_spec.size, _state->_current->class_alignment);
					ctx.AddArtifact(_state->_current);
					_state.SetReference(0);
					return true;
				} else {
					auto way = vname.Split(L'.');
					auto ns = _ctx.GetRootNamespace();
					for (int i = 0; i < way.Length() - 1; i++) ns = _ctx.CreateNamespace(ns, way[i]);
					_state->_current->symbol_refer_publically = _state->_current->symbol_refer = vname;
					_state->_class = _ctx.CreateClass(ns, way.LastElement());
					return true;
				}
			}
			virtual bool CreateField(DecompilerContext & ctx, XI::Module::Variable & fld, const string & vname) override
			{
				if (_state && _state->_current) {
					bool byref;
					string vcls;
					Array<int> alvl(0x10);
					DecomposeTCN(fld.type_canonical_name, vcls, alvl, byref);
					auto state = _state;
					if (!ctx.ProcessStructure(vcls, *this)) return false;
					_state = state;
					auto index = ctx.symbol_map[vcls];
					if (!index) { _state.SetReference(0); return true; }
					auto & tdata = ctx.symbol_cache[*index];
					if (tdata.symbol_class != 0) throw InvalidStateException();
					_state->_current->class_size = AddressAlign(_state->_current->class_size, tdata.class_alignment);
					if (tdata.class_alignment > _state->_current->class_alignment) _state->_current->class_alignment = tdata.class_alignment;
					SafePointer<XL::XType> type = XL::CreateType(L"C" + tdata.symbol_refer_publically, _ctx);
					_ctx.CreateField(_state->_class, vname, type, XA::TH::MakeSize(_state->_current->class_size, 0));
					auto fsize = type->GetArgumentSpecification();
					_state->_current->class_size += fsize.size.num_bytes + 4 * fsize.size.num_words;
					_state->_current->fields.Append(fld.offset.num_words * 4 + fld.offset.num_bytes, Volumes::KeyValuePair<string, string>(vname, fld.type_canonical_name));
				}
				return true;
			}
			virtual bool EndClass(DecompilerContext & ctx) override
			{
				if (_state && _state->_current) {
					ObjectArray<XL::LObject> vft(1);
					if (!_state->_current->class_alignment) _state->_current->class_alignment = 4;
					_state->_current->class_size = AddressAlign(_state->_current->class_size, _state->_current->class_alignment);
					_ctx.SetClassInstanceSize(_state->_class, XA::TH::MakeSize(_state->_current->class_size, 0));
					_ctx.CreateClassDefaultMethods(_state->_class, XL::CreateMethodConstructorInit | XL::CreateMethodConstructorCopy | XL::CreateMethodConstructorZero |
						XL::CreateMethodConstructorMove | XL::CreateMethodDestructor | XL::CreateMethodAssign, vft);
					ctx.AddArtifact(_state->_current);
				}
				_state.SetReference(0);
				return true;
			}
		};
		class ObjectFunctionGenerator : public DecompilerContext::IFunctionGenerator
		{
			XL::LContext & _ctx;
		public:
			ObjectFunctionGenerator(XL::LContext & ctx) : _ctx(ctx) {}
			virtual bool IsHumanReadable(void) const noexcept override { return false; }
			virtual ShaderLanguage GetLanguage(void) const noexcept override { return ShaderLanguage::Unknown; }
			virtual bool ProcessFunction(DecompilerContext & ctx, XI::Module::Function & func, const string & vname, FunctionDesc & fdesc, XA::Function & xasm, string & effective_name) override
			{
				if (fdesc.fdes == FunctionDesignation::Service) return true;
				if (!ctx.ValidateSemantics(fdesc)) return false;
				if (!IsCIdentifier(fdesc.fname)) {
					SetError(ctx.status, DecompilerStatus::BadShaderName, GetLanguage(), vname, fdesc.fname);
					return false;
				}
				effective_name = fdesc.fname;
				auto ns = _ctx.CreateNamespace(_ctx.GetRootNamespace(), effective_name);
				SafePointer<XL::LObject> name = _ctx.QueryLiteral(fdesc.fname);
				_ctx.AttachLiteral(name, ns, L"nomen");
				for (auto & arg : fdesc.args) if (arg.name.Length()) {
					if (arg.semantics == ArgumentSemantics::Constant || arg.semantics == ArgumentSemantics::Buffer ||
						arg.semantics == ArgumentSemantics::Texture || arg.semantics == ArgumentSemantics::Sampler) {
						string sname;
						if (arg.name[0] == L'_') sname = L"selector" + arg.name;
						else if (arg.name[0] >= L'A' && arg.name[0] <= L'a') sname = L"Selector" + arg.name;
						else sname = L"selector_" + arg.name;
						SafePointer<XL::LObject> selector = _ctx.QueryLiteral(arg.index, 4, true);
						_ctx.AttachLiteral(selector, ns, sname);
					}
				}
				return true;
			}
		};
		class LModuleLoader : public XL::IModuleLoadCallback
		{
			IDecompilerCallback & _callback;
		public:
			LModuleLoader(IDecompilerCallback & callback) : _callback(callback) {}
			virtual Streaming::Stream * GetModuleStream(const string & name) override { try { return _callback.QueryXModuleFileStream(name); } catch (...) { return 0; } }
			bool LoadModule(const string & name, XL::LContext & into_context, DecompilerStatusDesc & with_error)
			{
				if (!into_context.IncludeModule(name, this, false)) {
					SetError(with_error, DecompilerStatus::XModuleNotFound, name);
					return false;
				} else return true;
			}
		};

		IDecompilerCallback * CreateDecompilerCallback(const string * xmdl_pv, int xmdl_pc, const string * wmdl_pv, int wmdl_pc, IDecompilerCallback * dropback) { return new ListDecompilerCallback(xmdl_pv, xmdl_pc, wmdl_pv, wmdl_pc, dropback); }
		void Decompile(DecompileDesc & desc)
		{
			if (desc.flags & DecompilerFlagForThisMachine) {
				#ifdef ENGINE_WINDOWS
					desc.flags |= DecompilerFlagProduceHLSL;
				#endif
				#ifdef ENGINE_UNIX
					#ifdef ENGINE_MACOSX
						desc.flags |= DecompilerFlagProduceMSL;
					#elif
						desc.flags |= DecompilerFlagProduceGLSL;
					#endif
				#endif
			}
			try {
				AssemblyDesc asmdata;
				if (!LoadAssembly(asmdata, desc)) return;
				ObjectArray<Artifact> artifacts(0x20);
				if (desc.flags & DecompilerFlagProduceHLSL) {
					DecompilerContext dctx(desc.status, asmdata, artifacts);
					TextLanguageStructureGenerator sgen(DecompilerFlagProduceHLSL, (desc.flags & DecompilerFlagHumanReadable) != 0);
					ShaderLanguageFunctionGenerator fgen(DecompilerFlagProduceHLSL, (desc.flags & DecompilerFlagHumanReadable) != 0);
					if (!dctx.ProcessStructures(sgen)) return;
					if (!dctx.ProcessFunctions(fgen, true, artifacts)) return;
				}
				if (desc.flags & DecompilerFlagProduceMSL) {
					DecompilerContext dctx(desc.status, asmdata, artifacts);
					TextLanguageStructureGenerator sgen(DecompilerFlagProduceMSL, (desc.flags & DecompilerFlagHumanReadable) != 0);
					ShaderLanguageFunctionGenerator fgen(DecompilerFlagProduceMSL, (desc.flags & DecompilerFlagHumanReadable) != 0);
					if (!dctx.ProcessStructures(sgen)) return;
					if (!dctx.ProcessFunctions(fgen, false, artifacts)) return;
					DynamicString output;
					if (desc.flags & DecompilerFlagHumanReadable) dctx.MakeMachineGeneratedDisclamer(output);
					output << L"#include <metal_stdlib>\n";
					output << L"#include <simd/simd.h>\n";
					if (desc.flags & DecompilerFlagHumanReadable) output << L"\n";
					output << L"using namespace metal;";
					if (desc.flags & DecompilerFlagHumanReadable) output << L"\n\n";
					for (auto & a : dctx.symbol_cache) output << a.code_inject;
					SafePointer<Artifact> art = new Artifact;
					art->designation = DecompilerFlagProduceMSL;
					art->type = FunctionDesignation::Service;
					art->data = output.ToString().EncodeSequence(Encoding::ANSI, false);
					artifacts.Append(art);
				}
				if (desc.flags & DecompilerFlagProduceGLSL) {
					SetError(desc.status, DecompilerStatus::NotSupported, ShaderLanguage::GLSL);
					return;
				}
				if (desc.flags & DecompilerFlagRawOutput) {
					for (auto & a : artifacts) {
						string postfix, extension;
						SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(a.data->GetBuffer(), a.data->Length());
						if (a.designation == DecompilerFlagProduceHLSL) {
							postfix = a.entry_point_nominal;
							extension = L"hlsl";
						} else if (a.designation == DecompilerFlagProduceMSL) {
							postfix = L"";
							extension = L"metal";
						} else if (a.designation == DecompilerFlagProduceGLSL) {
							postfix = a.entry_point_nominal;
							if (a.type == FunctionDesignation::Vertex) extension = L"vert";
							else if (a.type == FunctionDesignation::Pixel) extension = L"frag";
							else continue;
						} else continue;
						SafePointer<OutputPortion> portion = new OutputPortion(postfix, extension, stream);
						desc.output_objects.Append(portion);
					}
				}
				SafePointer<DataBlock> egsu;
				if (desc.flags & (DecompilerFlagBundleToEGSU | DecompilerFlagBundleToXO | DecompilerFlagBundleToCXX)) {
					SafePointer<Streaming::Stream> egsu_stream = new Streaming::MemoryStream(0x10000);
					SafePointer<Storage::NewArchive> archive = Storage::CreateArchive(egsu_stream, artifacts.Length(), Storage::NewArchiveFlags::UseFormat32);
					Storage::ArchiveFile file = 0;
					for (auto & a : artifacts) {
						uint user = 0;
						string name;
						if (a.designation == DecompilerFlagProduceHLSL) {
							user = 0x10000;
							name = a.entry_point_nominal + L"!" + a.entry_point_effective;
						} else if (a.designation == DecompilerFlagProduceMSL) {
							user = 0x20000;
							name = L"_";
						} else if (a.designation == DecompilerFlagProduceGLSL) {
							user = 0x30000;
							name = a.entry_point_nominal + L"!" + a.entry_point_effective;
						} else continue;
						if (a.type == FunctionDesignation::Vertex) user |= 0x01;
						else if (a.type == FunctionDesignation::Pixel) user |= 0x02;
						file++;
						archive->SetFileName(file, name);
						archive->SetFileCustom(file, user);
						archive->SetFileData(file, a.data->GetBuffer(), a.data->Length());
					}
					archive->Finalize();
					archive.SetReference(0);
					egsu_stream->Seek(0, Streaming::Begin);
					egsu = egsu_stream->ReadAll();
				}
				if (desc.flags & DecompilerFlagBundleToEGSU) {
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(egsu->GetBuffer(), egsu->Length());
					SafePointer<OutputPortion> portion = new OutputPortion(L"", L"egsu", stream);
					desc.output_objects.Append(portion);
				}
				if (desc.flags & DecompilerFlagBundleToXO) {
					LModuleLoader ldr(*desc.callback);
					XL::LContext lctx(asmdata.rootname);
					lctx.AllowBuiltInInlines(true);
					lctx.MakeSubsystemLibrary();
					lctx.SetIdleMode(false);
					lctx.SetVersionInfoMode(desc.flags & DecompilerFlagVersionControl, desc.flags & DecompilerFlagVersionControl);
					if (asmdata.versioninfo.ThisModuleVersion != 0xFFFFFFFF) {
						lctx.SetVersionInfoMode(lctx.IsVersionControlOn(), true);
						lctx.SetVersionInfoVersion(asmdata.versioninfo.ThisModuleVersion);
					}
					if (asmdata.versioninfo.ReplacesVersions.Length()) {
						lctx.SetVersionInfoMode(lctx.IsVersionControlOn(), true);
						for (auto & s : asmdata.versioninfo.ReplacesVersions) lctx.SetVersionInfoSubstitute(s.MustBe, s.Mask);
					}
					SafePointer< Volumes::Dictionary<string, string> > mdata = XI::LoadModuleMetadata(asmdata.resources);
					if (!mdata->IsEmpty()) XI::AddModuleMetadata(lctx.QueryResources(), *mdata);
					lctx.QueryResources().Append(XI::MakeResourceID(L"XW", 1), egsu);
					if (!ldr.LoadModule(L"canonicalis", lctx, desc.status)) return;
					if (!ldr.LoadModule(L"graphicum", lctx, desc.status)) return;
					if (!ldr.LoadModule(L"mathvec", lctx, desc.status)) return;
					if (!ldr.LoadModule(L"matrices", lctx, desc.status)) return;
					DecompilerContext dctx(desc.status, asmdata, artifacts);
					ObjectStructureGenerator sgen(lctx);
					ObjectFunctionGenerator fgen(lctx);
					if (!dctx.ProcessStructures(sgen)) return;
					if (!dctx.ProcessFunctions(fgen, false, artifacts)) return;
					auto ns = lctx.GetRootNamespace();
					ns = lctx.CreateNamespace(ns, L"xw");
					ns = lctx.CreateNamespace(ns, asmdata.rootname);
					SafePointer<XL::XType> device_ptr = XL::CreateType(L"PCgraphicum.machinatio", lctx);
					SafePointer<XL::XType> library_sptr = XL::CreateType(L"C@praeformae.I0(Cadl)(Cgraphicum.liber_functionum)", lctx);
					auto fd = lctx.CreateFunction(ns, L"compila_functiones");
					auto func = lctx.CreateFunctionOverload(fd, library_sptr, 1, reinterpret_cast<XL::LObject **>(device_ptr.InnerRef()), XL::FunctionThrows);
					XL::FunctionContextDesc fdesc;
					string aname = L"A";
					fdesc.retval = library_sptr;
					fdesc.instance = 0;
					fdesc.argc = 1;
					fdesc.argvt = reinterpret_cast<XL::LObject **>(device_ptr.InnerRef());
					fdesc.argvn = &aname;
					fdesc.flags = XL::FunctionThrows;
					fdesc.vft_init = 0;
					fdesc.vft_init_seq = 0;
					fdesc.create_init_sequence = fdesc.create_shutdown_sequence = false;
					fdesc.init_callback = 0;
					SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(lctx, func, fdesc);
					SafePointer<XL::LObject> arg = fctx->GetRootScope()->GetMember(aname);
					SafePointer<XL::LObject> lit_xw = lctx.QueryLiteral(string(L"XW"));
					SafePointer<XL::LObject> lit_unus = lctx.QueryLiteral(1, 4, true);
					SafePointer<XL::LObject> mdl = lctx.QueryModuleOperator(asmdata.rootname);
					SafePointer<XL::LObject> para_auxilium = lctx.QueryObject(L"para_auxilium");
					XL::LObject * argv[3];
					argv[0] = mdl;
					argv[1] = lit_xw;
					argv[2] = lit_unus;
					SafePointer<XL::LObject> aux = para_auxilium->Invoke(3, argv);
					arg = arg->GetMember(XL::OperatorFollow);
					arg = arg->Invoke(0, 0);
					arg = arg->GetMember(L"compila_functiones");
					arg = arg->Invoke(1, aux.InnerRef());
					fctx->EncodeReturn(arg);
					fctx->EndEncoding();
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(0x4000);
					lctx.ProduceModule(L"XW", ENGINE_VI_VERSIONMAJOR, ENGINE_VI_VERSIONMINOR, ENGINE_VI_SUBVERSION, ENGINE_VI_BUILD, stream);
					stream->Seek(0, Streaming::Begin);
					SafePointer<OutputPortion> portion = new OutputPortion(L"", L"xo", stream);
					desc.output_objects.Append(portion);
				}
				if (desc.flags & DecompilerFlagProduceCXX) {
					DecompilerContext dctx(desc.status, asmdata, artifacts);
					TextLanguageStructureGenerator sgen(DecompilerFlagProduceCXX, (desc.flags & DecompilerFlagHumanReadableC) != 0);
					CLanguageFunctionGenerator fgen((desc.flags & DecompilerFlagHumanReadableC) != 0);
					if (!dctx.ProcessStructures(sgen)) return;
					if (!dctx.ProcessFunctions(fgen, false, artifacts)) return;
					DynamicString output;
					if (desc.flags & DecompilerFlagHumanReadableC) dctx.MakeMachineGeneratedDisclamer(output);
					output << L"#pragma once\n";
					output << L"#include <EngineRuntime.h>\n";
					if (desc.flags & DecompilerFlagHumanReadableC) output << L"\n";
					for (auto & a : dctx.symbol_cache) output << a.code_inject;
					if (desc.flags & DecompilerFlagBundleToCXX) {
						bool hr = (desc.flags & DecompilerFlagHumanReadableC);
						output << L"namespace Engine";
						if (hr) output << L' ';
						output << L'{';
						if (hr) output << L"\n\t";
						output << L"namespace XW";
						if (hr) output << L' ';
						output << L'{';
						if (hr) output << L"\n\t\t";
						output << L"namespace " << asmdata.rootname;
						if (hr) output << L' ';
						output << L'{';
						if (hr) output << L"\n\t\t\t";
						output << L"Engine::Graphics::IShaderLibrary * CompileFunctions(Engine::Graphics::IDevice * device, Engine::Graphics::ShaderError * error) noexcept;";
						if (hr) output << L"\n\t\t";
						output << L'}';
						if (hr) output << L"\n\t";
						output << L'}';
						if (hr) output << L'\n';
						output << L'}';
						if (hr) output << L'\n';
					}
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(0x1000);
					SafePointer<Streaming::TextWriter> wri = new Streaming::TextWriter(stream, Encoding::UTF8);
					wri->WriteEncodingSignature();
					wri->Write(output.ToString());
					wri.SetReference(0);
					stream->Seek(0, Streaming::Begin);
					SafePointer<OutputPortion> portion = new OutputPortion(L"", L"h", stream);
					desc.output_objects.Append(portion);
				}
				if (desc.flags & DecompilerFlagBundleToCXX) {
					DecompilerContext dctx(desc.status, asmdata, artifacts);
					DynamicString output;
					if (desc.flags & DecompilerFlagHumanReadableC) dctx.MakeMachineGeneratedDisclamer(output);
					output << L"#include <EngineRuntime.h>\n";
					if (desc.flags & DecompilerFlagHumanReadableC) output << L"\n";
					bool hr = (desc.flags & DecompilerFlagHumanReadableC);
					output << L"namespace Engine";
					if (hr) output << L' ';
					output << L'{';
					if (hr) output << L"\n\t";
					output << L"namespace XW";
					if (hr) output << L' ';
					output << L'{';
					if (hr) output << L"\n\t\t";
					output << L"namespace " << asmdata.rootname;
					if (hr) output << L' ';
					output << L'{';
					if (hr) output << L"\n\t\t\t";
					output << L"const uint8 egsu[] = {";
					for (int i = 0; i < egsu->Length(); i++) {
						if (hr) {
							if ((i % 32) == 0) output << L"\n\t\t\t\t";
							else output << L' ';
						}
						output << L"0x" << string(uint(egsu->ElementAt(i)), HexadecimalBase, 2);
						if (i + 1 < egsu->Length()) output << L',';
					}
					if (hr) output << L"\n\t\t\t";
					output << L"};";
					if (hr) output << L"\n\t\t\t";
					output << L"Engine::Graphics::IShaderLibrary * CompileFunctions(Engine::Graphics::IDevice * device, Engine::Graphics::ShaderError * error) noexcept";
					if (hr) output << L"\n\t\t\t";
					output << L"{";
					if (hr) output << L"\n\t\t\t\t";
					output << L"return device->CompileShaderLibrary(egsu, sizeof(egsu), error);";
					if (hr) output << L"\n\t\t\t";
					output << L"}";
					if (hr) output << L"\n\t\t";
					output << L'}';
					if (hr) output << L"\n\t";
					output << L'}';
					if (hr) output << L'\n';
					output << L'}';
					if (hr) output << L'\n';
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(0x1000);
					SafePointer<Streaming::TextWriter> wri = new Streaming::TextWriter(stream, Encoding::UTF8);
					wri->WriteEncodingSignature();
					wri->Write(output.ToString());
					wri.SetReference(0);
					stream->Seek(0, Streaming::Begin);
					SafePointer<OutputPortion> portion = new OutputPortion(L"", L"cxx", stream);
					desc.output_objects.Append(portion);
				}
				SetError(desc.status, DecompilerStatus::Success);
			} catch (...) {
				SetError(desc.status, DecompilerStatus::InternalError);
				desc.output_objects.Clear();
			}
		}
	}
}