#include "xvm_browser.h"
#include "xvm_editor.h"

#include "../xv/xv_manual.h"
#include "../xv/xv_meta.h"
#include "../ximg/xi_module.h"
#include "../xexec/xx_com.h"

using namespace Engine;
using namespace Engine::UI;
using namespace Engine::Windows;

class EngineTextFormatter : public XV::IManualTextFormatter
{
public:
	virtual string PlainText(const string & text) override
	{
		DynamicString result;
		int pos = 0;
		while (text[pos]) {
			while (text[pos] && (text[pos] == L' ' || text[pos] == L'\t' || text[pos] == L'\r' || text[pos] == L'\n')) pos++;
			int start = pos;
			while (text[pos] && text[pos] != L' ' && text[pos] != L'\t' && text[pos] != L'\r' && text[pos] != L'\n') pos++;
			if (result.Length()) result += L' ';
			if (!result.Length() && start) result += L" ";
			result += text.Fragment(start, pos - start);
		}
		return result;
	}
	virtual string BoldText(const string & inner) override { return L"\33b" + inner + L"\33e"; }
	virtual string ItalicText(const string & inner) override { return L"\33i" + inner + L"\33e"; }
	virtual string UnderlinedText(const string & inner) override { return L"\33u" + inner + L"\33e"; }
	virtual string LinkedText(const string & inner, const string & to) override { return L"\33l" + to + L"\33" + inner + L"\33e"; }
	virtual string Paragraph(const string & inner) override { return L"\n\33a1\t" + inner + L"\33e"; }
	virtual string ListItem(int index, const string & inner) override { return L"\n\33a1\33cFF803030" + string(index) + L".\33e " + inner + L"\33e"; }
	virtual string Code(const string & inner) override { return L"\33f01" + inner + L"\33e"; }
};
class PlainTextFormatter : public XV::IManualTextFormatter
{
public:
	virtual string PlainText(const string & text) override
	{
		DynamicString result;
		int pos = 0;
		while (text[pos]) {
			while (text[pos] && (text[pos] == L' ' || text[pos] == L'\t' || text[pos] == L'\r' || text[pos] == L'\n')) pos++;
			int start = pos;
			while (text[pos] && text[pos] != L' ' && text[pos] != L'\t' && text[pos] != L'\r' && text[pos] != L'\n') pos++;
			if (result.Length()) result += L' ';
			if (!result.Length() && start) result += L" ";
			result += text.Fragment(start, pos - start);
		}
		return result;
	}
	virtual string BoldText(const string & inner) override { return inner; }
	virtual string ItalicText(const string & inner) override { return inner; }
	virtual string UnderlinedText(const string & inner) override { return inner; }
	virtual string LinkedText(const string & inner, const string & to) override { return inner; }
	virtual string Paragraph(const string & inner) override { return L"\n\t" + inner; }
	virtual string ListItem(int index, const string & inner) override { return L"\n" + string(index) + L". " + inner; }
	virtual string Code(const string & inner) override { return inner; }
};
class BrowserCallback : public IEventCallback, public Controls::RichEdit::IRichEditHook
{
	IWindow * window;
	SafePointer<XV::ManualVolume> volume;
	string def_lang, path;
	Array<string> history;
	int current_page;
	
