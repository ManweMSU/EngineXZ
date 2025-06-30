#include "xvm_editor.h"
#include "xvm_browser.h"

#include "../xv/xv_manual.h"

using namespace Engine;
using namespace Engine::UI;
using namespace Engine::Windows;
using namespace Engine::Storage;
using namespace Engine::Streaming;

bool IsTypeSection(const XV::ManualSection * section)
{
	return section->GetClass() == XV::ManualSectionClass::ObjectType || section->GetClass() == XV::ManualSectionClass::Inheritance ||
		section->GetClass() == XV::ManualSectionClass::CastSection || section->GetClass() == XV::ManualSectionClass::DynamicCastSection;
}
Volumes::Set<string> GetLanguages(const XV::ManualVolume * volume)
{
	Volumes::Set<string> result;
	for (auto & p : volume->GetPages()) for (auto & s : p.value->GetSections()) if (!IsTypeSection(&s) && !s.GetContents().IsEmpty()) {
		for (auto & l : s.GetContents()) result.AddElement(l.key);
		return result;
	}
	return result;
}

class AddPageCallback : public IEventCallback
{
	SafePointer<XV::ManualVolume> _volume;
	SafePointer<IDispatchTask> _on_add;
	Array< Volumes::KeyValuePair<XV::ManualPageClass, string> > _classes;
	XV::ManualPageClass _result_class;
	string _result_path;
public:
	AddPageCallback(IWindow * parent, XV::ManualVolume * volume, IDispatchTask * on_add) : _classes(0x20)
	{
		_volume.SetRetain(volume);
		_on_add.SetRetain(on_add);
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Constant, *interface.Strings[L"PageConst"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Variable, *interface.Strings[L"PageVar"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Class, *interface.Strings[L"PageClass"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Function, *interface.Strings[L"PageFunc"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Namespace, *interface.Strings[L"PageSpace"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Prototype, *interface.Strings[L"PageProto"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Property, *interface.Strings[L"PageProp"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Field, *interface.Strings[L"PageField"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Alias, *interface.Strings[L"PageAlias"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Technique, *interface.Strings[L"PageTech"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Syntax, *interface.Strings[L"PageSyntax"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualPageClass, string>(XV::ManualPageClass::Sample, *interface.Strings[L"PageSample"]));
		CreateModalWindow(interface.Dialog[L"AddPage"], this, Rectangle::Entire(), parent);
	}
	bool Validate(IWindow * window)
	{
		_result_class = _classes[FindControl(window, 101)->As<Controls::ComboBox>()->GetSelectedIndex()].key;
		_result_path = FindControl(window, 102)->GetText();
		bool result = _result_path.Length() && !_volume->GetPages().ElementExists(_result_path);
		FindControl(window, 1)->Enable(result);
		return result;
	}
	virtual void Created(IWindow * window) override
	{
		GetRootControl(window)->AddDialogStandardAccelerators();
		auto list = FindControl(window, 101)->As<Controls::ComboBox>();
		for (auto & c : _classes) list->AddItem(c.value);
		list->SetSelectedIndex(0);
		Validate(window);
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { HandleControlEvent(window, 2, ControlEvent::AcceleratorCommand, 0); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 1) {
			if (Validate(window)) {
				_volume->AddPage(_result_path, _result_class);
				_on_add->DoTask(0);
				GetWindowSystem()->ExitModalSession(window);
			}
		} else if (ID == 2) {
			GetWindowSystem()->ExitModalSession(window);
		} else if (ID == 101 && event == ControlEvent::ValueChange) {
			Validate(window);
		} else if (ID == 102 && event == ControlEvent::ValueChange) {
			Validate(window);
		}
	}
};
class AddSectionCallback : public IEventCallback
{
	SafePointer<XV::ManualPage> _page;
	SafePointer<IDispatchTask> _on_add;
	Array< Volumes::KeyValuePair<XV::ManualSectionClass, string> > _classes;
	XV::ManualSectionClass _result_class;
	string _result_name;
	int _result_index;
public:
	AddSectionCallback(IWindow * parent, XV::ManualPage * page, IDispatchTask * on_add) : _classes(0x20)
	{
		_page.SetRetain(page);
		_on_add.SetRetain(on_add);
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::ObjectType, *interface.Strings[L"SectObjType"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::Summary, *interface.Strings[L"SectSummary"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::Details, *interface.Strings[L"SectDetails"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::ArgumentSection, *interface.Strings[L"SectArgCom"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::ResultSection, *interface.Strings[L"SectRetVal"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::ThrowRules, *interface.Strings[L"SectThRules"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::ContextRules, *interface.Strings[L"SectCxRules"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::Inheritance, *interface.Strings[L"SectInherit"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::CastSection, *interface.Strings[L"SectCast"]));
		_classes.Append(Volumes::KeyValuePair<XV::ManualSectionClass, string>(XV::ManualSectionClass::DynamicCastSection, *interface.Strings[L"SectDynCast"]));
		CreateModalWindow(interface.Dialog[L"AddSect"], this, Rectangle::Entire(), parent);
	}
	bool Validate(IWindow * window)
	{
		try {
			_result_class = _classes[FindControl(window, 101)->As<Controls::ComboBox>()->GetSelectedIndex()].key;
			_result_name = FindControl(window, 102)->GetText();
			_result_index = FindControl(window, 103)->GetText().ToUInt32();
			bool result = _page->FindSection(_result_class, _result_index) == 0;
			FindControl(window, 1)->Enable(result);
			return result;
		} catch (...) { return false; }
	}
	virtual void Created(IWindow * window) override
	{
		GetRootControl(window)->AddDialogStandardAccelerators();
		auto list = FindControl(window, 101)->As<Controls::ComboBox>();
		for (auto & c : _classes) list->AddItem(c.value);
		list->SetSelectedIndex(0);
		Validate(window);
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { HandleControlEvent(window, 2, ControlEvent::AcceleratorCommand, 0); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 1) {
			if (Validate(window)) {
				_page->AddSection(_result_class, _result_index, _result_name);
				_on_add->DoTask(0);
				GetWindowSystem()->ExitModalSession(window);
			}
		} else if (ID == 2) {
			GetWindowSystem()->ExitModalSession(window);
		} else if (ID == 101 && event == ControlEvent::ValueChange) {
			Validate(window);
		} else if (ID == 102 && event == ControlEvent::ValueChange) {
			Validate(window);
		} else if (ID == 103 && event == ControlEvent::ValueChange) {
			Validate(window);
		}
	}
};
class AddLanguageCallback : public IEventCallback
{
	SafePointer<XV::ManualVolume> _volume;
	SafePointer<IDispatchTask> _on_add;
	string _result_name;
public:
	AddLanguageCallback(IWindow * parent, XV::ManualVolume * volume, IDispatchTask * on_add)
	{
		_volume.SetRetain(volume);
		_on_add.SetRetain(on_add);
		CreateModalWindow(interface.Dialog[L"AddLang"], this, Rectangle::Entire(), parent);
	}
	bool Validate(IWindow * window)
	{
		_result_name = FindControl(window, 101)->GetText();
		bool result = _result_name.Length() == 2;
		FindControl(window, 1)->Enable(result);
		return result;
	}
	virtual void Created(IWindow * window) override
	{
		GetRootControl(window)->AddDialogStandardAccelerators();
		Validate(window);
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { HandleControlEvent(window, 2, ControlEvent::AcceleratorCommand, 0); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 1) {
			if (Validate(window)) {
				for (auto & p : _volume->GetPages()) for (auto & s : p.value->GetSections()) if (!IsTypeSection(&s)) s.SetContents(_result_name, L"");
				_on_add->DoTask(0);
				GetWindowSystem()->ExitModalSession(window);
			}
		} else if (ID == 2) {
			GetWindowSystem()->ExitModalSession(window);
		} else if (ID == 101 && event == ControlEvent::ValueChange) {
			Validate(window);
		}
	}
};
class RemoveLanguageCallback : public IEventCallback
{
	SafePointer<XV::ManualVolume> _volume;
	SafePointer<IDispatchTask> _on_add;
	Volumes::Set<string> _lang_list;
	string _result_name;
public:
	RemoveLanguageCallback(IWindow * parent, XV::ManualVolume * volume, IDispatchTask * on_add)
	{
		_volume.SetRetain(volume);
		_on_add.SetRetain(on_add);
		_lang_list = GetLanguages(volume);
		CreateModalWindow(interface.Dialog[L"RemoveLang"], this, Rectangle::Entire(), parent);
	}
	bool Validate(IWindow * window)
	{
		_result_name = FindControl(window, 101)->GetText();
		bool result = _lang_list[_result_name];
		FindControl(window, 1)->Enable(result);
		return result;
	}
	virtual void Created(IWindow * window) override
	{
		GetRootControl(window)->AddDialogStandardAccelerators();
		Validate(window);
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { HandleControlEvent(window, 2, ControlEvent::AcceleratorCommand, 0); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 1) {
			if (Validate(window)) {
				for (auto & p : _volume->GetPages()) for (auto & s : p.value->GetSections()) if (!IsTypeSection(&s)) s.RemoveLocale(_result_name);
				_on_add->DoTask(0);
				GetWindowSystem()->ExitModalSession(window);
			}
		} else if (ID == 2) {
			GetWindowSystem()->ExitModalSession(window);
		} else if (ID == 101 && event == ControlEvent::ValueChange) {
			Validate(window);
		}
	}
};
class SetModuleNameCallback : public IEventCallback
{
	SafePointer<XV::ManualVolume> _volume;
	SafePointer<IDispatchTask> _on_add;
public:
	SetModuleNameCallback(IWindow * parent, XV::ManualVolume * volume, IDispatchTask * on_add)
	{
		_volume.SetRetain(volume);
		_on_add.SetRetain(on_add);
		CreateModalWindow(interface.Dialog[L"SetModuleName"], this, Rectangle::Entire(), parent);
	}
	virtual void Created(IWindow * window) override
	{
		GetRootControl(window)->AddDialogStandardAccelerators();
		FindControl(window, 101)->SetText(_volume->GetModule());
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { HandleControlEvent(window, 2, ControlEvent::AcceleratorCommand, 0); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 1) {
			auto old_mdl = _volume->GetModule();
			auto new_mdl = FindControl(window, 101)->GetText();
			if (old_mdl != new_mdl) {
				_volume->SetModule(new_mdl);
				_on_add->DoTask(0);
			}
			GetWindowSystem()->ExitModalSession(window);
		} else if (ID == 2) {
			GetWindowSystem()->ExitModalSession(window);
		}
	}
};
class EditorCallback : public IEventCallback
{
	IWindow * window;
	bool alternated;
	string path;
	XV::ManualPage * current_page;
	XV::ManualSection * current_section;
	SafePointer<XV::ManualVolume> volume;
	Volumes::Dictionary<int, string> lang_map;

	void FillPageList(Controls::TreeView * view, Controls::TreeView::TreeViewItem * item, const Volumes::ObjectDictionary<string, XV::ManualPage>::Element * first,
		const Volumes::ObjectDictionary<string, XV::ManualPage>::Element * last, const string & prefix, int prefix_length)
	{
		if (!first) return;
		Volumes::Set<string> prohibit_prefixes;
		while (true) {
			bool discard = false;
			auto & path = first->GetValue().key;
			if (path.Fragment(0, prefix_length) != prefix) discard = true;
			if (!discard) for (auto p : prohibit_prefixes) if (path.Fragment(0, p.Length()) == p) { discard = true; break; }
			if (!discard) {
				auto name = path.Fragment(prefix_length, -1);
				auto new_item = item->AddItem(name, first->GetValue().value.Inner());
				if (first->GetValue().value.Inner() == current_page) {
					auto current_item = new_item;
					while (current_item) {
						current_item->Expand(true);
						current_item = current_item->GetParent();
					}
					view->SetSelectedItem(new_item, true);
				}
				auto subpref = path;
				if (first->GetValue().value->GetClass() == XV::ManualPageClass::Function) subpref += L":"; else subpref += L".";
				prohibit_prefixes.AddElement(subpref);
				FillPageList(view, new_item, first->GetNext(), last, subpref, subpref.Length());
			}
			if (first != last) first = first->GetNext(); else break;
		}
	}
	void UpdateTitle(void)
	{
		string file = path.Length() ? IO::Path::GetFileName(path) : *interface.Strings[L"NewVolumeTitle"];
		file += L" \x2013 " + string(ENGINE_VI_APPNAME);
		#ifndef ENGINE_MACOSX
		if (alternated) file = L"\x25CF " + file;
		#endif
		window->SetText(file);
		window->SetCloseButtonState(alternated ? CloseButtonState::Alert : CloseButtonState::Enabled);
	}
	void PerformSave(IDispatchTask * task, bool * result)
	{
		SafePointer<IDispatchTask> on_save;
		on_save.SetRetain(task);
		if (path.Length()) {
			try {
				SafePointer<Stream> stream = new FileStream(path, AccessReadWrite, CreateAlways);
				volume->Save(stream);
				alternated = false;
				UpdateTitle();
				if (result) *result = true;
				if (on_save) on_save->DoTask(0);
			} catch (...) {
				GetWindowSystem()->MessageBox(0, FormatString(*interface.Strings[L"SaveFileFailure"], path), ENGINE_VI_APPNAME,
					window, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, CreateFunctionalTask([on_save, rv = result]() {
					if (rv) *rv = false;
					if (on_save) on_save->DoTask(0);
				}));
			}
		} else {
			auto save_as_task = CreateStructuredTask<bool>([this, on_save, rv = result](bool save_as_result) {
				if (save_as_result && path.Length()) {
					PerformSave(on_save, rv);
				} else {
					if (rv) *rv = false;
					if (on_save) on_save->DoTask(0);
				}
			});
			PerformSaveAs(save_as_task, &save_as_task->Value1);
		}
	}
	void PerformSaveAs(IDispatchTask * task, bool * result)
	{
		SafePointer<IDispatchTask> on_save;
		on_save.SetRetain(task);
		auto hdlr = CreateStructuredTask<SaveFileInfo>([this, on_save, rv = result](const SaveFileInfo & info) {
			if (info.File.Length()) {
				path = info.File;
				UpdateTitle();
				if (rv) *rv = true;
				if (on_save) on_save->DoTask(0);
			} else {
				if (rv) *rv = false;
				if (on_save) on_save->DoTask(0);
			}
		});
		hdlr->Value1.AppendExtension = true;
		hdlr->Value1.Formats << FileFormat();
		hdlr->Value1.Formats.LastElement().Description = L"Manualis Linguae XV";
		hdlr->Value1.Formats.LastElement().Extensions << L"xvm";
		GetWindowSystem()->SaveFileDialog(&hdlr->Value1, window, hdlr);
	}
	void UpdatePageList(void)
	{
		auto list = FindControl(window, 1001)->As<Controls::TreeView>();
		list->ClearItems();
		FillPageList(list, list->GetRootItem(), volume->GetPages().GetFirst(), volume->GetPages().GetLast(), L"", 0);
		UpdatePage();
	}
	void UpdatePage(void)
	{
		auto edit = FindControl(window, 1002);
		auto list = FindControl(window, 1101)->As<Controls::ListBox>();
		auto cb0 = FindControl(window, 1010)->As<Controls::CheckBox>();
		auto cb1 = FindControl(window, 1011)->As<Controls::CheckBox>();
		auto cb2 = FindControl(window, 1012)->As<Controls::CheckBox>();
		auto cb3 = FindControl(window, 1013)->As<Controls::CheckBox>();
		auto cb4 = FindControl(window, 1014)->As<Controls::CheckBox>();
		auto cb5 = FindControl(window, 1015)->As<Controls::CheckBox>();
		auto cb6 = FindControl(window, 1016)->As<Controls::CheckBox>();
		auto cb7 = FindControl(window, 1017)->As<Controls::CheckBox>();
		auto cb8 = FindControl(window, 1018)->As<Controls::CheckBox>();
		if (current_page) {
			auto trt = current_page->GetTraits();
			edit->SetText(current_page->GetTitle()); edit->Enable(true);
			cb0->Check(trt & XV::ManualPageThrows); cb0->Enable(true);
			cb1->Check(trt & XV::ManualPageInstance); cb1->Enable(true);
			cb2->Check(trt & XV::ManualPagePropRead); cb2->Enable(true);
			cb3->Check(trt & XV::ManualPagePropWrite); cb3->Enable(true);
			cb4->Check(trt & XV::ManualPageVirtual); cb4->Enable(true);
			cb5->Check(trt & XV::ManualPageInterface); cb5->Enable(true);
			cb6->Check(trt & XV::ManualPageConstructor); cb6->Enable(true);
			cb7->Check(trt & XV::ManualPageOperator); cb7->Enable(true);
			cb8->Check(trt & XV::ManualPageConvertor); cb8->Enable(true);
			list->ClearItems(); list->Enable(true);
			for (auto & s : current_page->GetSections()) {
				string base;
				if (s.GetClass() == XV::ManualSectionClass::ObjectType) base = *interface.Strings[L"SectObjType"];
				else if (s.GetClass() == XV::ManualSectionClass::Summary) base = *interface.Strings[L"SectSummary"];
				else if (s.GetClass() == XV::ManualSectionClass::Details) base = *interface.Strings[L"SectDetails"];
				else if (s.GetClass() == XV::ManualSectionClass::ArgumentSection) base = *interface.Strings[L"SectArg"];
				else if (s.GetClass() == XV::ManualSectionClass::ResultSection) base = *interface.Strings[L"SectRetVal"];
				else if (s.GetClass() == XV::ManualSectionClass::ThrowRules) base = *interface.Strings[L"SectThRules"];
				else if (s.GetClass() == XV::ManualSectionClass::ContextRules) base = *interface.Strings[L"SectCxRules"];
				else if (s.GetClass() == XV::ManualSectionClass::Inheritance) base = *interface.Strings[L"SectInherit"];
				else if (s.GetClass() == XV::ManualSectionClass::CastSection) base = *interface.Strings[L"SectCast"];
				else if (s.GetClass() == XV::ManualSectionClass::DynamicCastSection) base = *interface.Strings[L"SectDynCast"];
				else base = *interface.Strings[L"SectUnk"];
				list->AddItem(FormatString(base, string(s.GetSubjectIndex()) + string(L" - ") + s.GetSubjectName()));
				if (&s == current_section) list->SetSelectedIndex(list->ItemCount() - 1, true);
			}
		} else {
			edit->SetText(L""); edit->Enable(false);
			list->ClearItems(); list->Enable(false);
			cb0->Check(false); cb0->Enable(false);
			cb1->Check(false); cb1->Enable(false);
			cb2->Check(false); cb2->Enable(false);
			cb3->Check(false); cb3->Enable(false);
			cb4->Check(false); cb4->Enable(false);
			cb5->Check(false); cb5->Enable(false);
			cb6->Check(false); cb6->Enable(false);
			cb7->Check(false); cb7->Enable(false);
			cb8->Check(false); cb8->Enable(false);
		}
		UpdateSection();
	}
	void UpdateSection(void)
	{
		lang_map.Clear();
		auto scrollbox = FindControl(window, 1201)->As<Controls::ScrollBox>();
		auto view = scrollbox->GetVirtualGroup();
		while (view->ChildrenCount()) view->RemoveChildAt(0);
		if (current_section) {
			Volumes::Set<string> lng;
			if (IsTypeSection(current_section)) lng.AddElement(L"");
			else lng = GetLanguages(volume);
			for (auto & l : lng) if (!current_section->GetContents().ElementExists(l)) {
				alternated = true;
				UpdateTitle();
				current_section->SetContents(l, L"");
			}
			int base_id = 10000;
			Coordinate current(0), label(0, 28.0, 0.0), edit(0, 350.0, 0.0);
			for (auto & v : current_section->GetContents()) {
				SafePointer<Controls::Static> ctl_label = new Controls::Static(interface.Dialog[L"BaseStatic"]);
				SafePointer<Controls::MultiLineEdit> ctl_edit = new Controls::MultiLineEdit(interface.Dialog[L"BaseMultiLine"]);
				ctl_label->SetText(v.key);
				ctl_label->SetRectangle(Rectangle(0, current, Coordinate::Right(), current + label));
				current += label;
				ctl_edit->SetID(base_id);
				ctl_edit->SetText(IsTypeSection(current_section) ? v.value.Replace(L'\33', L'\n') : v.value);
				ctl_edit->SetRectangle(Rectangle(0, current, Coordinate::Right(), current + edit));
				current += edit;
				lang_map.Append(base_id, v.key); base_id++;
				view->AddChild(ctl_label);
				view->AddChild(ctl_edit);
			}
			scrollbox->ArrangeChildren();
		}
	}
public:
	EditorCallback(const string & file, XV::ManualVolume * manual) : alternated(false), path(file)
	{
		current_page = 0;
		current_section = 0;
		volume.SetRetain(manual);
		auto wnd = CreateWindow(interface.Dialog[L"Editor"], this, Rectangle::Entire());
		RegisterWindow(wnd, this);
		wnd->Show(true);
	}
	virtual void Created(IWindow * wnd) override
	{
		window = wnd;
		auto & accel = GetRootControl(window)->GetAcceleratorTable();
		accel << Accelerators::AcceleratorCommand(901, KeyCodes::N);
		accel << Accelerators::AcceleratorCommand(901, KeyCodes::F1, false);
		accel << Accelerators::AcceleratorCommand(102, KeyCodes::O);
		accel << Accelerators::AcceleratorCommand(101, KeyCodes::N, true, true);
		accel << Accelerators::AcceleratorCommand(103, KeyCodes::S);
		accel << Accelerators::AcceleratorCommand(104, KeyCodes::S, true, true);
		accel << Accelerators::AcceleratorCommand(501, KeyCodes::F5, false);
		accel << Accelerators::AcceleratorCommand(502, KeyCodes::F5, false, true);
		accel << Accelerators::AcceleratorCommand(208, KeyCodes::F3, false);
		UpdatePageList();
		UpdateTitle();
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * wnd) override
	{
		if (alternated) {
			auto task = CreateStructuredTask<MessageBoxResult>([this](MessageBoxResult result) {
				if (result == MessageBoxResult::No) {
					UnregisterWindow(window); window->Destroy();
				} else if (result == MessageBoxResult::Yes) {
					auto after_save = CreateStructuredTask<bool>([this](bool result) {
						if (result) { UnregisterWindow(window); window->Destroy(); }
					});
					PerformSave(after_save, &after_save->Value1);
				}
			});
			GetWindowSystem()->MessageBox(&task->Value1, *interface.Strings[L"SaveChangesConf"], ENGINE_VI_APPNAME, window,
				MessageBoxButtonSet::YesNoCancel, MessageBoxStyle::Warning, task);
			
		} else { UnregisterWindow(window); window->Destroy(); }
	}
	virtual bool IsWindowEventEnabled(IWindow * window, WindowHandler handler) override
	{
		if (handler == WindowHandler::Save) return true;
		else if (handler == WindowHandler::SaveAs) return true;
		else return false;
	}
	virtual void HandleWindowEvent(IWindow * window, WindowHandler handler) override
	{
		if (handler == WindowHandler::Save) PerformSave(0, 0);
		else if (handler == WindowHandler::SaveAs) PerformSaveAs(0, 0);
	}
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 101) {
			CreateEditor();
		} else if (ID == 102) {
			GetWindowSystem()->GetCallback()->OpenSomeFile();
		} else if (ID == 103) {
			PerformSave(0, 0);
		} else if (ID == 104) {
			PerformSaveAs(0, 0);
		} else if (ID == 2) {
			WindowClose(window);
		} else if (ID == 201) {
			new AddPageCallback(window, volume, CreateFunctionalTask([this]() {
				alternated = true;
				UpdateTitle();
				UpdatePageList();
			}));
		} else if (ID == 202) {
			if (current_page) new AddSectionCallback(window, current_page, CreateFunctionalTask([this]() {
				alternated = true;
				UpdateTitle();
				UpdatePage();
			}));
		} else if (ID == 203) {
			new AddLanguageCallback(window, volume, CreateFunctionalTask([this]() {
				alternated = true;
				UpdateTitle();
				UpdatePage();
			}));
		} else if (ID == 204) {
			if (current_page) {
				volume->RemovePage(current_page->GetPath());
				current_page = 0;
				current_section = 0;
				alternated = true;
				UpdatePageList();
				UpdateTitle();
			}
		} else if (ID == 205) {
			if (current_page && current_section) {
				current_page->RemoveSection(current_section->GetClass(), current_section->GetSubjectIndex());
				current_section = 0;
				alternated = true;
				UpdatePage();
				UpdateTitle();
			}
		} else if (ID == 206) {
			new RemoveLanguageCallback(window, volume, CreateFunctionalTask([this]() {
				alternated = true;
				UpdateTitle();
				UpdatePage();
			}));
		} else if (ID == 207) {
			new SetModuleNameCallback(window, volume, CreateFunctionalTask([this]() {
				alternated = true;
				UpdateTitle();
			}));
		} else if (ID == 208) {
			XV::ManualPage * select = 0;
			for (auto & p : volume->GetPages()) {
				auto summary = p.value->FindSection(XV::ManualSectionClass::Summary);
				if (summary) {
					for (auto & l : summary->GetContents()) if (!l.value.Length()) { select = p.value; break; }
					if (summary->GetContents().IsEmpty()) select = p.value;
					if (select) break;
				} else { select = p.value; break; }
			}
			if (select) {
				current_page = select; current_section = 0;
				UpdatePageList();
			}
		} else if (ID == 501) {
			if (!current_page) HandleControlEvent(window, 502, ControlEvent::AcceleratorCommand, 0);
			else CreateBrowser(current_page->GetPath(), volume.Inner());
		} else if (ID == 502) {
			CreateBrowser(L".categoriae", volume.Inner());
		} else if (ID == 901) {
			GetWindowSystem()->GetCallback()->CreateNewFile();
		} else if (ID == 1001 && event == ControlEvent::ValueChange) {
			auto selected = FindControl(window, 1001)->As<Controls::TreeView>()->GetSelectedItem();
			current_page = selected ? reinterpret_cast<XV::ManualPage *>(selected->User) : 0;
			current_section = 0;
			UpdatePage();
		} else if (ID == 1002 && event == ControlEvent::ValueChange) {
			if (current_page) {
				current_page->SetTitle(sender->GetText());
				alternated = true;
				UpdateTitle();
			}
		} else if (ID >= 1010 && ID <= 1018) {
			if (current_page) {
				uint trt = 0;
				if (FindControl(window, 1010)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPageThrows;
				if (FindControl(window, 1011)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPageInstance;
				if (FindControl(window, 1012)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPagePropRead;
				if (FindControl(window, 1013)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPagePropWrite;
				if (FindControl(window, 1014)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPageVirtual;
				if (FindControl(window, 1015)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPageInterface;
				if (FindControl(window, 1016)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPageConstructor;
				if (FindControl(window, 1017)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPageOperator;
				if (FindControl(window, 1018)->As<Controls::CheckBox>()->Checked) trt |= XV::ManualPageConvertor;
				current_page->SetTraits(trt);
				alternated = true;
				UpdateTitle();
			}
		} else if (ID == 1101 && event == ControlEvent::ValueChange) {
			if (current_page) {
				auto selected = FindControl(window, 1101)->As<Controls::ListBox>()->GetSelectedIndex();
				current_section = &current_page->GetSections()[selected];
				UpdateSection();
			}
		} else if (event == ControlEvent::ValueChange) {
			auto lc = lang_map.GetElementByKey(ID);
			if (current_page && current_section && lc) {
				if (IsTypeSection(current_section)) {
					auto cont = FindControl(window, ID)->GetText();
					cont = cont.Replace(L'\r', L"");
					cont = cont.Replace(L'\n', L'\33');
					current_section->SetContents(*lc, cont);
				} else current_section->SetContents(*lc, FindControl(window, ID)->GetText());
				alternated = true;
				UpdateTitle();
			}
		}
	}
};

void CreateEditor(void)
{
	SafePointer<XV::ManualVolume> volume = new XV::ManualVolume;
	new EditorCallback(L"", volume);
}
bool CreateEditor(const Engine::ImmutableString & path)
{
	SafePointer<XV::ManualVolume> volume;
	try {
		SafePointer<Stream> stream = new FileStream(path, AccessRead, OpenExisting);
		volume = new XV::ManualVolume(stream);
		new EditorCallback(path, volume);
		return true;
	} catch (...) {
		try {
			SafePointer<Stream> stream = new FileStream(path, AccessRead, OpenExisting);
			if (stream->Length() > 0x100000) throw InvalidArgumentException();
			SafePointer<MemoryStream> copy = new MemoryStream(0x10000);
			stream->CopyTo(copy);
			copy->Seek(0, Begin);
			SafePointer<TextReader> rdr = new TextReader(copy);
			DynamicString contents;
			while (!rdr->EofReached()) {
				auto chr = rdr->ReadChar();
				if (chr != 0xFFFFFFFF) {
					if (chr < 32 && chr != L'\t' && chr != L'\r' && chr != L'\n') throw InvalidArgumentException();
					if (chr == L'\\') contents << L"\\\\";
					else if (chr == L'{') contents << L"\\{";
					else if (chr == L'}') contents << L"\\}";
					else if (chr < 0x10000) contents << widechar(chr);
					else contents << string(&chr, 1, Encoding::UTF32);
				}
			}
			CreateBrowser(path, contents.ToString());
			return true;
		} catch (...) {
			GetWindowSystem()->MessageBox(0, FormatString(*interface.Strings[L"OpenFileFailure"], path), ENGINE_VI_APPNAME,
				0, MessageBoxButtonSet::Ok, MessageBoxStyle::Warning, 0);
			return false;
		}
	}
}