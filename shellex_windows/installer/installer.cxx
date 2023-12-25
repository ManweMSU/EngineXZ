#include <EngineRuntime.h>
#include <PlatformSpecific/WindowsRegistry.h>

#include <Windows.h>
#include <Shlobj.h>

#undef GetCommandLine

using namespace Engine;
using namespace Engine::WindowsSpecific;
using namespace Engine::IO;

const uint8 self_guid_data[] = { 0xA1, 0x94, 0x8B, 0xFB, 0x80, 0x4D, 0x7A, 0xCA, 0xE3, 0x42, 0x75, 0x92, 0xE1, 0x46, 0x7C, 0x0D };
string self_guid;

void CreateFileClass(const string & cls, const string & desc, const string & icon, const string & exec)
{
	SafePointer<RegistryKey> root = OpenRootRegistryKey(RegistryRootKey::LocalMachine);
	root = root->OpenKey(L"Software\\Classes", RegistryKeyAccess::Full);
	SafePointer<RegistryKey> fmt_key = root->CreateKey(cls);
	fmt_key->SetValue(L"", desc);
	SafePointer<RegistryKey> icon_key = fmt_key->CreateKey(L"DefaultIcon");
	if (exec.Length()) {
		SafePointer<RegistryKey> shell_key = fmt_key->CreateKey(L"shell");
		shell_key->SetValue(L"", L"open");
		SafePointer<RegistryKey> open_key = shell_key->CreateKey(L"open");
		SafePointer<RegistryKey> command_key = open_key->CreateKey(L"command");
		command_key->SetValue(L"", L"\"" + exec + L"\" \"%1\"");
	} else fmt_key->SetValue(L"NoOpen", L"");
	SafePointer<RegistryKey> sex_key = fmt_key->CreateKey(L"ShellEx");
	if (!icon.Length()) {
		icon_key->SetValue(L"", L"%1");
		SafePointer<RegistryKey> icon_hdlr_key = sex_key->CreateKey(L"IconHandler");
		icon_hdlr_key->SetValue(L"", self_guid);
	} else icon_key->SetValue(L"", icon);
	fmt_key->SetValue(L"InfoTip", L"prop:System.Title;System.Author;System.Document.Version;System.Size;System.DateCreated");
	fmt_key->SetValue(L"FullDetails", L"prop:System.Title;System.Author;System.Copyright;System.Document.Version;System.ApplicationName;System.PropGroup.FileSystem;System.ItemNameDisplay;System.ItemTypeText;System.ItemFolderPathDisplay;System.Size;System.DateCreated;System.DateModified;System.FileAttributes;*System.StorageProviderState;*System.OfflineAvailability;*System.OfflineStatus;*System.SharedWith;*System.FileOwner;*System.ComputerName");
	fmt_key->SetValue(L"PreviewTitle", L"prop:System.Title;System.Author;System.Document.Version");
}
void AssignFileClass(const string & cls, const string & ext)
{
	SafePointer<RegistryKey> root = OpenRootRegistryKey(RegistryRootKey::LocalMachine);
	root = root->OpenKey(L"Software\\Classes", RegistryKeyAccess::Full);
	SafePointer<RegistryKey> ext_key = root->CreateKey(L"." + ext);
	SafePointer<RegistryKey> pids_key = ext_key->CreateKey(L"OpenWithProgids");
	pids_key->SetValue(cls, L"");
}

void RemoveRegistryKey(RegistryKey * root, const string & path)
{
	try {
		int index = path.FindLast(L'\\');
		string parent, name;
		if (index != -1) { parent = path.Fragment(0, index); name = path.Fragment(index + 1, -1); }
		else { parent = L""; name = path; }
		SafePointer<RegistryKey> parent_key;
		if (parent.Length()) parent_key = root->OpenKey(parent, RegistryKeyAccess::Full);
		else parent_key.SetRetain(root);
		if (parent_key) {
			SafePointer<RegistryKey> key = parent_key->OpenKey(name, RegistryKeyAccess::Full);
			if (key) {
				SafePointer< Array<string> > keys = key->EnumerateSubkeys();
				for (int i = 0; i < keys->Length(); i++) RemoveRegistryKey(key, keys->ElementAt(i));
			}
			key.SetReference(0);
			parent_key->DeleteKey(name);
		}
	} catch (...) {}
}
void RemoveRegistryValue(RegistryKey * root, const string & path)
{
	try {
		int index = path.FindLast(L'\\');
		string parent, name;
		if (index != -1) { parent = path.Fragment(0, index); name = path.Fragment(index + 1, -1); }
		else { parent = L""; name = path; }
		SafePointer<RegistryKey> parent_key;
		if (parent.Length()) parent_key = root->OpenKey(parent, RegistryKeyAccess::Full); else parent_key.SetRetain(root);
		if (parent_key) parent_key->DeleteValue(name);
	} catch (...) {}
}