	string DecorateTypeLink(const XI::Module::TypeReference & ref)
	{
		if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
			auto cls = ref.GetClassName();
			if (volume->FindPage(cls)) return L"\\l{" + cls + L"}{" + cls + L"}";
			else return cls;
		} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Array) {
			return L"ordo [" + string(ref.GetArrayVolume()) + L"] " + DecorateTypeLink(ref.GetArrayElement());
		} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) {
			return L"@" + DecorateTypeLink(ref.GetPointerDestination());
		} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
			return L"~" + DecorateTypeLink(ref.GetReferenceDestination());
		} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Function) {
			SafePointer< Array<XI::Module::TypeReference> > sgn = ref.GetFunctionSignature();
			DynamicString result;
			result << L"functio " << DecorateTypeLink(sgn->ElementAt(0)) << L"(";
			for (int i = 1; i < sgn->Length(); i++) {
				if (i > 1) result << L", ";
				result << DecorateTypeLink(sgn->ElementAt(i));
			}
			result << L")";
			return result;
		} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::AbstractInstance) {
			XI::Module::TypeReference rf = ref;
			Array<string> args(0x10);
			while (rf.GetReferenceClass() == XI::Module::TypeReference::Class::AbstractInstance) {
				args << DecorateTypeLink(rf.GetAbstractInstanceParameterType());
				auto next = rf.GetAbstractInstanceBase();
				MemoryCopy(&rf, &next, sizeof(rf));
			}
			DynamicString result;
			result << DecorateTypeLink(rf) << L"[";
			for (int i = args.Length() - 1; i >= 0; i--) {
				result << args[i];
				if (i) result << L", ";
			}
			result << L"]";
			return result;
		} else return L"";
	}
	string DecorateTypeLink(const string & tcn) { try { return DecorateTypeLink(XI::Module::TypeReference(tcn)); } catch (...) { return L""; } }
	string DecorateObjectLink(const string & object)
	{
		DynamicString result, unified;
		auto prt = object.Split(L'.');
		for (int i = 0; i < prt.Length(); i++) {
			if (i) { unified += L"."; result += L"."; }
			unified += prt[i];
			if (prt[i] == L"@crea") {
				result << L"\\i{" << XV::Lexic::KeywordCtor << L"}";
			} else if (prt[i] == L"@converte") {
				result << L"\\i{" << XV::Lexic::KeywordConvertor << L"}";
			} else {
				auto dest = i < prt.Length() - 1 ? volume->FindPage(unified) : 0;
				if (dest) result += L"\\l{";
				result += prt[i];
				if (dest) result << L"}{" << dest->GetPath() << L"}";
			}
		}
		return result;
	}
	string GetSectionContents(const XV::ManualSection * sect, const string & lang)
	{
		auto str = sect->GetContents(lang);
		if (str.Length() || !lang.Length()) return str;
		return sect->GetContents(def_lang);
	}
	string MakeCompleteTitle(XV::ManualPage * page)
	{
		try {
			auto cls = page->GetClass();
			if (cls == XV::ManualPageClass::Alias) {
				return string(XV::Lexic::KeywordAlias) + L" " + DecorateObjectLink(page->GetTitle());
			} else if (cls == XV::ManualPageClass::Constant || cls == XV::ManualPageClass::Variable || cls == XV::ManualPageClass::Field || cls == XV::ManualPageClass::Property) {
				DynamicString result;
				auto type = page->FindSection(XV::ManualSectionClass::ObjectType);
				if (type) result << DecorateTypeLink(GetSectionContents(type, L"")) << L" ";
				result << DecorateObjectLink(page->GetTitle());
				if (cls == XV::ManualPageClass::Constant) return string(XV::Lexic::KeywordConst) + L" " + result.ToString();
				else if (cls == XV::ManualPageClass::Variable) return string(XV::Lexic::KeywordVariable) + L" " + result.ToString();
				else if (cls == XV::ManualPageClass::Field) return result.ToString() + L" (" + *interface.Strings[L"SymbolField"] + L")";
				else if (cls == XV::ManualPageClass::Property) return result.ToString() + L" (" + *interface.Strings[L"SymbolProperty"] + L")";
				else return result;
			} else if (cls == XV::ManualPageClass::Class) {
				if (page->GetTraits() & XV::ManualPageInterface) return string(XV::Lexic::KeywordInterface) + L" " + DecorateObjectLink(page->GetTitle());
				else return string(XV::Lexic::KeywordClass) + L" " + DecorateObjectLink(page->GetTitle());
			} else if (cls == XV::ManualPageClass::Namespace) {
				return string(XV::Lexic::KeywordNamespace) + L" " + DecorateObjectLink(page->GetTitle());
			} else if (cls == XV::ManualPageClass::Prototype) {
				return string(XV::Lexic::KeywordPrototype) + L" " + DecorateObjectLink(page->GetTitle());
			} else if (cls == XV::ManualPageClass::Function) {
				DynamicString result;
				result << string(XV::Lexic::KeywordFunction) << L" ";
				auto type = page->FindSection(XV::ManualSectionClass::ObjectType);
				if (type) {
					auto cn = GetSectionContents(type, L"");
					auto ref = XI::Module::TypeReference(cn);
					SafePointer< Array<XI::Module::TypeReference> > sgn = ref.GetFunctionSignature();
					result << DecorateTypeLink(sgn->ElementAt(0)) << L" " << DecorateObjectLink(page->GetTitle()) << L"(";
					for (int i = 1; i < sgn->Length(); i++) {
						if (i > 1) result << L", ";
						result << DecorateTypeLink(sgn->ElementAt(i));
						auto ai = page->FindSection(XV::ManualSectionClass::ArgumentSection, i - 1);
						if (ai) result << L" " << ai->GetSubjectName();
					}
					result << L")";
				} else result << DecorateObjectLink(page->GetTitle());
				return result;
			} else return page->GetTitle();
		} catch (...) { return L""; }
	}
	string MakeCompleteTitle(XV::ManualPage * page, bool plain)
	{
		if (plain) {
			PlainTextFormatter fmt;
			return XV::FormatRichText(MakeCompleteTitle(page), &fmt);
		} else {
			EngineTextFormatter fmt;
			return XV::FormatRichText(MakeCompleteTitle(page), &fmt);
		}
	}
	string MakeBriefTitle(XV::ManualPage * page, bool plain)
	{
		if (plain) {
			PlainTextFormatter fmt;
			return XV::FormatRichText(DecorateObjectLink(page->GetTitle()), &fmt);
		} else {
			EngineTextFormatter fmt;
			return XV::FormatRichText(DecorateObjectLink(page->GetTitle()), &fmt);
		}
	}
	string MakeSectionContents(const XV::ManualSection * sect)
	{
		EngineTextFormatter fmt;
		auto text = GetSectionContents(sect, Assembly::CurrentLocale);
		if (text.Length()) return XV::FormatRichText(text, &fmt);
		return XV::FormatRichText(GetSectionContents(sect, L""), &fmt);
	}
	string Format(const string & text)
	{
		EngineTextFormatter fmt;
		return XV::FormatRichText(text, &fmt);
	}
	bool NotEmpty(const XV::ManualSection * sect) { return GetSectionContents(sect, L"").Length() || GetSectionContents(sect, Assembly::CurrentLocale).Length() || sect->GetSubjectName().Length(); }
	void LoadPage(void)
	{
		auto path_ctl = FindControl(window, 103)->As<Controls::Edit>();
		auto edit = FindControl(window, 201)->As<Controls::RichEdit>();
		path_ctl->ReadOnly = true;
		if (path == L".quaere") {
			path_ctl->ReadOnly = false;
			path_ctl->SetText(L"");
			path_ctl->SetFocus();
			window->SetText(*interface.Strings[L"SearchTitle"]);
			edit->SetAttributedText(L"");
		} else if (path == L".categoriae" && volume) {
			Volumes::Set<string> mdl;
			Volumes::Dictionary<string, XV::ManualPage *> common;
			Volumes::Dictionary<string, XV::ManualPage *> all;
			for (auto & page : volume->GetPages()) {
				if (page.key[0] == L'.') common.Append(page.key, page.value); else {
					all.Append(page.key, page.value);
					if (page.value->GetModule().Length()) mdl.AddElement(page.value->GetModule());
				}
			}
			path_ctl->SetText(path);
			auto title = *interface.Strings[L"IndexTitle"];
			window->SetText(title);
			DynamicString rt;
			rt << L"\33n*\33e\33n" << Graphics::SystemMonoSansSerifFont << L"\33e\33f00\33c********";
			rt << L"\33h001E\33a2";
			rt << title;
			rt << L"\n\33e\33e";
			if (!mdl.IsEmpty()) {
				rt << L"\33h001E\33a1";
				rt << *interface.Strings[L"IndexModules"];
				rt << L"\n\33e\33e";
				rt << L"\33h0014\33a1";
				for (auto & m : mdl) rt << L"\33l.modulus:" << m << L"\33" << m << L"\33e\n";
				rt << L"\33e\33e";
			}
			if (!common.IsEmpty()) {
				rt << L"\33h001E\33a1";
				rt << *interface.Strings[L"IndexCommons"];
				rt << L"\n\33e\33e";
				rt << L"\33h0014\33a1";
				for (auto & o : common) {
					auto com = MakeCompleteTitle(o.value, true);
					rt << L"\33l" << o.value->GetPath() << L"\33" << MakeBriefTitle(o.value, true) << L"\33e";
					if (com != MakeBriefTitle(o.value, true)) rt << L" (" << com << L")";
					rt << L"\n";
				}
				rt << L"\33e\33e";
			}
			if (!all.IsEmpty()) {
				rt << L"\33h001E\33a1";
				rt << *interface.Strings[L"IndexObjects"];
				rt << L"\n\33e\33e";
				rt << L"\33h0014\33a1";
				for (auto & o : all) {
					auto com = MakeCompleteTitle(o.value, true);
					rt << L"\33l" << o.value->GetPath() << L"\33" << MakeBriefTitle(o.value, true) << L"\33e";
					if (com != MakeBriefTitle(o.value, true)) rt << L" (" << com << L")";
					rt << L"\n";
				}
				rt << L"\33e\33e";
			}
			rt << L"\33e\33e";
			edit->SetAttributedText(rt);
		} else if (path.Fragment(0, 9) == L".modulus:" && volume) {
			auto mdl = path.Fragment(9, -1);
			Volumes::Dictionary<string, XV::ManualPage *> smbl;
			for (auto & page : volume->GetPages()) {
				if (page.key[0] != L'.' && page.value->GetModule() == mdl) smbl.Append(page.key, page.value);
			}
			path_ctl->SetText(path);
			auto title = FormatString(*interface.Strings[L"IndexOfModule"], mdl);
			window->SetText(title);
			DynamicString rt;
			rt << L"\33n*\33e\33n" << Graphics::SystemMonoSansSerifFont << L"\33e\33f00\33c********";
			rt << L"\33h001E\33a1";
			rt << title;
			rt << L"\n\33e\33e";
			if (!smbl.IsEmpty()) {
				rt << L"\33h0014\33a1";
				for (auto & o : smbl) {
					auto com = MakeCompleteTitle(o.value, true);
					rt << L"\33l" << o.value->GetPath() << L"\33" << MakeBriefTitle(o.value, true) << L"\33e";
					if (com != MakeBriefTitle(o.value, true)) rt << L" (" << com << L")";
					rt << L"\n";
				}
				rt << L"\33e\33e";
			}
			rt << L"\33e\33e";
			edit->SetAttributedText(rt);
		} else {
			path_ctl->SetText(path);
			auto page = volume ? volume->FindPage(path) : 0;
			if (page) {
				string fsgn_cn;
				SafePointer< Array<XI::Module::TypeReference> > fsgn;
				Volumes::Dictionary<string, XV::ManualPage *> children;
				for (auto & page : volume->GetPages()) {
					if (page.key.Fragment(0, path.Length()) == path && page.key.Length() > path.Length() && (page.key[path.Length()] == L'.' || page.key[path.Length()] == L':')) {
						bool add = true;
						for (auto & c : children) if (page.key.Fragment(0, c.key.Length()) == c.key && (page.key[c.key.Length()] == L'.' || page.key[c.key.Length()] == L':')) { add = false; break; }
						if (add) children.Append(page.key, page.value);
					}
				}
				if (page->GetClass() == XV::ManualPageClass::Function) {
					try {
						auto type = page->FindSection(XV::ManualSectionClass::ObjectType);
						if (type) {
							fsgn_cn = GetSectionContents(type, L"");
							fsgn = XI::Module::TypeReference(fsgn_cn).GetFunctionSignature();
						}
					} catch (...) {}
				}
				auto title_plain = MakeCompleteTitle(page, true);
				auto title = MakeCompleteTitle(page, false);
				window->SetText(title_plain);
				DynamicString rt;
				rt << L"\33n*\33e\33n" << Graphics::SystemMonoSansSerifFont << L"\33e\33f00\33c********";
				rt << L"\33h001E\33a1";
				rt << title;
				rt << L"\n\33e\33e";
				if (page->GetTraits()) rt << L"\33h0014\33a1";
				if (page->GetTraits() & XV::ManualPageThrows) rt << L"\33b\33cFF0000FF" << *interface.Strings[L"SymbolThrows"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPageInstance) rt << L"\33b\33cFF20C000" << *interface.Strings[L"SymbolInstance"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPagePropRead) rt << L"\33b\33cFFFF8000" << *interface.Strings[L"SymbolRead"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPagePropWrite) rt << L"\33b\33cFFFF0080" << *interface.Strings[L"SymbolWrite"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPageVirtual) rt << L"\33b\33cFFFF0000" << *interface.Strings[L"SymbolVirtual"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPageInterface) rt << L"\33b\33cFFFF0000" << *interface.Strings[L"SymbolInterface"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPageConstructor) rt << L"\33b\33cFF404040" << *interface.Strings[L"SymbolCtor"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPageOperator) rt << L"\33b\33cFF404040" << *interface.Strings[L"SymbolOperator"] << L"\33e\33e ";
				if (page->GetTraits() & XV::ManualPageConvertor) rt << L"\33b\33cFF404040" << *interface.Strings[L"SymbolConvertor"] << L"\33e\33e ";
				if (page->GetTraits()) rt << L"\n\33e\33e";
				if (page->GetModule().Length() && path[0] != L'.') {
					rt << L"\33h0014\33a1\33cFF404040" << *interface.Strings[L"SymbolModule"] << L": ";
					rt << L"\33l.modulus:" << page->GetModule() << L"\33" << page->GetModule() << L"\33e";
					rt << L"\n\33e\33e\33e";
				}
				for (auto & s : page->GetSections()) if (NotEmpty(&s)) {
					if (s.GetClass() == XV::ManualSectionClass::ObjectType) {
					} else if (s.GetClass() == XV::ManualSectionClass::Inheritance) {
						rt << L"\33h001E\33a1";
						rt << *interface.Strings[L"SymbolInherits"];
						rt << L"\n\33e\33e";
						rt << L"\33h0014\33a1";
						auto tl = GetSectionContents(&s, L"").Split(L'\33');
						for (auto & t : tl) if (t.Length()) rt << Format(DecorateTypeLink(t)) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::CastSection) {
						rt << L"\33h001E\33a1";
						rt << *interface.Strings[L"SymbolConforms"];
						rt << L"\n\33e\33e";
						rt << L"\33h0014\33a1";
						auto tl = GetSectionContents(&s, L"").Split(L'\33');
						for (auto & t : tl) if (t.Length()) rt << Format(DecorateTypeLink(t)) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::DynamicCastSection) {
						rt << L"\33h001E\33a1";
						rt << *interface.Strings[L"SymbolDynamic"];
						rt << L"\n\33e\33e";
						rt << L"\33h0014\33a1";
						auto tl = GetSectionContents(&s, L"").Split(L'\33');
						for (auto & t : tl) if (t.Length()) rt << Format(DecorateTypeLink(t)) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::Summary) {
						rt << L"\33h0014\33a1";
						rt << MakeSectionContents(&s) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::Details) {
						rt << L"\33h001E\33a1";
						rt << *interface.Strings[L"SymbolDetails"];
						rt << L"\n\33e\33e";
						rt << L"\33h0014\33a1";
						rt << MakeSectionContents(&s) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::ArgumentSection) {
						if (s.GetSubjectIndex() == 0) {
							rt << L"\33h001E\33a1";
							rt << *interface.Strings[L"SymbolArguments"];
							rt << L"\n\33e\33e";
						}
						rt << L"\33h0014\33a1\33b";
						if (fsgn) rt << Format(DecorateTypeLink(fsgn->ElementAt(1 + s.GetSubjectIndex()))) << L" ";
						rt << s.GetSubjectName();
						rt << L"\n\33e";
						rt << MakeSectionContents(&s) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::ResultSection) {
						rt << L"\33h001E\33a1";
						rt << *interface.Strings[L"SymbolResult"];
						rt << L"\n\33e\33e";
						rt << L"\33h0014\33a1";
						if (fsgn) rt << L"\33b" << Format(DecorateTypeLink(fsgn->ElementAt(0))) << L"\n\33e";
						rt << MakeSectionContents(&s) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::ThrowRules) {
						rt << L"\33h001E\33a1";
						rt << *interface.Strings[L"SymbolThRules"];
						rt << L"\n\33e\33e";
						rt << L"\33h0014\33a1";
						rt << MakeSectionContents(&s) << L"\n";
						rt << L"\33e\33e";
					} else if (s.GetClass() == XV::ManualSectionClass::ContextRules) {
						rt << L"\33h001E\33a1";
						rt << *interface.Strings[L"SymbolCxRules"];
						rt << L"\n\33e\33e";
						rt << L"\33h0014\33a1";
						rt << MakeSectionContents(&s) << L"\n";
						rt << L"\33e\33e";
					}
				}
				if (!children.IsEmpty()) {
					rt << L"\33h001E\33a1";
					rt << *interface.Strings[L"SymbolElements"];
					rt << L"\n\33e\33e";
					rt << L"\33h0014\33a1";
					for (auto & o : children) {
						auto com = MakeCompleteTitle(o.value, true);
						rt << L"\33l" << o.value->GetPath() << L"\33" << com << L"\33e";
						auto summary = o.value->FindSection(XV::ManualSectionClass::Summary);
						if (summary) rt << L"\n" << MakeSectionContents(summary);
						rt << L"\n\n";
					}
					rt << L"\33e\33e";
				}
				rt << L"\33e\33e";
				edit->SetAttributedText(rt);
			} else {
				auto title = *interface.Strings[L"NoSuchPathTitle"];
				auto text = FormatString(*interface.Strings[L"NoSuchPathText"], path);
				window->SetText(title);
				DynamicString rt;
				rt << L"\33n*\33e\33n" << Graphics::SystemMonoSansSerifFont << L"\33e\33f00\33c********";
				rt << L"\33h001E\33a2";
				rt << title;
				rt << L"\n\33e\33e";
				rt << L"\33h0014\33a2";
				rt << text;
				rt << L"\33e\33e";
				rt << L"\33e\33e";
				edit->SetAttributedText(rt);
			}
		}
	}
	void UpdateButtons(void)
	{
		FindControl(window, 101)->Enable(current_page > 0);
		FindControl(window, 102)->Enable(current_page < history.Length());
	}
	void MoveToPage(const string & to)
	{
		while (history.Length() > current_page) history.RemoveLast();
		history << path; current_page++; path = to;
		LoadPage();
		UpdateButtons();
	}
