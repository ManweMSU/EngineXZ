﻿#include <EngineRuntime.h>

#include "../xasm/xa_dasm.h"
#include "../ximg/xi_function.h"
#include "../xw/xw_types.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

constexpr uint ExtractListWithPrefix	= 0x001;
constexpr uint ExtractCodeArchitecture	= 0x002;
constexpr uint ExtractFunctionTraits	= 0x004;
constexpr uint ExtractAssembler			= 0x008;

struct {
	SafePointer<StringTable> localization;
	bool nologo = false;
	string input, symbol;
	uint extract_mask = 0;
} state;

Storage::Registry * LoadConfiguration(const string & from_file)
{
	SafePointer<Streaming::Stream> conf = new Streaming::FileStream(from_file, Streaming::AccessRead, Streaming::OpenExisting);
	try {
		SafePointer<Storage::Registry> result = Storage::LoadRegistry(conf);
		if (!result) throw InvalidFormatException();
		result->Retain();
		return result;
	} catch (...) {}
	conf->Seek(0, Streaming::Begin);
	SafePointer<Storage::Registry> result = Storage::CompileTextRegistry(conf);
	if (!result) throw InvalidFormatException();
	result->Retain();
	return result;
}
string Localized(int id)
{
	try {
		if (!state.localization) throw InvalidStateException();
		return state.localization->GetString(id);
	} catch (...) { return FormatString(L"LOC(%0)", id); }
}
void ProcessCommandLine(void)
{
	SafePointer< Array<string> > args = GetCommandLine();
	for (int i = 1; i < args->Length(); i++) {
		auto & arg = args->ElementAt(i);
		if (arg[0] == L':' || arg[0] == L'-') {
			for (int j = 1; j < arg.Length(); j++) {
				if (arg[j] == L'N') {
					state.nologo = true;
				} else if (arg[j] == L'a') {
					state.extract_mask |= ExtractCodeArchitecture;
				} else if (arg[j] == L'd') {
					state.extract_mask |= ExtractAssembler;
				} else if (arg[j] == L'e') {
					state.extract_mask |= ExtractListWithPrefix;
				} else if (arg[j] == L't') {
					state.extract_mask |= ExtractFunctionTraits;
				} else {
					console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
					throw Exception();
				}
			}
		} else {
			if (state.input.Length()) {
				if (state.symbol.Length()) {
					console << TextColor(12) << Localized(201) << TextColorDefault() << LineFeed();
					throw Exception();
				} else state.symbol = arg;
			} else state.input = ExpandPath(arg);
		}
	}
}
void ListFunctions(XI::Module::Class & cls, const string & clsname, Volumes::Dictionary<string, XI::Module::Function *> & dest)
{
	for (auto & f : cls.methods) {
		auto name = clsname + L"." + f.key;
		if (state.extract_mask & ExtractListWithPrefix) {
			if (name.Fragment(0, state.symbol.Length()) == state.symbol) dest.Append(name, &f.value);
		} else {
			if (name == state.symbol) dest.Append(name, &f.value);
		}
	}
}
Volumes::Dictionary<string, XI::Module::Function *> * ListFunctions(XI::Module & module)
{
	SafePointer< Volumes::Dictionary<string, XI::Module::Function *> > result = new Volumes::Dictionary<string, XI::Module::Function *>;
	for (auto & f : module.functions) {
		auto name = f.key;
		if (state.extract_mask & ExtractListWithPrefix) {
			if (name.Fragment(0, state.symbol.Length()) == state.symbol) result->Append(name, &f.value);
		} else {
			if (name == state.symbol) result->Append(name, &f.value);
		}
	}
	for (auto & c : module.classes) ListFunctions(c.value, c.key, *result);
	result->Retain();
	return result;
}
class FunctionLoader : public XI::IFunctionLoader
{
public:
	XA::Function xa;
	string symbol, library;
	bool invalid, is_xa;
public:
	FunctionLoader(void) : invalid(false), is_xa(false) {}
	virtual Platform GetArchitecture(void) noexcept override { return Platform::Unknown; }
	virtual XA::Environment GetEnvironment(void) noexcept override { return XA::Environment::Unknown; }
	virtual void HandleAbstractFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override { try { xa.Load(fout); is_xa = true; } catch (...) { invalid = true; } }
	virtual void HandlePlatformFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override {}
	virtual void HandleNearImport(const string & symbol, const XI::Module::Function & fin, const string & func_name) noexcept override { try { this->symbol = func_name; } catch (...) { invalid = true; } }
	virtual void HandleFarImport(const string & symbol, const XI::Module::Function & fin, const string & func_name, const string & lib_name) noexcept override { try { this->symbol = func_name; this->library = lib_name; } catch (...) { invalid = true; } }
	virtual void HandleLoadError(const string & symbol, const XI::Module::Function & fin, XI::LoadFunctionError error) noexcept override { invalid = true; }
};
class FunctionLoaderW : public XW::IFunctionLoader
{
	XW::ShaderLanguage _lang;
public:
	XA::Function xa;
	XW::TranslationRules xw;
	bool invalid, is_xa, is_xw;
public:
	FunctionLoaderW(XW::ShaderLanguage lang) : _lang(lang), invalid(false), is_xa(false), is_xw(false) {}
	virtual XW::ShaderLanguage GetLanguage(void) noexcept override { return _lang; }
	virtual void HandleFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override { try { xa.Load(fout); is_xa = true; } catch (...) { invalid = true; } }
	virtual void HandleRule(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override { try { xw.Load(fout); is_xw = true; } catch (...) { invalid = true; } }
	virtual void HandleLoadError(const string & symbol, const XI::Module::Function & fin, XI::LoadFunctionError error) noexcept override { invalid = true; }
};

int Main(void)
{
	try {
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> xi_conf = LoadConfiguration(root + L"/xda.ini");
		auto language_override = xi_conf->GetValueString(L"Lingua");
		if (language_override.Length()) Assembly::CurrentLocale = language_override;
		auto localizations = xi_conf->GetValueString(L"Locale");
		if (localizations.Length()) {
			try {
				SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + Assembly::CurrentLocale + L".ecst", AccessRead, OpenExisting);
				state.localization = new StringTable(table);
			} catch (...) {}
			if (!state.localization) {
				auto language_default = xi_conf->GetValueString(L"LinguaDefalta");
				try {
					SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + language_default + L".ecst", AccessRead, OpenExisting);
					state.localization = new StringTable(table);
				} catch (...) {}
			}
		}
		ProcessCommandLine();
	} catch (...) { return 0x40; }
	if (!state.nologo) {
		console << Localized(1) << LineFeedSequence;
		console << Localized(2) << LineFeedSequence;
		console << FormatString(Localized(3), ENGINE_VI_APPVERSION) << LineFeedSequence << LineFeedSequence;
	}
	if (state.input.Length()) {
		try {
			SafePointer<XI::Module> module;
			try {
				SafePointer<Stream> stream = new FileStream(state.input, AccessRead, OpenExisting);
				try { module = new XI::Module(stream); } catch (...) {
					console << TextColor(12) << Localized(302) << TextColorDefault() << LineFeed();
					return 2;
				}
			} catch (...) {
				console << TextColor(12) << Localized(301) << TextColorDefault() << LineFeed();
				return 1;
			}
			SafePointer< Volumes::Dictionary<string, XI::Module::Function *> > funcs = ListFunctions(*module);
			if (funcs->IsEmpty()) {
				console << TextColor(12) << Localized(303) << TextColorDefault() << LineFeed();
				return 3;
			}
			for (auto & f : funcs->Elements()) {
				FunctionLoader fldr;
				FunctionLoaderW fldrw(XW::ShaderLanguage::Unknown);
				SafePointer< Array<uint32> > fabi;
				SafePointer< Array<XW::ShaderLanguage> > flang;
				XI::LoadFunction(f.key, *f.value, &fldr);
				XW::LoadFunction(f.key, *f.value, &fldrw);
				fabi = XI::LoadFunctionABI(*f.value);
				flang = XW::LoadRuleVariants(*f.value);
				console << TextColor(11) << FormatString(Localized(401), f.key) << TextColorDefault() << LineFeed();
				if (state.extract_mask & ExtractCodeArchitecture) {
					console << TextColor(14);
					if (!fldr.invalid) {
						if (fldr.is_xa) console << L"  " << Localized(402) << LineFeed();
						else if (fldr.library.Length()) console << L"  " << FormatString(Localized(405), fldr.symbol, fldr.library) << LineFeed();
						else if (fldr.symbol.Length()) console << L"  " << FormatString(Localized(404), fldr.symbol) << LineFeed();
						else if (fldrw.is_xa) console << L"  " << Localized(419) << LineFeed();
					}
					for (auto & abi : fabi->Elements()) {
						Platform arch;
						XA::Environment osenv;
						XI::ReadFunctionABI(abi, arch, osenv);
						string text_arch, text_os;
						if (arch == Platform::X86) text_arch = Localized(601);
						else if (arch == Platform::X64) text_arch = Localized(602);
						else if (arch == Platform::ARM) text_arch = Localized(603);
						else if (arch == Platform::ARM64) text_arch = Localized(604);
						else text_arch = Localized(600);
						if (osenv == XA::Environment::Windows)		text_os = Localized(501);
						else if (osenv == XA::Environment::MacOSX)	text_os = Localized(502);
						else if (osenv == XA::Environment::Linux)	text_os = Localized(503);
						else if (osenv == XA::Environment::EFI)		text_os = Localized(504);
						else if (osenv == XA::Environment::XSO)		text_os = Localized(505);
						else text_os = Localized(500);
						console << L"  " << FormatString(Localized(403), text_os, text_arch) << LineFeed();
					}
					for (auto & lang : flang->Elements()) {
						string text_lang;
						if (lang == XW::ShaderLanguage::HLSL) text_lang = Localized(701);
						else if (lang == XW::ShaderLanguage::MSL) text_lang = Localized(702);
						else if (lang == XW::ShaderLanguage::GLSL) text_lang = Localized(703);
						else text_lang = Localized(700);
						console << L"  " << FormatString(Localized(418), text_lang) << LineFeed();
					}
					console << TextColorDefault();
				}
				if (state.extract_mask & ExtractFunctionTraits) {
					console << TextColor(13);
					if (f.value->code_flags & XI::Module::Function::FunctionEntryPoint) console << L"  " << Localized(406) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::FunctionInitialize) console << L"  " << Localized(407) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::FunctionShutdown) console << L"  " << Localized(408) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::FunctionInstance) console << L"  " << Localized(409) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::FunctionThrows) console << L"  " << Localized(410) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::FunctionThisCall) console << L"  " << Localized(411) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::FunctionPrototype) console << L"  " << Localized(412) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::FunctionInline) console << L"  " << Localized(413) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::ConvertorExpanding) console << L"  " << Localized(414) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::ConvertorNarrowing) console << L"  " << Localized(415) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::ConvertorExpensive) console << L"  " << Localized(416) << LineFeed();
					if (f.value->code_flags & XI::Module::Function::ConvertorSimilar) console << L"  " << Localized(417) << LineFeed();
					console << TextColorDefault();
				}
				if (state.extract_mask & ExtractAssembler) {
					if (fldr.is_xa && !fldr.invalid) {
						console << LineFeed() << TextColor(10);
						XA::DisassemblyFunction(fldr.xa, &console);
						console << TextColorDefault();
					} else if (fldrw.is_xa && !fldrw.invalid) {
						for (int i = fldrw.xa.instset.Length() - 1; i >= 0; i--) {
							auto & inst = fldrw.xa.instset[i];
							if (inst.opcode >= 0x1000) inst.opcode = XA::OpcodeNOP;
						}
						console << LineFeed() << TextColor(10);
						XA::DisassemblyFunction(fldrw.xa, &console);
						console << TextColorDefault();
					} else if (flang->Length()) {
						console << LineFeed();
						for (auto & lang : flang->Elements()) {
							FunctionLoaderW fldrw2(lang);
							XW::LoadFunction(f.key, *f.value, &fldrw2);
							if (fldrw2.is_xw && !fldrw2.invalid) {
								string text_lang;
								if (lang == XW::ShaderLanguage::HLSL) text_lang = Localized(701);
								else if (lang == XW::ShaderLanguage::MSL) text_lang = Localized(702);
								else if (lang == XW::ShaderLanguage::GLSL) text_lang = Localized(703);
								else text_lang = Localized(700);
								console << TextColor(11);
								console << L"  " << FormatString(Localized(418), text_lang) << L":" << LineFeed();
								for (auto & e : fldrw2.xw.extrefs) {
									console << TextColor(10) << L"    EXTERNAL " << TextColorDefault() << L"\'" << TextColor(10) << e << TextColorDefault() << L"\'" << LineFeed();
								}
								for (auto & b : fldrw2.xw.blocks) {
									if (b.rule == XW::Rule::InsertString) {
										console << TextColor(10) << L"    --> " << TextColorDefault() << L"\'" << TextColor(10) << b.text << TextColorDefault() << L"\'" << LineFeed();
									} else if (b.rule == XW::Rule::InsertArgument) {
										console << TextColor(10) << L"    --> A[" << b.index << L"]" << LineFeed();
									} else if (b.rule == XW::Rule::InsertReference) {
										console << TextColor(10) << L"    --> E[" << b.index << L"]" << LineFeed();
									}
								}
								console << TextColorDefault();
							}
						}
					} else console << TextColor(12) << Localized(304) << TextColorDefault() << LineFeed();
				}
			}
			return 0;
		} catch (...) { return 0x3F; }
	} else {
		try {
			auto length = Localized(100).ToInt32();
			for (int i = 0; i < length; i++) console << Localized(101 + i) << LineFeedSequence;
		} catch (...) {}
		return 0;
	}
}