#include "xesec.h"
#include "../xenv_sec/xe_sec_core.h"
#include "../xenv_sec/xe_sec_rsa.h"
#include "../xenv_sec/xe_sec_arithm.h"

using namespace Engine;
using namespace Engine::UI;
using namespace Engine::Windows;
using namespace Engine::Storage;
using namespace Engine::Streaming;

#define STORE_FILE_PATH	L"/store.ecs"

Engine::UI::InterfaceTemplate interface;
Volumes::Dictionary<IWindow *, IWindowCallback *> windows;
uint modal_counter = 0;
void RegisterWindow(Engine::Windows::IWindow * window, Engine::Windows::IWindowCallback * callback) { windows.Append(window, callback); }
void UnregisterWindow(Engine::Windows::IWindow * window) { windows.Remove(window); if (windows.IsEmpty()) GetWindowSystem()->ExitMainLoop(); }

void OpenFileDialog(Engine::Windows::OpenFileInfo & info, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task)
{
	SafePointer<IDispatchTask> t; t.SetRetain(task);
	InterlockedIncrement(modal_counter);
	GetWindowSystem()->OpenFileDialog(&info, modally, CreateFunctionalTask([t]() {
		InterlockedDecrement(modal_counter);
		if (t) t->DoTask(GetWindowSystem());
		if (!modal_counter && windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
	}));
}
void SaveFileDialog(Engine::Windows::SaveFileInfo & info, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task)
{
	SafePointer<IDispatchTask> t; t.SetRetain(task);
	InterlockedIncrement(modal_counter);
	GetWindowSystem()->SaveFileDialog(&info, modally, CreateFunctionalTask([t]() {
		InterlockedDecrement(modal_counter);
		if (t) t->DoTask(GetWindowSystem());
		if (!modal_counter && windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
	}));
}
void ChooseDirectoryDialog(Engine::Windows::ChooseDirectoryInfo & info, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task)
{
	SafePointer<IDispatchTask> t; t.SetRetain(task);
	InterlockedIncrement(modal_counter);
	GetWindowSystem()->ChooseDirectoryDialog(&info, modally, CreateFunctionalTask([t]() {
		InterlockedDecrement(modal_counter);
		if (t) t->DoTask(GetWindowSystem());
		if (!modal_counter && windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
	}));
}
void MessageBox(MessageBoxResult * result, const string & text, const string & title, IWindow * parent, MessageBoxButtonSet buttons, MessageBoxStyle style, IDispatchTask * task)
{
	SafePointer<IDispatchTask> t; t.SetRetain(task);
	InterlockedIncrement(modal_counter);
	GetWindowSystem()->MessageBox(result, text, title, parent, buttons, style, CreateFunctionalTask([t]() {
		InterlockedDecrement(modal_counter);
		if (t) t->DoTask(GetWindowSystem());
		if (!modal_counter && windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
	}));
}
void PasswordInput(Engine::string & input, bool & status, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task)
{
	class PasswordInputCallback : public IEventCallback
	{
		string * _input;
		bool * _status;
		SafePointer<IDispatchTask> _task;
	public:
		PasswordInputCallback(Engine::string & input, bool & status, Engine::IDispatchTask * task) : _input(&input), _status(&status) { _task.SetRetain(task); }
		virtual void Created(IWindow * window) override
		{
			GetRootControl(window)->AddDialogStandardAccelerators();
			window->SetText(ENGINE_VI_APPNAME);
		}
		virtual void Destroyed(IWindow * window) override { delete this; }
		virtual void WindowClose(IWindow * window) override
		{
			*_input = L"";
			*_status = false;
			auto task = _task;
			GetWindowSystem()->ExitModalSession(window);
			InterlockedDecrement(modal_counter);
			if (!modal_counter && windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
			if (task) task->DoTask(GetWindowSystem());
		}
		virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
		{
			if (ID == 1) {
				*_input = FindControl(window, 101)->GetText();
				*_status = true;
				auto task = _task;
				GetWindowSystem()->ExitModalSession(window);
				InterlockedDecrement(modal_counter);
				if (!modal_counter && windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
				if (task) task->DoTask(GetWindowSystem());
			} else if (ID == 2) WindowClose(window);
		}
	};
	auto callback = new PasswordInputCallback(input, status, task);
	InterlockedIncrement(modal_counter);
	CreateModalWindow(interface.Dialog[L"PasswordInput"], callback, Rectangle::Entire(), modally);
}

void OpenFiles(IWindow * relative_to, IDispatchTask * on_success)
{
	SafePointer<IDispatchTask> succ; succ.SetRetain(on_success);
	auto task = CreateStructuredTask<OpenFileInfo>([succ](const OpenFileInfo & value) {
		auto app = GetWindowSystem()->GetCallback();
		if (app) for (auto & f : value.Files) app->OpenExactFile(f);
		if (succ && value.Files.Length()) succ->DoTask(GetWindowSystem());
	});
	task->Value1.Formats << FileFormat();
	task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatCert"];
	task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Certificate;
	task->Value1.Formats << FileFormat();
	task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatUCert"];
	task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::UnsignedCertificate;
	task->Value1.Formats << FileFormat();
	task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatKey"];
	task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::PrivateKey;
	task->Value1.Formats << FileFormat();
	task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatIdent"];
	task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Identity;
	task->Value1.MultiChoose = true;
	OpenFileDialog(task->Value1, relative_to, task);
}
class StoreViewCallback : public IEventCallback, Controls::RichEdit::IRichEditHook
{
	int _mode;
	string _path;
	SafePointer<XE::Security::IContainer> _store;
	SafePointer<XE::Security::IKey> _private_key;
	IWindow * _window;
private:
	static string _usage_desc(uint usage)
	{
		DynamicString result;
		if (usage & XE::Security::CertificateUsageAuthority) {
			if (result.Length()) result << L", ";
			result << L"\033b" << *interface.Strings[L"ViewPropUAuth"] << L"\033e";
		}
		if (usage & XE::Security::CertificateUsageSignature) {
			if (result.Length()) result << L", ";
			result << L"\033b" << *interface.Strings[L"ViewPropUSign"] << L"\033e";
		}
		if (usage & ~XE::Security::CertificateUsageMask) {
			if (result.Length()) result << L", ";
			result << L"\033b" << string(usage & ~XE::Security::CertificateUsageMask, HexadecimalBase, 8) << L"\033e";
		}
		return result.ToString();
	}
	static string _long_number(DataBlock * data)
	{
		DynamicString result;
		if (data) for (int i = data->Length() - 1; i >= 0; i--) {
			if (!data->ElementAt(i) && !result.Length()) continue;
			if (result.Length()) result << L' ';
			result << string(uint(data->ElementAt(i)), HexadecimalBase, 2);
		}
		if (!result.Length()) result << L"00";
		return result.ToString();
	}
	static void _write_key_desc(XE::Security::IKey * key, Controls::RichEdit * edit)
	{
		if (key->GetKeyClass() == XE::Security::KeyClass::RSA_Public || key->GetKeyClass() == XE::Security::KeyClass::RSA_Private) {
			uint num_bits = 0;
			DynamicString attrs;
			for (uint i = 0; i < key->GetParameterCount(); i++) {
				string name;
				SafePointer<DataBlock> data;
				if (key->LoadParameter(i, name, data.InnerRef())) {
					attrs << name << L" = \033b" << _long_number(data) << L"\033e\n";
					if (name == L"modulus" && data) for (int i = data->Length() - 1; i >= 0; i--) if (data->ElementAt(i)) { num_bits = (i + 1) * 8; break; }
				}
			}
			edit->Print(L"\033b" + FormatString(*interface.Strings[L"ViewPropRSA"], num_bits) + L"\033e\n");
			edit->Print(attrs.ToString());
		}
	}
	static void _write_cert_desc(XE::Security::ICertificate * cert, DataBlock * cert_ds, Controls::RichEdit * edit, bool ask_to_sign)
	{
		Time time = Time::GetCurrentTime();
		SafePointer<XE::Security::IKey> cert_key = cert->LoadPublicKey();
		auto & desc = cert->GetDescription();
		if (desc.IsRootCertificate) {
			edit->Print(L"\033b" + *interface.Strings[L"ViewPropIsRoot"] + L"\033e\n");
		}
		if (desc.PersonName.Length()) {
			edit->Print(*interface.Strings[L"ViewPropPerson"]);
			edit->Print(L"\033b" + desc.PersonName + L"\033e\n");
		}
		if (desc.Organization.Length()) {
			edit->Print(*interface.Strings[L"ViewPropOrg"]);
			edit->Print(L"\033b" + desc.Organization + L"\033e\n");
		}
		string clr = time < desc.ValidSince ? L"FF00C0C0" : L"FF40C020";
		edit->Print(*interface.Strings[L"ViewPropValidSince"]);
		edit->Print(L"\033b\033c" + clr + desc.ValidSince.ToLocal().ToString() + L"\033e\033e\n");
		clr = time > desc.ValidUntil ? L"FF0000FF" : L"FF40C020";
		edit->Print(*interface.Strings[L"ViewPropValidUntil"]);
		edit->Print(L"\033b\033c" + clr + desc.ValidUntil.ToLocal().ToString() + L"\033e\033e\n");
		if (desc.CertificateUsage) {
			edit->Print(*interface.Strings[L"ViewPropUsage"] + _usage_desc(desc.CertificateUsage) + L"\n");
		}
		if (desc.CertificateDerivation) {
			edit->Print(*interface.Strings[L"ViewPropUsageDeriv"] + _usage_desc(desc.CertificateDerivation) + L"\n");
		}
		if (!desc.Attributes.IsEmpty()) {
			edit->Print(L"\033b" + *interface.Strings[L"ViewPropAttributes"] + L"\033e\n");
			for (auto & a : desc.Attributes) edit->Print(a.key + L" = \033b" + a.value + L"\033e\n");
		}
		if (cert_key) _write_key_desc(cert_key, edit);
		if (cert_ds) {
			edit->Print(*interface.Strings[L"ViewPropSignature"]);
			edit->Print(L"\033b" + _long_number(cert_ds) + L"\033e\n");
		} else if (ask_to_sign) {
			edit->Print(*interface.Strings[L"ViewPropNoSign"]);
		}
	}
public:
	StoreViewCallback(XE::Security::IContainer * store, const string & path) : _mode(0), _path(path) { _store.SetRetain(store); }
	virtual void Created(IWindow * window) override
	{
		_window = window;
		GetRootControl(window)->AddDialogStandardAccelerators();
		window->SetText(IO::Path::GetFileName(_path) + L" - " + window->GetText());
		FindControl(window, 102)->As<Controls::RichEdit>()->SetHook(this);
		auto root = FindControl(window, 101)->As<Controls::TreeView>()->GetRootItem();
		auto item = root;
		for (uint i = 0; i < _store->GetCertificateChainLength(); i++) {
			SafePointer<XE::Security::ICertificate> cert = _store->LoadCertificate(i);
			if (!cert) break;
			string text;
			auto & desc = cert->GetDescription();
			if (desc.PersonName.Length() && desc.Organization.Length()) {
				text = FormatString(*interface.Strings[L"ViewObjCertFull"], desc.PersonName, desc.Organization);
			} else if (desc.Organization.Length()) {
				text = FormatString(*interface.Strings[L"ViewObjCertShort"], desc.Organization);
			} else {
				text = FormatString(*interface.Strings[L"ViewObjCertShort"], desc.PersonName);
			}
			item = item->AddItem(text, handle(i));
		}
		if (_store->GetContainerClass() == XE::Security::ContainerClass::PrivateKey || _store->GetContainerClass() == XE::Security::ContainerClass::Identity) {
			root->AddItem(*interface.Strings[L"ViewObjKey"], handle(intptr(-1)));
		}
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { if (_mode) return; window->Destroy(); UnregisterWindow(window); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (_mode) return;
		if (ID == 101 && event == ControlEvent::ValueChange) {
			auto edit = FindControl(window, 102)->As<Controls::RichEdit>();
			auto item = FindControl(window, 101)->As<Controls::TreeView>()->GetSelectedItem();
			edit->SetText(L"");
			if (!item) return;
			auto index = sintptr(item->User);
			if (index >= 0) {
				if (_store->GetContainerClass() == XE::Security::ContainerClass::Identity) {
					edit->Print(*interface.Strings[L"ViewPropGetPublic"] + L"\n");
				}
				SafePointer<XE::Security::ICertificate> cert = _store->LoadCertificate(index);
				SafePointer<DataBlock> cert_ds = _store->LoadCertificateSignature(index);
				if (!cert) return;
				_write_cert_desc(cert, cert_ds, edit, true);
			} else {
				if (_store->GetContainerClass() == XE::Security::ContainerClass::PrivateKey) {
					edit->Print(*interface.Strings[L"ViewPropMakeIdent"] + L"\n");
				}
				if (_private_key) {
					_write_key_desc(_private_key, edit);
				} else {
					edit->Print(*interface.Strings[L"ViewPropPreviewKey"] + L"\n");
				}
			}
		}
	}
	virtual void LinkPressed(const string & resource, Controls::RichEdit * sender) override
	{
		if (_mode) return;
		if (resource == L"subs") {
			auto task = CreateStructuredTask<OpenFileInfo>([this](const OpenFileInfo & value) {
				if (value.Files.Length()) {
					SafePointer<XE::Security::IContainer> ident_cont;
					try {
						SafePointer<Stream> stream = new FileStream(value.Files[0], AccessRead, OpenExisting);
						ident_cont = XE::Security::LoadContainer(stream);
						if (!ident_cont) throw InvalidFormatException();
						if (ident_cont->GetContainerClass() != XE::Security::ContainerClass::Identity) throw InvalidFormatException();
					} catch (...) {
						MessageBox(0, FormatString(*interface.Strings[L"ErrorInvalidFile"], value.Files[0]), ENGINE_VI_APPNAME,
							_window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
						return;
					}
					auto task2 = CreateStructuredTask<string, bool>([this, ident_cont](const string & password, bool status) {
						if (status) {
							SafePointer<XE::Security::IIdentity> ident = XE::Security::LoadIdentity(ident_cont, 0, password);
							if (!ident) {
								MessageBox(0, *interface.Strings[L"ErrorInvalidIdent"], ENGINE_VI_APPNAME, _window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
								return;
							}
							SafePointer<TaskQueue> queue = new TaskQueue;
							if (!queue->ProcessAsSeparateThread()) return;
							_mode = 1;
							_window->SetCloseButtonState(CloseButtonState::Disabled);
							queue->SubmitTask(CreateFunctionalTask([this, ident]() {
								SafePointer<XE::Security::ICertificate> cert_unsigned = _store->LoadCertificate(0);
								SafePointer<XE::Security::IContainer> cert_signed = cert_unsigned ? XE::Security::ValidateCertificate(cert_unsigned, ident) : 0;
								GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, cert_signed]() {
									_mode = 0;
									_window->SetCloseButtonState(CloseButtonState::Enabled);
									auto task3 = CreateStructuredTask<SaveFileInfo>([this, cert_signed](const SaveFileInfo & value) {
										if (value.File.Length()) try {
											SafePointer<DataBlock> data = cert_signed->LoadContainerRepresentation();
											if (!data) throw OutOfMemoryException();
											SafePointer<Stream> stream = new FileStream(value.File, AccessWrite, CreateAlways);
											stream->WriteArray(data);
										} catch (...) {
											MessageBox(0, FormatString(*interface.Strings[L"ErrorSaveFailure"], value.File), ENGINE_VI_APPNAME,
												_window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
										}
										auto task4 = CreateStructuredTask<MessageBoxResult>([this](MessageBoxResult result) {
											if (result == MessageBoxResult::Yes) try {
												IO::RemoveFile(_path);
											} catch (...) {}
										});
										MessageBox(&task4->Value1, *interface.Strings[L"MsgCertSigned"], ENGINE_VI_APPNAME, _window, MessageBoxButtonSet::YesNo, MessageBoxStyle::Information, task4);
									});
									task3->Value1.Formats << FileFormat();
									task3->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatCert"];
									task3->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Certificate;
									SaveFileDialog(task3->Value1, _window, task3);
								}));
							}));
							queue->Quit();
						}
					});
					PasswordInput(task2->Value1, task2->Value2, _window, task2);
				}
			});
			task->Value1.Formats << FileFormat();
			task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatIdent"];
			task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Identity;
			OpenFileDialog(task->Value1, _window, task);
		} else if (resource == L"expub") {
			SafePointer<XE::Security::IContainer> pubcert = XE::Security::CreateCertificateStorage(_store);
			if (!pubcert) return;
			auto task = CreateStructuredTask<SaveFileInfo>([this, pubcert](const SaveFileInfo & value) {
				if (value.File.Length()) try {
					SafePointer<DataBlock> data = pubcert->LoadContainerRepresentation();
					if (!data) throw OutOfMemoryException();
					SafePointer<Stream> stream = new FileStream(value.File, AccessWrite, CreateAlways);
					stream->WriteArray(data);
				} catch (...) {
					MessageBox(0, FormatString(*interface.Strings[L"ErrorSaveFailure"], value.File), ENGINE_VI_APPNAME,
						_window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
				}
			});
			task->Value1.Formats << FileFormat();
			task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatCert"];
			task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Certificate;
			SaveFileDialog(task->Value1, _window, task);
		} else if (resource == L"crind") {
			auto task = CreateStructuredTask<OpenFileInfo>([this](const OpenFileInfo & value) {
				if (value.Files.Length()) {
					SafePointer<XE::Security::IContainer> cert;
					try {
						SafePointer<Stream> stream = new FileStream(value.Files[0], AccessRead, OpenExisting);
						cert = XE::Security::LoadContainer(stream);
						if (!cert) throw InvalidFormatException();
						if (cert->GetContainerClass() != XE::Security::ContainerClass::Certificate) throw InvalidFormatException();
					} catch (...) {
						MessageBox(0, FormatString(*interface.Strings[L"ErrorInvalidFile"], value.Files[0]), ENGINE_VI_APPNAME,
							_window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
						return;
					}
					auto task2 = CreateStructuredTask<string, bool>([this, cert, cert_file = value.Files[0]](const string & password, bool status) {
						if (status) {
							SafePointer<XE::Security::IIdentity> ident = XE::Security::LoadIdentity(_store, cert, password);
							SafePointer<XE::Security::IContainer> ident_cont = ident ? XE::Security::CreateIdentityStorage(ident, password) : 0;
							if (!ident_cont) {
								MessageBox(0, *interface.Strings[L"ErrorInvalidCertKey"], ENGINE_VI_APPNAME, _window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
								return;
							}
							auto task3 = CreateStructuredTask<SaveFileInfo>([this, ident_cont, cert_file](const SaveFileInfo & value) {
								if (value.File.Length()) try {
									SafePointer<DataBlock> data = ident_cont->LoadContainerRepresentation();
									if (!data) throw OutOfMemoryException();
									SafePointer<Stream> stream = new FileStream(value.File, AccessWrite, CreateAlways);
									stream->WriteArray(data);
								} catch (...) {
									MessageBox(0, FormatString(*interface.Strings[L"ErrorSaveFailure"], value.File), ENGINE_VI_APPNAME,
										_window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
								}
								auto task4 = CreateStructuredTask<MessageBoxResult>([this, cert_file](MessageBoxResult result) {
									if (result == MessageBoxResult::Yes) try {
										IO::RemoveFile(_path);
										IO::RemoveFile(cert_file);
									} catch (...) {}
								});
								MessageBox(&task4->Value1, *interface.Strings[L"MsgIdentityCreated"], ENGINE_VI_APPNAME, _window, MessageBoxButtonSet::YesNo, MessageBoxStyle::Information, task4);
							});
							task3->Value1.Formats << FileFormat();
							task3->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatIdent"];
							task3->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Identity;
							SaveFileDialog(task3->Value1, _window, task3);
						}
					});
					PasswordInput(task2->Value1, task2->Value2, _window, task2);
				}
			});
			task->Value1.Formats << FileFormat();
			task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatCert"];
			task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Certificate;
			OpenFileDialog(task->Value1, _window, task);
		} else if (resource == L"expri") {
			auto task = CreateStructuredTask<string, bool>([this](const string & password, bool status) {
				if (status) {
					_private_key = _store->LoadPrivateKey(password);
					HandleControlEvent(_window, 101, ControlEvent::ValueChange, 0);
				}
			});
			PasswordInput(task->Value1, task->Value2, _window, task);
		}
	}
};
class CreateCertCallback : public IEventCallback
{
	ENGINE_REFLECTED_CLASS(ListItem, Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Text1);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Text2);
	ENGINE_END_REFLECTED_CLASS
	static string _text_from_date(Time t)
	{
		auto tl = t.ToLocal();
		uint y, m, d;
		tl.GetDate(y, m, d);
		return FormatString(L"%2.%1.%0 %3:%4:%5", string(y, DecimalBase, 4), string(m, DecimalBase, 2), string(d, DecimalBase, 2),
			string(tl.GetHour(), DecimalBase, 2), string(tl.GetMinute(), DecimalBase, 2), string(tl.GetSecond(), DecimalBase, 2));
	}
	static Time _date_from_text(const string & s)
	{
		Array<uint> parts(0x10);
		int length = s.Length();
		int p = 0;
		while (p < length) {
			while (p < length && (s[p] == L' ' || s[p] == L'\t' || s[p] == L'.' || s[p] == L':')) p++;
			int o = p;
			while (p < length && (s[p] >= L'0' && s[p] <= L'9')) p++;
			if (p == o) break;
			auto f = s.Fragment(o, p - o);
			parts << s.Fragment(o, p - o).ToUInt32();
		}
		while (parts.Length() < 6) parts.Append(0);
		return Time(parts[2], parts[1], parts[0], parts[3], parts[4], parts[5], 0).ToUniversal();
	}
	void _update_usage_controls(IWindow * window)
	{
		auto susage = FindControl(window, 111)->As<Controls::CheckBox>()->IsChecked();
		FindControl(window, 121)->Enable(susage);
		FindControl(window, 122)->Enable(susage);
		if (!susage) {
			FindControl(window, 121)->As<Controls::CheckBox>()->Check(false);
			FindControl(window, 122)->As<Controls::CheckBox>()->Check(false);
		}
	}
	void _update_attribute_controls(IWindow * window)
	{
		auto key = FindControl(window, 202)->GetText();
		FindControl(window, 204)->Enable(!_cert.Attributes.ElementExists(key) && key.Length());
		FindControl(window, 205)->Enable(FindControl(window, 201)->As<Controls::ListView>()->GetSelectedIndex() >= 0);
	}
	void _reset_list(IWindow * window)
	{
		auto view = FindControl(window, 201)->As<Controls::ListView>();
		view->ClearItems();
		for (auto & a : _cert.Attributes) {
			ListItem item;
			item.Text1 = a.key;
			item.Text2 = a.value;
			view->AddItem(item);
		}
	}
	void _write_log(IWindow * window, const string & message, Color color)
	{
		auto re = FindControl(window, 1001)->As<Controls::RichEdit>();
		re->PrintAttributed(FormatString(L"\033c%0%1\033e\n", string(color.Value, HexadecimalBase, 8), message));
	}
private:
	uint _mode, _gcnt;
	XE::Security::CertificateDesc _cert;
	SafePointer<ThreadPool> _pool;
	SafePointer<XE::Security::IGeneratorTask> _gen1, _gen2;
	SafePointer<XE::Security::IKey> _key;
public:
	virtual void Created(IWindow * window) override
	{
		_mode = 0;
		GetRootControl(window)->AddDialogStandardAccelerators();
		auto dsacb = FindControl(window, 301)->As<Controls::ComboBox>();
		dsacb->AddItem(*interface.Strings[L"CreateDSA_RSA"]);
		dsacb->SetSelectedIndex(0);
		auto mdcb = FindControl(window, 302)->As<Controls::TextComboBox>();
		mdcb->AddItem(L"2048");
		mdcb->AddItem(L"4096");
		mdcb->AddItem(L"8192");
		mdcb->AddItem(L"16384");
		mdcb->SetText(L"2048");
		auto pecb = FindControl(window, 303)->As<Controls::ComboBox>();
		pecb->AddItem(L"15");
		pecb->AddItem(L"257");
		pecb->AddItem(L"65537");
		pecb->SetSelectedIndex(2);
		Time time = Time::GetCurrentTime();
		FindControl(window, 104)->SetText(_text_from_date(time));
		FindControl(window, 105)->SetText(_text_from_date(Time(time.GetYear() + 5, time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute(), time.GetSecond(), 0)));
		_update_usage_controls(window);
		_update_attribute_controls(window);
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override
	{
		if (_mode == 0) {
			window->Destroy();
			UnregisterWindow(window);
		} else if (_mode == 1) {
			if (_gen1) _gen1->Cancel();
			if (_gen2) _gen2->Cancel();
		} else if (_mode == 2) {
			window->Destroy();
			UnregisterWindow(window);
		}
	}
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (_mode == 0) {
			if (ID == 1) {
				uint modulus_length, e;
				string psw;
				try {
					_cert.PersonName = FindControl(window, 101)->GetText();
					_cert.Organization = FindControl(window, 102)->GetText();
					_cert.IsRootCertificate = FindControl(window, 103)->As<Controls::CheckBox>()->IsChecked();
					_cert.ValidSince = _date_from_text(FindControl(window, 104)->GetText());
					_cert.ValidUntil = _date_from_text(FindControl(window, 105)->GetText());
					_cert.CertificateUsage = _cert.CertificateDerivation = 0;
					if (FindControl(window, 111)->As<Controls::CheckBox>()->IsChecked()) _cert.CertificateUsage |= XE::Security::CertificateUsageAuthority;
					if (FindControl(window, 112)->As<Controls::CheckBox>()->IsChecked()) _cert.CertificateUsage |= XE::Security::CertificateUsageSignature;
					if (FindControl(window, 121)->As<Controls::CheckBox>()->IsChecked()) _cert.CertificateDerivation |= XE::Security::CertificateUsageAuthority;
					if (FindControl(window, 122)->As<Controls::CheckBox>()->IsChecked()) _cert.CertificateDerivation |= XE::Security::CertificateUsageSignature;
					modulus_length = FindControl(window, 302)->GetText().ToUInt32();
					uint e_index = FindControl(window, 303)->As<Controls::ComboBox>()->GetSelectedIndex();
					if (e_index == 0) e = 15;
					else if (e_index == 1) e = 257;
					else if (e_index == 2) e = 65537;
					else throw InvalidStateException();
					psw = FindControl(window, 304)->GetText();
					if (psw != FindControl(window, 305)->GetText()) throw InvalidArgumentException();
					if (modulus_length < 1024 || modulus_length > 16384) throw InvalidArgumentException();
				} catch (...) {
					MessageBox(0, *interface.Strings[L"ErrorInvalidCertReq"], ENGINE_VI_APPNAME, window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
					return;
				}
				_mode = 1;
				_gcnt = 2;
				FindControl(window, 1000)->Show(true);
				FindControl(window, 100)->Show(false);
				FindControl(window, 1)->Enable(false);
				window->SetCloseButtonState(CloseButtonState::Alert);
				auto chdlr = CreateFunctionalTask([this, w = window, e, psw]() {
					if (InterlockedDecrement(_gcnt)) return;
					if (_gen1 && _gen2 && _gen1->Status() != XE::Security::GeneratorTaskStatus::Incomplete && _gen2->Status() != XE::Security::GeneratorTaskStatus::Incomplete) {
						auto stat_1 = _gen1->Status();
						auto stat_2 = _gen2->Status();
						if (stat_1 == XE::Security::GeneratorTaskStatus::Failed || stat_2 == XE::Security::GeneratorTaskStatus::Failed) {
							_mode = 2;
							w->SetCloseButtonState(CloseButtonState::Enabled);
							_write_log(w, *interface.Strings[L"CreateStateKGenFail"], Color(255, 0, 0));
						} else if (stat_1 == XE::Security::GeneratorTaskStatus::NotFound || stat_2 == XE::Security::GeneratorTaskStatus::NotFound) {
							_mode = 2;
							w->SetCloseButtonState(CloseButtonState::Enabled);
							_write_log(w, *interface.Strings[L"CreateStateKGenFail"], Color(255, 0, 0));
						} else if (stat_1 == XE::Security::GeneratorTaskStatus::Cancelled || stat_2 == XE::Security::GeneratorTaskStatus::Cancelled) {
							w->Destroy();
							UnregisterWindow(w);
						} else if (stat_1 == XE::Security::GeneratorTaskStatus::Success && stat_2 == XE::Security::GeneratorTaskStatus::Success) {
							_write_log(w, *interface.Strings[L"CreateStateKCreate"], Color(0, 64, 192));
							_pool->SubmitTask(CreateFunctionalTask([this, w, e, psw, gen1 = _gen1, gen2 = _gen2]() {
								auto key_1 = gen1->Number();
								auto key_2 = gen2->Number();
								auto kstate = XE::Security::CreateKeyRSA(key_1->Data(), key_1->DWordLength() * 4, key_2->Data(), key_2->DWordLength() * 4, e, _key.InnerRef());
								if (!kstate) {
									GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, w]() {
										_mode = 2;
										w->SetCloseButtonState(CloseButtonState::Enabled);
										_write_log(w, *interface.Strings[L"CreateStateKCrFail"], Color(255, 0, 0));
									}));
									return;
								}
								GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, w]() {
									w->SetCloseButtonState(CloseButtonState::Disabled);
									_write_log(w, *interface.Strings[L"CreateStateCCreate"], Color(0, 64, 192));
								}));
								SafePointer<XE::Security::IKey> key_pub = _key->ExtractPublicKey();
								SafePointer<XE::Security::IContainer> cert_store = key_pub ? XE::Security::CreateCertificate(_cert, key_pub) : 0;
								SafePointer<XE::Security::IContainer> pkey_store = XE::Security::CreatePrivateKeyStorage(_key, psw);
								if (!cert_store || !pkey_store) {
									GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, w]() {
										_mode = 2;
										w->SetCloseButtonState(CloseButtonState::Enabled);
										_write_log(w, *interface.Strings[L"CreateStateKCrFail"], Color(255, 0, 0));
									}));
									return;
								}
								if (_cert.IsRootCertificate) {
									GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, w]() {
										_write_log(w, *interface.Strings[L"CreateStateCSign"], Color(0, 64, 192));
									}));
									SafePointer<XE::Security::ICertificate> cert_obj = cert_store->LoadCertificate(0);
									SafePointer<XE::Security::IContainer> cert_signed = cert_obj ? XE::Security::ValidateCertificate(cert_obj, _key) : 0;
									SafePointer<XE::Security::IIdentity> identity = cert_signed ? XE::Security::LoadIdentity(cert_signed, pkey_store, psw) : 0;
									SafePointer<XE::Security::IContainer> ident_store = identity ? XE::Security::CreateIdentityStorage(identity, psw) : 0;
									if (!ident_store) {
										GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, w]() {
											_mode = 2;
											w->SetCloseButtonState(CloseButtonState::Enabled);
											_write_log(w, *interface.Strings[L"CreateStateKCrFail"], Color(255, 0, 0));
										}));
										return;
									}
									GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, w, ident_store]() {
										w->SetCloseButtonState(CloseButtonState::Enabled);
										_write_log(w, *interface.Strings[L"CreateStateCSuccess"], Color(32, 128, 64));
										_mode = 2;
										auto task = CreateStructuredTask<SaveFileInfo>([this, w, ident_store](const SaveFileInfo & value) {
											if (value.File.Length()) {
												try {
													SafePointer<Stream> stream = new FileStream(value.File, AccessWrite, CreateAlways);
													SafePointer<DataBlock> data = ident_store->LoadContainerRepresentation();
													if (!data) throw InvalidStateException();
													stream->WriteArray(data);
													WindowClose(w);
												} catch (...) { _write_log(w, *interface.Strings[L"CreateStateCError"], Color(255, 0, 0)); }
											} else WindowClose(w);
										});
										task->Value1.Formats << FileFormat();
										task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatIdent"];
										task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::Identity;
										SaveFileDialog(task->Value1, w, task);
									}));
								} else {
									GetWindowSystem()->SubmitTask(CreateFunctionalTask([this, w, cert_store, pkey_store]() {
										_write_log(w, *interface.Strings[L"CreateStateCSuccess"], Color(32, 128, 64));
										w->SetCloseButtonState(CloseButtonState::Enabled);
										_mode = 2;
										auto task = CreateStructuredTask<SaveFileInfo>([this, w, cert_store, pkey_store](const SaveFileInfo & value) {
											if (value.File.Length()) {
												auto task2 = CreateStructuredTask<SaveFileInfo>([this, w, cert_store, pkey_store, cert_file = value.File](const SaveFileInfo & value) {
													if (value.File.Length()) {
														try {
															SafePointer<Stream> stream_cert = new FileStream(cert_file, AccessWrite, CreateAlways);
															SafePointer<Stream> stream_pkey = new FileStream(value.File, AccessWrite, CreateAlways);
															SafePointer<DataBlock> data_cert = cert_store->LoadContainerRepresentation();
															SafePointer<DataBlock> data_pkey = pkey_store->LoadContainerRepresentation();
															if (!data_cert || !data_pkey) throw InvalidStateException();
															stream_cert->WriteArray(data_cert);
															stream_pkey->WriteArray(data_pkey);
															WindowClose(w);
														} catch (...) { _write_log(w, *interface.Strings[L"CreateStateCError"], Color(255, 0, 0)); }
													} else WindowClose(w);
												});
												task2->Value1.Formats << FileFormat();
												task2->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatKey"];
												task2->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::PrivateKey;
												SaveFileDialog(task2->Value1, w, task2);
											} else WindowClose(w);
										});
										task->Value1.Formats << FileFormat();
										task->Value1.Formats.LastElement().Description = *interface.Strings[L"FileFormatUCert"];
										task->Value1.Formats.LastElement().Extensions << XE::Security::FileExtensions::UnsignedCertificate;
										SaveFileDialog(task->Value1, w, task);
									}));
								}
							}));
						} else {
							_mode = 2;
							w->SetCloseButtonState(CloseButtonState::Enabled);
							_write_log(w, *interface.Strings[L"CreateStateKGenFail"], Color(255, 0, 0));
						}
					}
				});
				_pool = new ThreadPool;
				_gen1 = XE::Security::LongGenerateRandomPrime(modulus_length / 2, 128, 2000, _pool, GetWindowSystem(), chdlr);
				_gen2 = XE::Security::LongGenerateRandomPrime(modulus_length / 2, 128, 2000, _pool, GetWindowSystem(), chdlr);
				_write_log(window, *interface.Strings[L"CreateStateKGen"], Color(0, 64, 192));
			} else if (ID == 2) {
				WindowClose(window);
			} else if (ID == 111) {
				_update_usage_controls(window);
			} else if (ID == 201 && event == ControlEvent::ValueChange) {
				_update_attribute_controls(window);
			} else if (ID == 202 && event == ControlEvent::ValueChange) {
				_update_attribute_controls(window);
			} else if (ID == 204) {
				auto key = FindControl(window, 202)->GetText();
				auto value = FindControl(window, 203)->GetText();
				_cert.Attributes.Append(key, value);
				FindControl(window, 202)->SetText(L"");
				FindControl(window, 203)->SetText(L"");
				_reset_list(window);
				_update_attribute_controls(window);
			} else if (ID == 205) {
				auto index = FindControl(window, 201)->As<Controls::ListView>()->GetSelectedIndex();
				auto rem = _cert.Attributes.ElementAt(index);
				if (rem) _cert.Attributes.BinaryTree::Remove(rem);
				_reset_list(window);
				_update_attribute_controls(window);
			}
		} else if (ID == 2) WindowClose(window);
	}
};
class SettingsCallback : public IEventCallback
{
	string _xe;
private:
	void _read(XX::SecuritySettings & sec, IWindow * window)
	{
		sec.ValidateTrust = FindControl(window, 101)->As<Controls::CheckBox>()->IsChecked();
		sec.ValidateTrustForQuarantine = FindControl(window, 104)->As<Controls::CheckBox>()->IsChecked();
		sec.TrustedCertificates = FindControl(window, 102)->GetText();
		sec.UntrustedCertificates = FindControl(window, 103)->GetText();
	}
public:
	SettingsCallback(const string & xe) : _xe(xe) {}
	virtual void Created(IWindow * window) override
	{
		GetRootControl(window)->AddDialogStandardAccelerators();
		XX::SecuritySettings security;
		try {
			XX::LoadSecuritySettings(security, _xe);
		} catch (...) {
			security.ValidateTrust = false;
			security.TrustedCertificates = security.UntrustedCertificates = L"";
		}
		FindControl(window, 101)->As<Controls::CheckBox>()->Check(security.ValidateTrust);
		FindControl(window, 104)->As<Controls::CheckBox>()->Check(security.ValidateTrustForQuarantine);
		FindControl(window, 102)->SetText(security.TrustedCertificates);
		FindControl(window, 103)->SetText(security.UntrustedCertificates);
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { window->Destroy(); UnregisterWindow(window); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 1) {
			XX::SecuritySettings security;
			_read(security, window);
			try {
				XX::UpdateSecuritySettings(security, _xe);
			} catch (...) {
				MessageBox(0, *interface.Strings[L"ErrorSecUpdate"], ENGINE_VI_APPNAME, window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
				return;
			}
			WindowClose(window);
		} else if (ID == 2) {
			WindowClose(window);
		} else if (ID == 101 || ID == 104) {
			XX::SecuritySettings security;
			_read(security, window);
			if ((security.ValidateTrust || security.ValidateTrustForQuarantine) && !security.TrustedCertificates.Length() && !security.UntrustedCertificates.Length()) {
				auto task = CreateStructuredTask<MessageBoxResult>([this, w = window](MessageBoxResult result) {
					if (result == MessageBoxResult::Yes) {
						FindControl(w, 102)->SetText(L"fidelitas");
						FindControl(w, 103)->SetText(L"infidelitas");
						try { IO::CreateDirectory(IO::Path::GetDirectory(_xe) + L"/fidelitas"); } catch (...) {}
						try { IO::CreateDirectory(IO::Path::GetDirectory(_xe) + L"/infidelitas"); } catch (...) {}
					}
				});
				MessageBox(&task->Value1, *interface.Strings[L"MsgCreateTrustPaths"], ENGINE_VI_APPNAME, window, MessageBoxButtonSet::YesNo, MessageBoxStyle::Information, task);
			}
		} else if (ID == 202) {
			XX::SecuritySettings security;
			_read(security, window);
			try { Shell::ShowInBrowser(IO::ExpandPath(IO::Path::GetDirectory(_xe) + L"/" + security.TrustedCertificates), true); } catch (...) {}
		} else if (ID == 203) {
			XX::SecuritySettings security;
			_read(security, window);
			try { Shell::ShowInBrowser(IO::ExpandPath(IO::Path::GetDirectory(_xe) + L"/" + security.UntrustedCertificates), true); } catch (...) {}
		}
	}
};
class IntroCallback : public IEventCallback
{
public:
	virtual void Created(IWindow * window) override { GetRootControl(window)->AddDialogStandardAccelerators(); }
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { window->Destroy(); UnregisterWindow(window); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 101) {
			OpenFiles(window, CreateFunctionalTask([this, w = window]() { WindowClose(w); }));
		} else if (ID == 102) {
			auto callback = new CreateCertCallback;
			auto wnd = CreateWindow(interface.Dialog[L"CreateCert"], callback, Rectangle::Entire());
			wnd->Show(true);
			RegisterWindow(wnd, callback);
			WindowClose(window);
		} else if (ID == 103) {
			GetWindowSystem()->GetCallback()->ShowProperties();
			WindowClose(window);
		} else if (ID == 2) WindowClose(window);
	}
};
class ApplicationCallback : public IApplicationCallback
{
public:
	virtual bool IsHandlerEnabled(ApplicationHandler event) override
	{
		if (event == ApplicationHandler::CreateFile) return true;
		else if (event == ApplicationHandler::OpenExactFile) return true;
		else if (event == ApplicationHandler::OpenSomeFile) return true;
		else if (event == ApplicationHandler::ShowProperties) return true;
		else if (event == ApplicationHandler::Terminate) return true;
		else return false;
	}
	virtual bool IsWindowEventAccessible(WindowHandler handler) override
	{
		if (handler == WindowHandler::Undo) return true;
		else if (handler == WindowHandler::Redo) return true;
		else if (handler == WindowHandler::Cut) return true;
		else if (handler == WindowHandler::Copy) return true;
		else if (handler == WindowHandler::Paste) return true;
		else if (handler == WindowHandler::Delete) return true;
		else if (handler == WindowHandler::SelectAll) return true;
		else return false;
	}
	virtual void CreateNewFile(void) override
	{
		auto callback = new IntroCallback;
		auto window = CreateWindow(interface.Dialog[L"Intro"], callback, Rectangle::Entire());
		window->Show(true);
		RegisterWindow(window, callback);
	}
	virtual void OpenSomeFile(void) override { OpenFiles(0, 0); }
	virtual bool OpenExactFile(const string & path) override
	{
		try {
			SafePointer<Stream> stream = new FileStream(path, AccessRead, OpenExisting);
			SafePointer<XE::Security::IContainer> store = XE::Security::LoadContainer(stream);
			if (!store) throw InvalidFormatException();
			auto callback = new StoreViewCallback(store, path);
			auto window = CreateWindow(interface.Dialog[L"ViewStore"], callback, Rectangle::Entire());
			window->Show(true);
			RegisterWindow(window, callback);
			return true;
		} catch (...) {
			MessageBox(0, FormatString(*interface.Strings[L"ErrorInvalidFile"], path), ENGINE_VI_APPNAME, 0, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
			return false;
		}
	}
	virtual void ShowProperties(void) override
	{
		string xe_conf;
		try { xe_conf = XX::LocateEnvironmentConfiguration(IO::Path::GetDirectory(IO::GetExecutablePath()) + STORE_FILE_PATH); } catch (...) {
			MessageBox(0, *interface.Strings[L"ErrorXXNotFound"], ENGINE_VI_APPNAME, 0, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
			return;
		}
		auto callback = new SettingsCallback(xe_conf);
		auto window = CreateWindow(interface.Dialog[L"Settings"], callback, Rectangle::Entire());
		window->Show(true);
		RegisterWindow(window, callback);
	}
	virtual bool Terminate(void) override
	{
		if (windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
		if (modal_counter) return false;
		auto wnd = windows;
		for (auto & w : wnd) w.value->WindowClose(w.key);
		return windows.IsEmpty();
	}
};

int Main(void)
{
	try {
		SafePointer<IScreen> screen = GetDefaultScreen();
		CurrentScaleFactor = screen->GetDpiScale();
		if (CurrentScaleFactor < 1.25) CurrentScaleFactor = 1.0;
		else if (CurrentScaleFactor < 1.75) CurrentScaleFactor = 1.5;
		else CurrentScaleFactor = 2.0;
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		SafePointer<Stream> com_stream = Assembly::QueryLocalizedResource(L"COM");
		SafePointer<StringTable> com = new StringTable(com_stream);
		Assembly::SetLocalizedCommonStrings(com);
		SafePointer<Stream> ui_stream = Assembly::QueryResource(L"UI");
		Loader::LoadUserInterfaceFromBinary(interface, ui_stream);
	} catch (...) { return 1; }
	ApplicationCallback callback;
	SafePointer< Array<string> > args = GetCommandLine();
	GetWindowSystem()->SetFilesToOpen(args->GetBuffer() + 1, args->Length() - 1);
	GetWindowSystem()->SetCallback(&callback);
	GetWindowSystem()->RunMainLoop();
	GetWindowSystem()->SetCallback(0);
	return 0;
}