public:
	BrowserCallback(XV::ManualVolume * manual, const string & deflang, const string & open) : def_lang(deflang), path(open), history(0x20)
	{
		current_page = 0;
		volume.SetRetain(manual);
		auto wnd = CreateWindow(interface.Dialog[L"Browser"], this, Rectangle::Entire());
		RegisterWindow(wnd, this);
		wnd->Show(true);
	}
	virtual void Created(IWindow * wnd) override
	{
		window = wnd;
		auto & accel = GetRootControl(window)->GetAcceleratorTable();
		accel << Accelerators::AcceleratorCommand(101, KeyCodes::Back, true);
		accel << Accelerators::AcceleratorCommand(102, KeyCodes::Back, true, true);
		accel << Accelerators::AcceleratorCommand(104, KeyCodes::F1, false);
		accel << Accelerators::AcceleratorCommand(105, KeyCodes::F2, false);
		accel << Accelerators::AcceleratorCommand(106, KeyCodes::F3, false);
		accel << Accelerators::AcceleratorCommand(401, KeyCodes::F5, false);
		accel << Accelerators::AcceleratorCommand(301, KeyCodes::N);
		accel << Accelerators::AcceleratorCommand(302, KeyCodes::O);
		accel << Accelerators::AcceleratorCommand(303, KeyCodes::N, true, true);
		FindControl(window, 201)->As<Controls::RichEdit>()->SetHook(this);
		LoadPage();
		UpdateButtons();
		if (volume->GetPages().Count() < 2) {
			FindControl(window, 103)->Enable(false);
			FindControl(window, 104)->Enable(false);
			FindControl(window, 105)->Enable(false);
			FindControl(window, 106)->Enable(false);
		}
	}
	virtual void Destroyed(IWindow * window) override { delete this; }
	virtual void WindowClose(IWindow * window) override { UnregisterWindow(window); window->Destroy(); }
	virtual bool IsWindowEventEnabled(IWindow * window, WindowHandler handler) override
	{
		if (handler == WindowHandler::Find) return true;
		else return false;
	}
	virtual void HandleWindowEvent(IWindow * window, WindowHandler handler) override { if (handler == WindowHandler::Find) HandleControlEvent(window, 106, ControlEvent::AcceleratorCommand, 0); }
	virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
	{
		if (ID == 101) {
			if (current_page > 0) {
				current_page--;
				swap(history[current_page], path);
				LoadPage();
				UpdateButtons();
			}
		} else if (ID == 102) {
			if (current_page < history.Length()) {
				swap(history[current_page], path);
				current_page++;
				LoadPage();
				UpdateButtons();
			}
		} else if (ID == 103 && event == ControlEvent::ValueChange && path == L".quaere") {
			auto path_ctl = FindControl(window, 103)->As<Controls::Edit>();
			auto edit = FindControl(window, 201)->As<Controls::RichEdit>();
			auto request = path_ctl->GetText();
			auto words = request.Split(L' ');
			Volumes::Dictionary<string, XV::ManualPage *> results;
			for (auto & page : volume->GetPages()) {
				auto title = MakeCompleteTitle(page.value, true);
				bool ok = false;
				for (auto & w : words) if (w.Length()) {
					if (title.FindFirst(w) >= 0) ok = true;
					else { ok = false; break; }
				}
				if (ok) results.Append(page.key, page.value);
			}
			DynamicString rt;
			rt << L"\33n*\33e\33n" << Graphics::SystemMonoSansSerifFont << L"\33e\33f00\33c********";
			if (!results.IsEmpty()) {
				rt << L"\33h0014\33a1";
				for (auto & o : results) {
					auto com = MakeCompleteTitle(o.value, true);
					rt << L"\33l" << o.value->GetPath() << L"\33" << com << L"\33e";
					auto summary = o.value->FindSection(XV::ManualSectionClass::Summary);
					if (summary) rt << L"\n" << MakeSectionContents(summary);
					rt << L"\n\n";
				}
				rt << L"\33e\33e";
			}
			rt << L"\33e\33e";
			edit->SetAttributedText(rt);
		} else if (ID == 104) {
			MoveToPage(L".primus");
		} else if (ID == 105) {
			MoveToPage(L".categoriae");
		} else if (ID == 106) {
			MoveToPage(L".quaere");
		} else if (ID == 301) {
			GetWindowSystem()->GetCallback()->CreateNewFile();
		} else if (ID == 302) {
			GetWindowSystem()->GetCallback()->OpenSomeFile();
		} else if (ID == 303) {
			CreateEditor();
		} else if (ID == 401) {
			LoadPage();
		}
	}
	virtual void InitializeContextMenu(Windows::IMenu * menu, Controls::RichEdit * sender) override {}
	virtual void LinkPressed(const string & resource, Controls::RichEdit * sender) override
	{
		if (resource.Fragment(0, 8) == L".impera:") {
			try { HandleControlEvent(window, resource.Fragment(8, -1).ToUInt32(), ControlEvent::AcceleratorCommand, 0); } catch (...) {}
		} else MoveToPage(resource);
	}
	virtual void CaretPositionChanged(Controls::RichEdit * sender) override {}
	virtual Graphics::IBitmap * GetImageByName(const string & resource, Controls::RichEdit * sender) override { return 0; }
};

