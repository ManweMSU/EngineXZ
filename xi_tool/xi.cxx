#include <EngineRuntime.h>

#include "../ximg/xi_resources.h"
#include "../ximg/xi_pret.h"
#include "../xexec/xx_com.h"
#include "../xenv_sec/xe_sec_core.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

constexpr uint InspectSymbols		= 0x001;
constexpr uint InspectDependencies	= 0x002;
constexpr uint InspectModuleStamp	= 0x004;
constexpr uint InspectMetadata		= 0x008;
constexpr uint InspectIcon			= 0x010;
constexpr uint InspectResources		= 0x020;
constexpr uint InspectVersions		= 0x040;

struct {
	SafePointer<StringTable> localization;
	string store_integration;
	XX::SecuritySettings security;
	bool silent = false;
	bool nologo = false;
	bool print_security_state = false;
	bool validate_security = false;
	uint inspect_mask = 0;
	string input;
	string output;
	string identity_file;
	string identity_password;
	Array<XI::PretranslateDesc> pret_list = Array<XI::PretranslateDesc>(0x10);
} state;

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
				} else if (arg[j] == L'S') {
					state.silent = true;
				} else if (arg[j] == L'f') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & arg2 = args->ElementAt(i);
					for (int k = 0; k < arg2.Length(); k++) {
						if (arg2[k] == L'i') {
							state.print_security_state = true;
						} else if (arg2[k] == L's') {
							i++; if (i >= args->Length()) {
								console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
								throw Exception();
							}
							if (args->ElementAt(i) == L"-") state.identity_file = L"-";
							else state.identity_file = ExpandPath(args->ElementAt(i));
						} else if (arg2[k] == L'v') {
							state.validate_security = true;
						} else {
							console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
							throw Exception();
						}
					}
				} else if (arg[j] == L'i') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & arg2 = args->ElementAt(i);
					for (int k = 0; k < arg2.Length(); k++) {
						if (arg2[k] == L's') state.inspect_mask |= InspectSymbols;
						else if (arg2[k] == L'd') state.inspect_mask |= InspectDependencies;
						else if (arg2[k] == L't') state.inspect_mask |= InspectModuleStamp;
						else if (arg2[k] == L'm') state.inspect_mask |= InspectMetadata;
						else if (arg2[k] == L'i') state.inspect_mask |= InspectIcon;
						else if (arg2[k] == L'r') state.inspect_mask |= InspectResources;
						else if (arg2[k] == L'v') state.inspect_mask |= InspectVersions; else {
							console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
							throw Exception();
						}
					}
				} else if (arg[j] == L'o') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.output.Length()) {
						console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.output = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'p') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.identity_password = args->ElementAt(i);
				} else if (arg[j] == L't') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & arg2 = args->ElementAt(i);
					auto prt = arg2.Split(L'-');
					XI::PretranslateDesc desc;
					if (prt.Length() != 2) {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (prt[0] == L"win")		desc.osenv = XA::Environment::Windows;
					else if (prt[0] == L"mac")	desc.osenv = XA::Environment::MacOSX;
					else if (prt[0] == L"lnx")	desc.osenv = XA::Environment::Linux;
					else if (prt[0] == L"efi")	desc.osenv = XA::Environment::EFI;
					else if (prt[0] == L"xso")	desc.osenv = XA::Environment::XSO;
					else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (prt[1] == L"x86") desc.arch = Platform::X86;
					else if (prt[1] == L"x64") desc.arch = Platform::X64;
					else if (prt[1] == L"arm") desc.arch = Platform::ARM;
					else if (prt[1] == L"arm64") desc.arch = Platform::ARM64; else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.pret_list << desc;
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
void TableView(const string ** columns, int num_cols, int num_items, const int * colors = 0)
{
	Array<int> widths(1);
	widths.SetLength(num_cols);
	ZeroMemory(widths.GetBuffer(), widths.Length() * sizeof(int));
	for (int i = 0; i < num_items; i++) {
		for (int c = 0; c < num_cols; c++) {
			int len = columns[c][i].Length();
			if (len > widths[c]) widths[c] = len;
		}
	}
	for (int i = 0; i < num_items; i++) {
		if (colors) console.SetTextColor(colors[i]);
		for (int c = 0; c < num_cols; c++) {
			int len = columns[c][i].Length();
			console << columns[c][i];
			if (c == num_cols - 1) console << LineFeed();
			else console << string(L' ', widths[c] + 1 - len);
		}
	}
}
void ListCertificates(const string & at)
{
	SafePointer< Array<string> > files = Search::GetFiles(at + L"/*." + XE::Security::FileExtensions::Certificate);
	Array<string> names(0x100);
	Array<string> holders(0x100);
	for (auto & f : files->Elements()) try {
		SafePointer<Stream> data = new FileStream(at + L"/" + f, AccessRead, OpenExisting);
		SafePointer<XE::Security::IContainer> store = XE::Security::LoadContainer(data);
		if (!store || store->GetContainerClass() != XE::Security::ContainerClass::Certificate) continue;
		SafePointer<XE::Security::ICertificate> cert = store->LoadCertificate(0);
		if (!cert) continue;
		auto & desc = cert->GetDescription();
		names << f;
		if (desc.PersonName.Length() && desc.Organization.Length()) {
			holders << (L": " + desc.PersonName + L" / " + desc.Organization);
		} else if (desc.PersonName.Length()) {
			holders << (L": " + desc.PersonName);
		} else {
			holders << (L": " + desc.Organization);
		}
	} catch (...) {}
	const string * table[] = { names.GetBuffer(), holders.GetBuffer() };
	TableView(table, 2, names.Length());
}
bool CreateSecurityData(Stream * dest, XE::Security::IIdentity * identity) noexcept
{
	try {
		SafePointer<DataBlock> data = XI::ReadConsistencyData(dest);
		SafePointer<DataBlock> hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, data);
		if (!hash) return false;
		SafePointer<XE::Security::IContainer> addendum = identity ? XE::Security::CreateSignatureData(hash, identity) : XE::Security::CreateIntegrityData(hash);
		if (!addendum) return false;
		SafePointer<DataBlock> signature = addendum->LoadContainerRepresentation();
		if (!signature) return false;
		dest->WriteArray(signature);
		return true;
	} catch (...) { return false; }
}
string IntegrityStatusToString(XE::Security::IntegrityStatus status)
{
	if (status == XE::Security::IntegrityStatus::OK) return Localized(521);
	else if (status == XE::Security::IntegrityStatus::DataCorruption) return Localized(522);
	else if (status == XE::Security::IntegrityStatus::DataSurrogation) return Localized(523);
	else if (status == XE::Security::IntegrityStatus::Expired) return Localized(524);
	else if (status == XE::Security::IntegrityStatus::NotIntroduced) return Localized(525);
	else if (status == XE::Security::IntegrityStatus::InvalidUsage) return Localized(526);
	else if (status == XE::Security::IntegrityStatus::InvalidDerivation) return Localized(527);
	else if (status == XE::Security::IntegrityStatus::Compromised) return Localized(528);
	else if (status == XE::Security::IntegrityStatus::NoTrustInChain) return Localized(529);
	else return Localized(530);
}
int IntegrityStatusToColor(XE::Security::IntegrityStatus status)
{
	if (status == XE::Security::IntegrityStatus::OK) return 10;
	else if (status == XE::Security::IntegrityStatus::NoTrustInChain) return 14;
	else return 12;
}

