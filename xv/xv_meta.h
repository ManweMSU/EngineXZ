#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XV
	{
		namespace Meta
		{
			constexpr const widechar * Stamp = L"XV";
			constexpr uint VersionMajor = 1;
			constexpr uint VersionMinor = 0;
			constexpr uint Subversion = 0;
			constexpr uint BuildNumber = 1;
		}
		namespace Lexic
		{
			constexpr const widechar * LiteralYes	= L"sic";
			constexpr const widechar * LiteralNo	= L"non";

			constexpr const widechar * KeywordNamespace	= L"spatium";
			constexpr const widechar * KeywordClass		= L"genus";
			constexpr const widechar * KeywordInterface	= L"protocollum";
			constexpr const widechar * KeywordAlias		= L"nomen_alternum";
			constexpr const widechar * KeywordFunction	= L"functio";
			constexpr const widechar * KeywordOperator	= L"operator";
			constexpr const widechar * KeywordEntry		= L"introitus";
			constexpr const widechar * KeywordThrows	= L"iacit";
			constexpr const widechar * KeywordThrow		= L"iace";
			constexpr const widechar * KeywordSizeOf	= L"magnitudo";
			constexpr const widechar * KeywordSizeOfMX	= L"magnitudo_maxima";
			constexpr const widechar * KeywordModule	= L"modulus";
			constexpr const widechar * KeywordArray		= L"ordo";
			constexpr const widechar * KeywordConst		= L"constatus";
			constexpr const widechar * KeywordUse		= L"ute";
			constexpr const widechar * KeywordVariable	= L"var";
			constexpr const widechar * KeywordImport	= L"importa";
			constexpr const widechar * KeywordResource	= L"auxilium";
			constexpr const widechar * KeywordTry		= L"proba";
			constexpr const widechar * KeywordCatch		= L"cape";
			constexpr const widechar * KeywordClassFunc	= L"classis";
			constexpr const widechar * KeywordCtor		= L"structor";
			constexpr const widechar * KeywordDtor		= L"destructor";
			constexpr const widechar * KeywordConvertor	= L"convertor";
			constexpr const widechar * KeywordVirtual	= L"virtualis";
			constexpr const widechar * KeywordPure		= L"pura";
			constexpr const widechar * KeywordContinue	= L"dura";
			constexpr const widechar * KeywordReturn	= L"responde";
			constexpr const widechar * KeywordIf		= L"si";
			constexpr const widechar * KeywordElse		= L"alioqui";
			constexpr const widechar * KeywordWhile		= L"dum";
			constexpr const widechar * KeywordDo		= L"fac";
			constexpr const widechar * KeywordFor		= L"per";
			constexpr const widechar * KeywordBreak		= L"exi";
			constexpr const widechar * KeywordNull		= L"nullus";
			constexpr const widechar * KeywordOverride	= L"redefini";
			constexpr const widechar * KeywordInherit	= L"hereditat";
			constexpr const widechar * KeywordInit		= L"funda";
			constexpr const widechar * KeywordStructure	= L"structura";
			constexpr const widechar * KeywordPrototype	= L"praeforma";
			constexpr const widechar * KeywordNew		= L"crea";
			constexpr const widechar * KeywordDelete	= L"perde";
			constexpr const widechar * KeywordConstruct	= L"initia";
			constexpr const widechar * KeywordDestruct	= L"fini";
			constexpr const widechar * KeywordEnum		= L"enumeratio";
			constexpr const widechar * KeywordAs		= L"acsi";
			constexpr const widechar * KeywordTrap		= L"__decipula__";
			constexpr const widechar * KeywordBLT		= L"__ttl__";

			constexpr const widechar * AttributeSystem	= L"[systema]";
			constexpr const widechar * AttributeExtens	= L"[extensio]";
			constexpr const widechar * AttributeSemant	= L"[significatio]";
			constexpr const widechar * AttributeSize	= L"[magnitudo]";
			constexpr const widechar * AttributeCore	= L"[innatum]";
			constexpr const widechar * AttributeInit	= L"[initium]";
			constexpr const widechar * AttributeNoTC	= L"[thiscall_nullum]";
			constexpr const widechar * AttributeFinal	= L"[finis]";
			constexpr const widechar * AttributeAsm		= L"[xa]";
			constexpr const widechar * AttributeInline	= L"[inline]";
			constexpr const widechar * AttributeImport	= L"[importa]";
			constexpr const widechar * AttributeImpLib	= L"[importa_de]";
			constexpr const widechar * AttributeOffset	= L"[positus]";
			constexpr const widechar * AttributeUnalign	= L"[non_polire]";
			constexpr const widechar * AttributeRPC		= L"[ifr]";
			constexpr const widechar * AttributeConsole	= L"scripta";
			constexpr const widechar * AttributeGUI		= L"graphica";
			constexpr const widechar * AttributeNoUI	= L"nulla";
			constexpr const widechar * AttributeLibrary	= L"libera";
			constexpr const widechar * AttributeSUnk	= L"ignotus";
			constexpr const widechar * AttributeSNone	= L"nihil";
			constexpr const widechar * AttributeSUInt	= L"integer";
			constexpr const widechar * AttributeSSInt	= L"integer_signus";
			constexpr const widechar * AttributeSFloat	= L"fractus";
			constexpr const widechar * AttributeSObject	= L"classis";
			constexpr const widechar * AttributeSThis	= L"ego";
			constexpr const widechar * AttributeSRTTI	= L"genus";
			constexpr const widechar * AttributeSError	= L"error";
			constexpr const widechar * AttributeCnvrtr	= L"[convertor]";
			constexpr const widechar * AttributeCExt	= L"extendens";
			constexpr const widechar * AttributeCNar	= L"angustans";
			constexpr const widechar * AttributeCInv	= L"conformis";
			constexpr const widechar * AttributeCExp	= L"pretiosus";

			constexpr const widechar * ConstructorZero	= L"vacuus";
			constexpr const widechar * ConstructorMove	= L"motus";
			constexpr const widechar * IdentifierThis	= L"ego";
			constexpr const widechar * IdentifierSet	= L"loca";
			constexpr const widechar * IdentifierGet	= L"adipisce";
			constexpr const widechar * IdentifierValue	= L"valor";
			constexpr const widechar * IdentifierTVPP	= L"valor_publicus_";
			constexpr const widechar * IdentifierDefs	= L"xvdefinitiones";

			constexpr const widechar * ResourceData	= L"data";
			constexpr const widechar * ResourceIcon	= L"icon";
			constexpr const widechar * ResourceMeta	= L"attributum";
			constexpr const widechar * ResourceLang	= L"lingua";
		}
	}
}