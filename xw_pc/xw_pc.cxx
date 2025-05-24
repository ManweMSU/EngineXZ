#include <EngineRuntime.h>

#include "../xexec/xx_com.h"
#include "xw_dx_com.h"
#include "xw_metal_com.h"
#include "xw_vk_com.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

struct {
	SafePointer<StringTable> localization;
	SafePointer<Registry> profile;
	bool silent = false;
	bool nologo = false;
	string logfile;
	string input;
	string output;
	string output_path;
} state;

string Localized(int id)
{
	try {
		if (!state.localization) throw InvalidStateException();
		return state.localization->GetString(id);
	} catch (...) { return FormatString(L"LOC(%0)", id); }
}
string ErrorDescription(XW::PrecompilationStatus status)
{
	try {
		if (!state.localization) throw InvalidStateException();
		return state.localization->GetString(700 + int(status));
	} catch (...) { return Localized(302); }
}
void ProcessCommandLine(Console & console)
{
	SafePointer< Array<string> > args = GetCommandLine();
	for (int i = 1; i < args->Length(); i++) {
		auto & arg = args->ElementAt(i);
		if (arg[0] == L':' || arg[0] == L'-') {
			for (int j = 1; j < arg.Length(); j++) {
				if (arg[j] == L'N') {
					state.nologo = true;
				} else if (arg[j] == L'O') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.output.Length() || state.output_path.Length()) {
						console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.output_path = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'S') {
					state.silent = true;
				} else if (arg[j] == L'a') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.logfile.Length()) {
						console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.logfile = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'm') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					try {
						state.profile = XX::LoadConfiguration(ExpandPath(args->ElementAt(i)));
					} catch (...) {
						console << TextColor(12) << Localized(206) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'o') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.output.Length() || state.output_path.Length()) {
						console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.output = ExpandPath(args->ElementAt(i));
				} else {
					console << TextColor(12) << Localized(204) << TextColorDefault() << LineFeed();
					throw Exception();
				}
			}
		} else {
			if (state.input.Length()) {
				console << TextColor(12) << Localized(201) << TextColorDefault() << LineFeed();
				throw Exception();
			}
			state.input = ExpandPath(arg);
		}
	}
}
void ReadDuplexVersion(RegistryNode * node, const string & value, uint & major, uint & minor)
{
	auto parts = node->GetValueString(value).Split(L'.');
	if (parts.Length() > 0) major = parts[0].ToUInt32(); else major = 0;
	if (parts.Length() > 1) minor = parts[1].ToUInt32(); else minor = 0;
}

int Main(void)
{
	handle _con_in, _con_out;
	try {
		_con_in = CloneHandle(GetStandardInput());
		_con_out = CloneHandle(GetStandardOutput());
	} catch (...) { return 0x40; }
	Console console(_con_out, _con_in);
	try {
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> xw_conf = XX::LoadConfiguration(root + L"/xw.ini");
		state.profile = xw_conf;
		auto language_override = xw_conf->GetValueString(L"Lingua");
		if (language_override.Length()) Assembly::CurrentLocale = language_override;
		auto localizations = xw_conf->GetValueString(L"Locale");
		if (localizations.Length()) {
			try {
				SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + Assembly::CurrentLocale + L".ecst", AccessRead, OpenExisting);
				state.localization = new StringTable(table);
			} catch (...) {}
			if (!state.localization) {
				auto language_default = xw_conf->GetValueString(L"LinguaDefalta");
				try {
					SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + language_default + L".ecst", AccessRead, OpenExisting);
					state.localization = new StringTable(table);
				} catch (...) {}
			}
		}
		ProcessCommandLine(console);
	} catch (...) { return 0x40; }
	if (!state.nologo && !state.silent) {
		console << Localized(4) << LineFeedSequence;
		console << Localized(2) << LineFeedSequence;
		console << FormatString(Localized(3), ENGINE_VI_APPVERSION) << LineFeedSequence << LineFeedSequence;
	}
	if (state.input.Length()) {
		try {
			XW::DXProfileDesc dx_profile;
			XW::MetalProfileDesc metal_profile;
			XW::VulkanProfileDesc vulkan_profile;
			dx_profile.vertex_shader_profile = state.profile->GetValueString(L"HLSL/ModusVerticis");
			dx_profile.pixel_shader_profile = state.profile->GetValueString(L"HLSL/ModusPuncti");
			metal_profile.metal_language_version = state.profile->GetValueString(L"MSL/VersioLinguae");
			metal_profile.target_system = state.profile->GetValueString(L"MSL/Destinatio");
			ReadDuplexVersion(state.profile, L"GLSL/VersioVulkani", vulkan_profile.vulkan_version_major, vulkan_profile.vulkan_version_minor);
			ReadDuplexVersion(state.profile, L"GLSL/VersioSPIRV", vulkan_profile.spirv_version_major, vulkan_profile.spirv_version_minor);
			ReadDuplexVersion(state.profile, L"GLSL/VersioLinguae", vulkan_profile.glsl_version_major, vulkan_profile.glsl_version_minor);
			if (state.output_path.Length()) {
				state.output = ExpandPath(state.output_path + L"/" + Path::GetFileNameWithoutExtension(state.input) + L".egso");
			} else if (!state.output.Length()) {
				state.output = ExpandPath(Path::GetDirectory(state.input) + L"/" + Path::GetFileNameWithoutExtension(state.input) + L".egso");
			}
			XW::PrecompilationStatus status = XW::PrecompilationStatus::NoPlatformCompiler;
			#if defined(ENGINE_WINDOWS)
			status = XW::DXCompile(state.input, state.output, state.logfile, dx_profile);
			#elif defined(ENGINE_MACOSX)
			status = XW::MetalCompile(state.input, state.output, state.logfile, metal_profile);
			#elif defined(ENGINE_LINUX)
			status = XW::VulkanCompile(state.input, state.output, state.logfile, vulkan_profile);
			#endif
			if (status != XW::PrecompilationStatus::Success) {
				if (!state.silent) {
					console.SetTextColor(12);
					console.WriteLine(FormatString(Localized(506), ErrorDescription(status)));
					console.SetTextColor(-1);
				}
				return int(status);
			}
			return 0;
		} catch (...) { return 0x3F; }
	} else {
		if (!state.silent) try {
			auto length = Localized(600).ToInt32();
			for (int i = 0; i < length; i++) console << Localized(601 + i) << LineFeedSequence;
		} catch (...) {}
		return 0;
	}
}