int Main(void)
{
	SafePointer< Array<string> > args = GetCommandLine();
	if (args->Length() == 2) {
		auto dll_path = ExpandPath(Path::GetDirectory(GetExecutablePath()) + L"\\xxcomex.dll");
		auto exe_path = ExpandPath(Path::GetDirectory(GetExecutablePath()) + L"\\xxsc.exe");
		CLSID self;
		MemoryCopy(&self, &self_guid_data, sizeof(CLSID));
		LPOLESTR clsid_desc;
		if (StringFromCLSID(self, &clsid_desc) != S_OK) return 2;
		self_guid = clsid_desc;
		CoTaskMemFree(clsid_desc);
		if (args->ElementAt(1) == L"+") {
			Array<string> a2(1);
			a2 << L"+A";
			if (!CreateProcessElevated(GetExecutablePath(), &a2)) return 3;
			Engine::Sleep(1000);
		} else if (args->ElementAt(1) == L"+A") {
			SafePointer<RegistryKey> root = OpenRootRegistryKey(RegistryRootKey::LocalMachine);
			root = root->OpenKey(L"Software\\Classes", RegistryKeyAccess::Full);
			SafePointer<RegistryKey> clsid_root = root->OpenKey(L"CLSID", RegistryKeyAccess::Full);
			SafePointer<RegistryKey> clsid = clsid_root->CreateKey(self_guid);
			SafePointer<RegistryKey> server = clsid->CreateKey(L"InprocServer32");
			clsid->SetValue(L"", L"Engine XX Extensio COM");
			server->SetValue(L"", dll_path);
			server->SetValue(L"ThreadingModel", L"Apartment");
			root = OpenRootRegistryKey(RegistryRootKey::LocalMachine);
			root = root->OpenKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers", RegistryKeyAccess::Full);
			SafePointer<RegistryKey> ext_key = root->CreateKey(L".xx");
			ext_key->SetValue(L"", self_guid);
			ext_key = root->CreateKey(L".xex");
			ext_key->SetValue(L"", self_guid);
			ext_key = root->CreateKey(L".xo");
			ext_key->SetValue(L"", self_guid);
			CreateFileClass(L"Engine.XX", L"Applicatio XX", L"", exe_path);
			CreateFileClass(L"Engine.XO", L"Liber XX", exe_path + L",2", L"");
			AssignFileClass(L"Engine.XX", L"xx");
			AssignFileClass(L"Engine.XX", L"xex");
			AssignFileClass(L"Engine.XO", L"xo");
			SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
		} else if (args->ElementAt(1) == L"-") {
			Array<string> a2(1);
			a2 << L"-A";
			if (!CreateProcessElevated(GetExecutablePath(), &a2)) return 3;
			DynamicString temp_path;
			temp_path.ReserveLength(MAX_PATH + 1);
			if (GetTempPathW(temp_path.ReservedLength(), temp_path) == 0) return 4;
			temp_path[temp_path.Length() - 1] = 0;
			SetCurrentDirectoryW(temp_path);
			Engine::Sleep(1000);
		} else if (args->ElementAt(1) == L"-A") {
			SafePointer<RegistryKey> root = OpenRootRegistryKey(RegistryRootKey::LocalMachine);
			RemoveRegistryKey(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.xx");
			RemoveRegistryKey(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.xex");
			RemoveRegistryKey(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.xo");
			root = root->OpenKey(L"Software\\Classes", RegistryKeyAccess::Full);
			RemoveRegistryValue(root, L".xx\\OpenWithProgids\\Engine.XX");
			RemoveRegistryValue(root, L".xex\\OpenWithProgids\\Engine.XX");
			RemoveRegistryValue(root, L".xo\\OpenWithProgids\\Engine.XO");
			RemoveRegistryKey(root, L"Engine.XX");
			RemoveRegistryKey(root, L"Engine.XO");
			RemoveRegistryKey(root, string(L"CLSID\\") + self_guid);
			SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
			DynamicString self_move_to, dll_move_to, temp_path;
			self_move_to.ReserveLength(MAX_PATH + 1);
			dll_move_to.ReserveLength(MAX_PATH + 1);
			temp_path.ReserveLength(MAX_PATH + 1);
			if (GetTempPathW(temp_path.ReservedLength(), temp_path) == 0) return 4;
			if (GetTempFileNameW(temp_path, L"", 0, self_move_to) == 0) return 4;
			if (GetTempFileNameW(temp_path, L"", 0, dll_move_to) == 0) return 4;
			DeleteFileW(self_move_to.ToString());
			DeleteFileW(dll_move_to.ToString());
			MoveFileW(GetExecutablePath(), self_move_to.ToString());
			MoveFileW(dll_path, dll_move_to.ToString());
			MoveFileExW(self_move_to.ToString(), 0, MOVEFILE_DELAY_UNTIL_REBOOT);
			MoveFileExW(dll_move_to.ToString(), 0, MOVEFILE_DELAY_UNTIL_REBOOT);
			temp_path[temp_path.Length() - 1] = 0;
			SetCurrentDirectoryW(temp_path);
		} else return 1;
	} else return 1;
	return 0;
}