int Main(void)
{
	try {
		Codec::InitializeDefaultCodecs();
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> xi_conf = XX::LoadConfiguration(root + L"/xi.ini");
		state.store_integration = xi_conf->GetValueString(L"Entheca");
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
	if (!state.nologo && !state.silent) {
		console << Localized(1) << LineFeedSequence;
		console << Localized(2) << LineFeedSequence;
		console << FormatString(Localized(3), ENGINE_VI_APPVERSION) << LineFeedSequence << LineFeedSequence;
	}
	if (state.validate_security || state.print_security_state) {
		try {
			auto xe = XX::LocateEnvironmentConfiguration(Path::GetDirectory(GetExecutablePath()) + L"/" + state.store_integration);
			XX::LoadSecuritySettings(state.security, xe);
			state.security.TrustedCertificates = ExpandPath(Path::GetDirectory(xe) + L"/" + state.security.TrustedCertificates);
			state.security.UntrustedCertificates = ExpandPath(Path::GetDirectory(xe) + L"/" + state.security.UntrustedCertificates);
		} catch (...) {
			state.security.ValidateTrust = false;
			state.security.TrustedCertificates = state.security.UntrustedCertificates = L"";
		}
	}
	if (state.print_security_state && !state.silent) {
		if (state.security.ValidateTrust) {
			console.SetTextColor(10);
			console.WriteLine(Localized(501));
			console.SetTextColor(-1);
		} else {
			console.SetTextColor(14);
			console.WriteLine(Localized(502));
			console.SetTextColor(-1);
		}
		console.SetTextColor(10);
		console.WriteLine(Localized(503));
		ListCertificates(state.security.TrustedCertificates);
		console.SetTextColor(-1);
		console.SetTextColor(12);
		console.WriteLine(Localized(504));
		ListCertificates(state.security.UntrustedCertificates);
		console.SetTextColor(-1);
	}
	if (state.input.Length()) {
		SafePointer<XE::Security::IIdentity> identity;
		if (state.identity_file.Length() && state.identity_file != L"-") {
			if (!state.identity_password.Length() && !state.silent) {
				DynamicString psw;
				console.Write(Localized(505));
				console.SetInputMode(ConsoleInputMode::Raw);
				while (true) {
					auto chr = console.ReadChar();
					if (chr == 0xFFFFFFFF || chr == L'\n' || chr == L'\r') break;
					psw << chr;
				}
				console.SetInputMode(ConsoleInputMode::Echo);
				console.LineFeed();
				state.identity_password = psw.ToString();
			}
			try {
				SafePointer<Stream> data = new FileStream(state.identity_file, AccessRead, OpenExisting);
				SafePointer<XE::Security::IContainer> store = XE::Security::LoadContainer(data);
				if (!store || store->GetContainerClass() != XE::Security::ContainerClass::Identity) throw InvalidFormatException();
				identity = XE::Security::LoadIdentity(store, 0, state.identity_password);
				if (!identity) throw InvalidFormatException();
			} catch (...) {
				if (!state.silent) console << TextColor(12) << Localized(506) << TextColorDefault() << LineFeed();
				return 1;
			}
		}
		try {
			SafePointer<XI::Module> module;
			SafePointer<DataBlock> module_data;
			SafePointer<XE::Security::IContainer> module_signature;
			try {
				SafePointer<Stream> stream = new FileStream(state.input, AccessRead, OpenExisting);
				try {
					module = new XI::Module(stream);
					if (state.validate_security) {
						module_data = XI::ReadConsistencyData(stream);
						module_signature = XE::Security::LoadContainer(stream);
					}
				} catch (...) {
					if (!state.silent) console << TextColor(12) << Localized(302) << TextColorDefault() << LineFeed();
					return 2;
				}
			} catch (...) {
				if (!state.silent) console << TextColor(12) << Localized(301) << TextColorDefault() << LineFeed();
				return 1;
			}
			if (state.inspect_mask & InspectSymbols) {
				console << Localized(401) << LineFeed();
				Volumes::Set<string> smbls;
				for (auto & c : module->literals) smbls.AddElement(c.key + Localized(402));
				for (auto & c : module->classes) smbls.AddElement(c.key + Localized(403));
				for (auto & c : module->variables) smbls.AddElement(c.key + Localized(404));
				for (auto & c : module->functions) smbls.AddElement(c.key + Localized(405));
				for (auto & c : module->aliases) smbls.AddElement(c.key + Localized(406));
				for (auto & c : module->prototypes) smbls.AddElement(c.key + Localized(407));
				for (auto & s : smbls) console << s << LineFeed();
			}
			if (state.inspect_mask & InspectDependencies) {
				console << Localized(408) << LineFeed();
				for (auto & d : module->modules_depends_on) console << d << LineFeed();
			}
			if (state.inspect_mask & InspectModuleStamp) {
				string ss;
				if (module->subsystem == XI::Module::ExecutionSubsystem::ConsoleUI) ss = Localized(410);
				else if (module->subsystem == XI::Module::ExecutionSubsystem::GUI) ss = Localized(411);
				else if (module->subsystem == XI::Module::ExecutionSubsystem::NoUI) ss = Localized(412);
				else if (module->subsystem == XI::Module::ExecutionSubsystem::Library) ss = Localized(413);
				else if (module->subsystem == XI::Module::ExecutionSubsystem::XW) ss = Localized(427);
				console << FormatString(Localized(409), module->module_import_name,
					module->assembler_name, module->assembler_version.major, module->assembler_version.minor,
					module->assembler_version.subver, module->assembler_version.build, ss) << LineFeed();
			}
			if (state.inspect_mask & InspectMetadata) try {
				SafePointer< Volumes::Dictionary<string, string> > meta = XI::LoadModuleMetadata(module->resources);
				console << Localized(414) << LineFeed();
				Array<string> keys(0x10), values(0x10);
				for (auto & m : meta->Elements()) { keys << m.key; values << m.value; }
				const string * table[] = { keys.GetBuffer(), values.GetBuffer() };
				TableView(table, 2, keys.Length());
			} catch (...) {}
			if (state.inspect_mask & InspectIcon) try {
				Array<Color> palette(0x10);
				for (int i = 0; i < 16; i++) palette << Color(Math::ColorF(static_cast<Math::StandardColor>(i)));
				SafePointer<Codec::Frame> icon = XI::LoadModuleIcon(module->resources, 1, Point(16, 16));
				if (!icon) throw Exception();
				console << Localized(415) << LineFeed();
				for (int y = 0; y < icon->GetHeight(); y++) {
					for (int x = 0; x < icon->GetWidth(); x++) {
						auto color = Color(icon->ReadPixel(x, y));
						if (color.a < 128) console << L"  "; else {
							int index = -1;
							int dist;
							for (int i = 0; i < palette.Length(); i++) {
								int ld = abs(int(color.r) - palette[i].r) + abs(int(color.g) - palette[i].g) + abs(int(color.b) - palette[i].b);
								if (index < 0 || ld < dist) { index = i; dist = ld; }
							}
							console << TextBackground(index) << L"  " << TextBackgroundDefault();
						}
					}
					console << LineFeed();
				}
			} catch (...) {}
			if (state.inspect_mask & InspectResources) {
				console << Localized(416) << LineFeed();
				Array<string> types(0x10), ids(0x10), sizes(0x10);
				types << Localized(417);
				ids << Localized(418);
				sizes << Localized(419);
				for (auto & r : module->resources) {
					string type;
					int id;
					XI::ReadResourceID(r.key, type, id);
					types << type;
					ids << string(id);
					sizes << string(r.value->Length());
				}
				const string * table[] = { types.GetBuffer(), ids.GetBuffer(), sizes.GetBuffer() };
				TableView(table, 3, types.Length());
			}
			if (state.inspect_mask & InspectVersions) try {
				XI::AssemblyVersionInformation vi;
				if (XI::LoadModuleVersionInformation(module->resources, vi)) {
					console << Localized(420) << LineFeed();
					if (vi.ThisModuleVersion != 0xFFFFFFFF) {
						console << Localized(422) << string(vi.ThisModuleVersion, HexadecimalBase, 8) << LineFeed();
					}
					if (vi.ReplacesVersions.Length()) {
						Array<string> vol1(0x20), vol2(0x20);
						vol1 << Localized(423); vol2 << Localized(424);
						for (auto & v : vi.ReplacesVersions) {
							vol1 << string(v.MustBe, HexadecimalBase, 8);
							vol2 << string(v.Mask, HexadecimalBase, 8);
						}
						const string * table[] = { vol1.GetBuffer(), vol2.GetBuffer() };
						TableView(table, 2, vol1.Length());
					}
					if (!vi.ModuleVersionsNeeded.IsEmpty()) {
						Array<string> vol1(0x20), vol2(0x20);
						vol1 << Localized(425); vol2 << Localized(426);
						for (auto & v : vi.ModuleVersionsNeeded) {
							vol1 << string(v.value, HexadecimalBase, 8);
							vol2 << v.key;
						}
						const string * table[] = { vol1.GetBuffer(), vol2.GetBuffer() };
						TableView(table, 2, vol1.Length());
					}
				} else console << Localized(421) << LineFeed();
			} catch (...) {}
			if (state.validate_security) {
				SafePointer<XE::Security::ITrustProvider> trust;
				try {
					trust = XE::Security::CreateTrustProvider();
					if (!trust) throw OutOfMemoryException();
					if (!trust->AddTrustDirectory(state.security.TrustedCertificates, true)) throw InvalidStateException();
					if (!trust->AddTrustDirectory(state.security.UntrustedCertificates, false)) throw InvalidStateException();
				} catch (...) {
					if (!state.silent) console << TextColor(12) << Localized(508) << TextColorDefault() << LineFeed();
					return 1;
				}
				if (!state.silent) {
					if (state.security.ValidateTrust) {
						console.SetTextColor(10);
						console.WriteLine(Localized(501));
						console.SetTextColor(-1);
					} else {
						console.SetTextColor(14);
						console.WriteLine(Localized(502));
						console.SetTextColor(-1);
					}
				}
				if (module_signature) {
					SafePointer<DataBlock> hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, module_data);
					if (!hash) throw Exception();
					XE::Security::IntegrityValidationDesc validation;
					auto status = XE::Security::EvaluateIntegrity(hash, module_signature, Time::GetCurrentTime(), trust, &validation);
					if (!state.silent) {
						if (status == XE::Security::TrustStatus::Untrusted) {
							console.SetTextColor(12);
							console.WriteLine(Localized(512));
							console.SetTextColor(-1);
						} else if (status == XE::Security::TrustStatus::Undefined) {
							console.SetTextColor(14);
							console.WriteLine(Localized(513));
							console.SetTextColor(-1);
						} else if (status == XE::Security::TrustStatus::Trusted) {
							console.SetTextColor(10);
							console.WriteLine(Localized(514));
							console.SetTextColor(-1);
						}
					}
					Array<string> objects(0x20), states(0x20);
					Array<int> colors(0x20);
					objects << Localized(509); states << IntegrityStatusToString(validation.object); colors << IntegrityStatusToColor(validation.object);
					for (int i = 0; i < validation.chain.Length(); i++) {
						SafePointer<XE::Security::ICertificate> cert = module_signature->LoadCertificate(i);
						if (cert) {
							auto & desc = cert->GetDescription();
							if (desc.PersonName.Length() && desc.Organization.Length()) {
								objects << (desc.PersonName + L" / " + desc.Organization);
							} else if (desc.PersonName.Length()) {
								objects << desc.PersonName;
							} else {
								objects << desc.Organization;
							}
						} else objects << L"?";
						states << IntegrityStatusToString(validation.chain[i]); colors << IntegrityStatusToColor(validation.chain[i]);
					}
					const string * table[] = { objects.GetBuffer(), states.GetBuffer() };
					if (!state.silent) {
						TableView(table, 2, objects.Length(), colors);
						console.SetTextColor(-1);
					}
				} else {
					if (!state.silent) {
						console.SetTextColor(14);
						console.WriteLine(Localized(511));
						console.SetTextColor(-1);
					}
				}
			}
			if (state.pret_list.Length()) {
				try {
					XI::PretranslateModule(*module, state.pret_list.GetBuffer(), state.pret_list.Length());
				} catch (InvalidArgumentException &) {
					if (!state.silent) console << TextColor(12) << Localized(303) << TextColorDefault() << LineFeed();
					return 3;
				}
				if (!state.output.Length()) {
					auto dir = Path::GetDirectory(state.input);
					auto name = Path::GetFileNameWithoutExtension(state.input);
					auto ext = Path::GetExtension(state.input);
					state.output = ExpandPath(dir + L"/" + name + L".praet." + ext);
				}
				try {
					SafePointer<Stream> stream = new FileStream(state.output, AccessReadWrite, CreateAlways);
					module->Save(stream);
					if (state.identity_file.Length()) {
						if (!CreateSecurityData(stream, identity)) {
							if (!state.silent) console << TextColor(12) << Localized(507) << TextColorDefault() << LineFeed();
							return 4;
						}
					}
				} catch (...) {
					if (!state.silent) console << TextColor(12) << Localized(304) << TextColorDefault() << LineFeed();
					return 4;
				}
			} else if (state.identity_file.Length()) {
				if (!state.output.Length()) {
					auto dir = Path::GetDirectory(state.input);
					auto name = Path::GetFileNameWithoutExtension(state.input);
					auto ext = Path::GetExtension(state.input);
					state.output = ExpandPath(dir + L"/" + name + L".subs." + ext);
				}
				try {
					SafePointer<Stream> stream = new FileStream(state.output, AccessReadWrite, CreateAlways);
					module->Save(stream);
					if (state.identity_file.Length()) {
						if (!CreateSecurityData(stream, identity)) {
							if (!state.silent) console << TextColor(12) << Localized(507) << TextColorDefault() << LineFeed();
							return 4;
						}
					}
				} catch (...) {
					if (!state.silent) console << TextColor(12) << Localized(304) << TextColorDefault() << LineFeed();
					return 4;
				}
			}
		} catch (...) { return 0x3F; }
	} else if (!state.print_security_state) {
		if (!state.silent) try {
			auto length = Localized(100).ToInt32();
			for (int i = 0; i < length; i++) console << Localized(101 + i) << LineFeedSequence;
		} catch (...) {}
	}
	return 0;
}