void CreateBrowser(const Engine::ImmutableString & path)
{
	Array<string> module_search_paths(0x10);
	SafePointer<XV::ManualVolume> manual;
	string default_language;
	auto root = IO::Path::GetDirectory(IO::GetExecutablePath());
	SafePointer<Storage::Registry> xv_conf = XX::LoadConfiguration(root + L"/xv.ini");
	try {
		auto core = xv_conf->GetValueString(L"XE");
		if (core) XX::IncludeComponent(module_search_paths, root + L"/" + core);
	} catch (...) {}
	try {
		auto store = xv_conf->GetValueString(L"Entheca");
		if (store.Length()) XX::IncludeStoreIntegration(module_search_paths, root + L"/" + store);
	} catch (...) {}
	for (auto & msp : module_search_paths) try {
		SafePointer< Array<string> > files = IO::Search::GetFiles(msp + L"/*." + string(XI::FileExtensionManual));
		for (auto & f : *files) try {
			SafePointer<Streaming::Stream> stream = new Streaming::FileStream(msp + "/" + f, Streaming::AccessRead, Streaming::OpenExisting);
			SafePointer<XV::ManualVolume> volume = new XV::ManualVolume(stream);
			if (manual) manual->Unify(volume); else manual = volume;
		} catch (...) {}
	} catch (...) {}
	default_language = xv_conf->GetValueString(L"LinguaDefalta");
	if (!manual) return;
	new BrowserCallback(manual, default_language, path);
}
void CreateBrowser(const Engine::ImmutableString & path, Engine::XV::ManualVolume * volume)
{
	if (!volume) return;
	string default_language;
	auto root = IO::Path::GetDirectory(IO::GetExecutablePath());
	SafePointer<Storage::Registry> xv_conf = XX::LoadConfiguration(root + L"/xv.ini");
	default_language = xv_conf->GetValueString(L"LinguaDefalta");
	new BrowserCallback(volume, default_language, path);
}
void CreateBrowser(const Engine::ImmutableString & file, const Engine::ImmutableString & text)
{
	SafePointer<XV::ManualVolume> volume = new XV::ManualVolume;
	auto page = volume->AddPage(file, XV::ManualPageClass::Sample);
	auto sect = page->AddSection(XV::ManualSectionClass::Summary);
	DynamicString file_escaped;
	for (int i = 0; i < file.Length(); i++) {
		if (file[i] == L'\\') file_escaped << L"\\\\";
		else if (file[i] == L'{') file_escaped << L"\\{";
		else if (file[i] == L'}') file_escaped << L"\\}";
		else file_escaped << file[i];
	}
	page->SetTitle(file_escaped);
	sect->SetContents(L"", L"\\c{" + text + L"}");
	CreateBrowser(file, volume);
}