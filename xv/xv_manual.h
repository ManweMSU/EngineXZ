#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XV
	{
		enum class ManualPageClass {
			Unknown = 0,
			Constant = 1, Variable = 2, Class = 3, Function = 4, Namespace = 5, Prototype = 6, Property = 7, Field = 8, Alias = 9,
			Technique = 9, Syntax = 10, Sample = 11
		};
		enum class ManualSectionClass {
			Unknown = 0, ObjectType = 1, Summary = 2, Details = 30,
			ArgumentSection = 10, ResultSection = 11, ThrowRules = 31, ContextRules = 32,
			Inheritance = 20, CastSection = 21, DynamicCastSection = 22
		};
		enum ManualPageTraits {
			ManualPageThrows		= 0x001,
			ManualPageInstance		= 0x002,
			ManualPagePropRead		= 0x004,
			ManualPagePropWrite		= 0x008,
			ManualPageVirtual		= 0x010,
			ManualPageInterface		= 0x020,
			ManualPageConstructor	= 0x040,
			ManualPageOperator		= 0x080,
			ManualPageConvertor		= 0x100,
		};
		class ManualSection : public Object
		{
			int _index;
			string _subject;
			ManualSectionClass _class;
			Volumes::Dictionary<string, string> _contents;
		public:
			ManualSection(void);
			virtual ~ManualSection(void) override;
			void SetSubject(const string & subj, int index);
			void SetContents(const string & locale, const string & text);
			void SetClass(ManualSectionClass cls);
			void RemoveLocale(const string & locale);
			const string & GetSubjectName(void) const;
			int GetSubjectIndex(void) const;
			const Volumes::Dictionary<string, string> & GetContents(void) const;
			string GetContents(const string & locale) const;
			ManualSectionClass GetClass(void) const;
		};
		class ManualPage : public Object
		{
			string _module, _path, _title;
			ManualPageClass _class;
			uint _traits;
			ObjectArray<ManualSection> _sections;
		public:
			ManualPage(void);
			virtual ~ManualPage(void) override;
			void SetModule(const string & str);
			void SetPath(const string & str);
			void SetTitle(const string & str);
			void SetTraits(uint trt);
			void SetClass(ManualPageClass cls);
			const string & GetModule(void) const;
			const string & GetPath(void) const;
			const string & GetTitle(void) const;
			uint GetTraits(void) const;
			ManualPageClass GetClass(void) const;
			ManualSection * AddSection(ManualSectionClass cls, int index = 0, const string & subject = L"");
			ManualSection * FindSection(ManualSectionClass cls, int index = 0);
			const ManualSection * FindSection(ManualSectionClass cls, int index = 0) const;
			void RemoveSection(ManualSectionClass cls, int index = 0);
			const ObjectArray<ManualSection> & GetSections(void) const;
		};
		class ManualVolume : public Object
		{
			string _module;
			Volumes::ObjectDictionary<string, ManualPage> _pages;
		public:
			ManualVolume(void);
			ManualVolume(Streaming::Stream * stream);
			virtual ~ManualVolume(void) override;

			void Save(Streaming::Stream * into);
			void Update(ManualVolume * newer, ManualVolume ** deletes);
			void Unify(ManualVolume * volume);

			void SetModule(const string & str);
			const string & GetModule(void) const;
			ManualPage * AddPage(const string & path, ManualPageClass cls);
			ManualPage * FindPage(const string & path);
			const ManualPage * FindPage(const string & path) const;
			void RemovePage(const string & path);
			const Volumes::ObjectDictionary<string, ManualPage> & GetPages(void) const;
		};

		class IManualTextFormatter : public Object
		{
		public:
			virtual string PlainText(const string & text) = 0;
			virtual string BoldText(const string & inner) = 0;
			virtual string ItalicText(const string & inner) = 0;
			virtual string UnderlinedText(const string & inner) = 0;
			virtual string LinkedText(const string & inner, const string & to) = 0;
			virtual string Paragraph(const string & inner) = 0;
			virtual string ListItem(int index, const string & inner) = 0;
		};
		string FormatRichText(const string & text, IManualTextFormatter * formatter);
	}
}