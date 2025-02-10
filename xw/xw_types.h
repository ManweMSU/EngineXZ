#pragma once

#include "../ximg/xi_function.h"
#include "../xasm/xa_trans.h"

namespace Engine
{
	namespace XW
	{
		enum StatementHints : uint32
		{
			HintScope		= 0x1000,
			HintEndScope	= 0x2000,
			HintIf			= 0x3000,
			HintElse		= 0x4000,
			HintEndIf		= 0x5000,
			HintWhile		= 0x6000,
			HintDo			= 0x7000,
			HintFor			= 0x8000,
			HintEndLoop		= 0x9000,
			HintBreak		= 0xA000,
			HintContinue	= 0xB000,
			HintReturn		= 0xC000,
		};
		enum class ShaderLanguage { Unknown = 0, HLSL = 1, MSL = 2, GLSL = 3 };
		enum class Rule : uint
		{
			InsertString	= 0x00,
			InsertArgument	= 0x01,
			InsertReference	= 0x02,
		};

		struct TranslationBlock
		{
			Rule rule;
			uint index;
			string text;
		};
		struct TranslationRules
		{
			Array<string> extrefs;
			Array<TranslationBlock> blocks;

			TranslationRules(void);
			void Clear(void);
			void Save(Streaming::Stream * dest) const;
			void Load(Streaming::Stream * src);
		};

		class IFunctionLoader
		{
		public:
			virtual ShaderLanguage GetLanguage(void) noexcept = 0;
			virtual void HandleFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept = 0;
			virtual void HandleRule(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept = 0;
			virtual void HandleLoadError(const string & symbol, const XI::Module::Function & fin, XI::LoadFunctionError error) noexcept = 0;
		};

		void MakeFunction(XI::Module::Function & dest);
		void MakeFunction(XI::Module::Function & dest, XA::Function & src);
		void MakeFunction(XI::Module::Function & dest, ShaderLanguage lang, TranslationRules & src);

		void LoadFunction(const string & symbol, const XI::Module::Function & func, IFunctionLoader * loader);
		Array<ShaderLanguage> * LoadRuleVariants(const XI::Module::Function & func);
	}
}