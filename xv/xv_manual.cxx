#include "xv_manual.h"

using namespace Engine::Storage;
using namespace Engine::Streaming;
using namespace Engine::Reflection;

namespace Engine
{
	namespace XV
	{
		namespace ManualFormat
		{
			ENGINE_REFLECTED_CLASS(Section, Reflected)
				ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, nomen);
				ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, classis);
				ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, numerus);
				ENGINE_DEFINE_REFLECTED_ARRAY(STRING, lingue);
				ENGINE_DEFINE_REFLECTED_ARRAY(STRING, scriptiones);
			ENGINE_END_REFLECTED_CLASS
			ENGINE_REFLECTED_CLASS(Page, Reflected)
				ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, nomen_paginae);
				ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, nomen_moduli);
				ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, titulus_paginae);
				ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, classis);
				ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, tractus);
				ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(Section, membra);
			ENGINE_END_REFLECTED_CLASS
			ENGINE_REFLECTED_CLASS(Document, Reflected)
				ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, nomen_moduli);
				ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(Page, paginae);
			ENGINE_END_REFLECTED_CLASS
		}

		ManualSection::ManualSection(void) : _class(ManualSectionClass::Unknown), _index(0) {}
		ManualSection::~ManualSection(void) {}
		void ManualSection::SetSubject(const string & subj, int index) { _subject = subj; _index = index; }
		void ManualSection::SetContents(const string & locale, const string & text) { _contents.Update(locale, text); }
		void ManualSection::SetClass(ManualSectionClass cls) { _class = cls; }
		void ManualSection::RemoveLocale(const string & locale) { _contents.Remove(locale); }
		const string & ManualSection::GetSubjectName(void) const { return _subject; }
		int ManualSection::GetSubjectIndex(void) const { return _index; }
		const Volumes::Dictionary<string, string> & ManualSection::GetContents(void) const { return _contents; }
		string ManualSection::GetContents(const string & locale) const { auto line = _contents[locale]; if (line) return *line; return L""; }
		ManualSectionClass ManualSection::GetClass(void) const { return _class; }

		ManualPage::ManualPage(void) : _class(ManualPageClass::Unknown), _traits(0), _sections(0x10) {}
		ManualPage::~ManualPage(void) {}
		void ManualPage::SetModule(const string & str) { _module = str; }
		void ManualPage::SetPath(const string & str) { _path = str; }
		void ManualPage::SetTitle(const string & str) { _title = str; }
		void ManualPage::SetTraits(uint trt) { _traits = trt; }
		void ManualPage::SetClass(ManualPageClass cls) { _class = cls; }
		const string & ManualPage::GetModule(void) const { return _module; }
		const string & ManualPage::GetPath(void) const { return _path; }
		const string & ManualPage::GetTitle(void) const { return _title; }
		uint ManualPage::GetTraits(void) const { return _traits; }
		ManualPageClass ManualPage::GetClass(void) const { return _class; }
		ManualSection * ManualPage::AddSection(ManualSectionClass cls, int index, const string & subject)
		{
			SafePointer<ManualSection> section = new ManualSection;
			section->SetSubject(subject, index);
			section->SetClass(cls);
			int pos = 0;
			while (pos < _sections.Length() && (_sections[pos].GetClass() < cls || (_sections[pos].GetClass() == cls && _sections[pos].GetSubjectIndex() < index))) pos++;
			_sections.Insert(section, pos);
			return section;
		}
		ManualSection * ManualPage::FindSection(ManualSectionClass cls, int index)
		{
			for (auto & s : _sections) if (s.GetClass() == cls && s.GetSubjectIndex() == index) return &s;
			return 0;
		}
		const ManualSection * ManualPage::FindSection(ManualSectionClass cls, int index) const
		{
			for (auto & s : _sections) if (s.GetClass() == cls && s.GetSubjectIndex() == index) return &s;
			return 0;
		}
		void ManualPage::RemoveSection(ManualSectionClass cls, int index)
		{
			for (int i = 0; i < _sections.Length(); i++) if (_sections[i].GetClass() == cls && _sections[i].GetSubjectIndex() == index) {
				_sections.Remove(i);
				return;
			}
		}
		ObjectArray<ManualSection> & ManualPage::GetSections(void) { return _sections; }
		const ObjectArray<ManualSection> & ManualPage::GetSections(void) const { return _sections; }

		ManualVolume::ManualVolume(void) {}
		ManualVolume::ManualVolume(Streaming::Stream * stream)
		{
			ManualFormat::Document src;
			RestoreFromBinaryObject(src, stream);
			_module = src.nomen_moduli;
			for (auto & p : src.paginae) {
				auto page = AddPage(p.nomen_paginae, static_cast<ManualPageClass>(p.classis));
				page->SetTitle(p.titulus_paginae);
				page->SetTraits(p.tractus);
				for (auto & s : p.membra) {
					auto sect = page->AddSection(static_cast<ManualSectionClass>(s.classis), s.numerus, s.nomen);
					if (s.lingue.Length() == s.scriptiones.Length()) for (int i = 0; i < s.lingue.Length(); i++) sect->SetContents(s.lingue[i], s.scriptiones[i]);
				}
			}
		}
		ManualVolume::~ManualVolume(void) {}
		void ManualVolume::Save(Streaming::Stream * into)
		{
			ManualFormat::Document dest;
			dest.nomen_moduli = _module;
			for (auto & p : _pages) {
				ManualFormat::Page page;
				page.nomen_moduli = p.value->GetModule();
				page.nomen_paginae = p.value->GetPath();
				page.titulus_paginae = p.value->GetTitle();
				page.classis = uint(p.value->GetClass());
				page.tractus = p.value->GetTraits();
				for (auto & s : p.value->GetSections()) {
					ManualFormat::Section sect;
					sect.nomen = s.GetSubjectName();
					sect.numerus = s.GetSubjectIndex();
					sect.classis = uint(s.GetClass());
					for (auto l : s.GetContents()) {
						sect.lingue << l.key;
						sect.scriptiones << l.value;
					}
					page.membra.InnerArray.Append(sect);
				}
				dest.paginae.InnerArray.Append(page);
			}
			SerializeToBinaryObject(dest, into);
		}
		void ManualVolume::Update(ManualVolume * newer, ManualVolume ** deletes)
		{
			_module = newer->_module;
			SafePointer<ManualVolume> del = new ManualVolume;
			auto current = _pages.GetFirst();
			while (current) {
				auto next = current->GetNext();
				auto update = newer->_pages.GetObjectByKey(current->GetValue().key);
				if (update) {
					current->GetValue().value->SetPath(update->GetPath());
					current->GetValue().value->SetTitle(update->GetTitle());
					current->GetValue().value->SetClass(update->GetClass());
					current->GetValue().value->SetTraits(update->GetTraits());
					for (auto & s : update->GetSections()) {
						auto sect = current->GetValue().value->FindSection(s.GetClass(), s.GetSubjectIndex());
						if (!sect) sect = current->GetValue().value->AddSection(s.GetClass(), s.GetSubjectIndex(), s.GetSubjectName());
						sect->SetSubject(s.GetSubjectName(), s.GetSubjectIndex());
						for (auto & c : s.GetContents()) if (c.value.Length()) sect->SetContents(c.key, c.value);
					}
				} else {
					bool keep = false;
					if (current->GetValue().key[0] == L'.') keep = true;
					if (!keep) for (auto & p : _pages) if (current->GetValue().key.Fragment(0, p.key.Length()) == p.key && p.value->GetClass() == ManualPageClass::Prototype) { keep = true; break; }
					if (!keep) {
						del->_pages.Append(current->GetValue().key, current->GetValue().value);
						_pages.Remove(current->GetValue().key);
					}
				}
				current = next;
			}
			for (auto & p : newer->_pages) if (!_pages.ElementExists(p.key)) _pages.Append(p.key, p.value);
			if (!del->_pages.IsEmpty()) {
				*deletes = del.Inner();
				del->Retain();
			}
		}
		void ManualVolume::Unify(ManualVolume * volume) { for (auto & p : volume->_pages) _pages.Append(p.key, p.value); _module = L""; }
		void ManualVolume::SetModule(const string & str) { _module = str; }
		const string & ManualVolume::GetModule(void) const { return _module; }
		ManualPage * ManualVolume::AddPage(const string & path, ManualPageClass cls)
		{
			SafePointer<ManualPage> page = new ManualPage;
			page->SetPath(path);
			page->SetClass(cls);
			page->SetModule(_module);
			if (!_pages.Append(path, page)) return 0;
			return page;
		}
		ManualPage * ManualVolume::FindPage(const string & path) { return _pages.GetObjectByKey(path); }
		const ManualPage * ManualVolume::FindPage(const string & path) const { return _pages.GetObjectByKey(path); }
		void ManualVolume::RemovePage(const string & path) { _pages.Remove(path); }
		const Volumes::ObjectDictionary<string, ManualPage> & ManualVolume::GetPages(void) const { return _pages; }

		string FormatCodeText(const string & text, int & pos)
		{
			DynamicString result;
			bool scope = text[pos] == L'{';
			if (scope) pos++;
			while (text[pos] && (!scope || text[pos] != L'}')) {
				if (text[pos] == L'\\') {
					pos++;
					if (text[pos] == L'{') {
						result += text[pos]; pos++;
					} else if (text[pos] == L'}') {
						result += text[pos]; pos++;
					} else if (text[pos] == L'\\') {
						result += text[pos]; pos++;
					} else return result;
				} else { result << text[pos]; pos++; }
			}
			if (text[pos]) pos++;
			return result;
		}
		string FormatRichText(const string & text, int & pos, IManualTextFormatter * formatter)
		{
			DynamicString result;
			bool scope = text[pos] == L'{';
			if (scope) pos++;
			int begin = pos;
			int index = 0;
			while (text[pos] && (!scope || text[pos] != L'}')) {
				if (text[pos] == L'\\') {
					if (begin < pos) result += formatter->PlainText(text.Fragment(begin, pos - begin));
					pos++;
					if (text[pos] == L'b') {
						pos++;
						result += formatter->BoldText(FormatRichText(text, pos, formatter));
					} else if (text[pos] == L'i') {
						pos++;
						result += formatter->ItalicText(FormatRichText(text, pos, formatter));
					} else if (text[pos] == L'u') {
						pos++;
						result += formatter->UnderlinedText(FormatRichText(text, pos, formatter));
					} else if (text[pos] == L'l') {
						pos++;
						auto cont = FormatRichText(text, pos, formatter);
						auto link = FormatRichText(text, pos, formatter);
						result += formatter->LinkedText(cont, link);
					} else if (text[pos] == L'p') {
						pos++;
						result += formatter->Paragraph(FormatRichText(text, pos, formatter));
					} else if (text[pos] == L'q') {
						pos++; index++;
						result += formatter->ListItem(index, FormatRichText(text, pos, formatter));
					} else if (text[pos] == L'c') {
						pos++;
						result += formatter->Code(FormatCodeText(text, pos));
					} else if (text[pos] == L'{') {
						result += text[pos]; pos++;
					} else if (text[pos] == L'}') {
						result += text[pos]; pos++;
					} else if (text[pos] == L'\\') {
						result += text[pos]; pos++;
					} else return result;
					begin = pos;
				} else if (text[pos] == L'{') {
					if (begin < pos) result += formatter->PlainText(text.Fragment(begin, pos - begin));
					result += FormatRichText(text, pos, formatter);
					begin = pos;
				} else pos++;
			}
			if (begin < pos) result += formatter->PlainText(text.Fragment(begin, pos - begin));
			if (text[pos]) pos++;
			return result;
		}
		string FormatRichText(const string & text, IManualTextFormatter * formatter)
		{
			int pos = 0;
			return FormatRichText(text, pos, formatter);
		}
	}
}