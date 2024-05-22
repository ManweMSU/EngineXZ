#pragma once

#include "xi_module.h"
#include "../xasm/xa_trans.h"

namespace Engine
{
	namespace XI
	{
		enum class LoadFunctionError { InvalidFunctionFormat, UnknownImageFlags, NoTargetPlatform };
		class IFunctionLoader
		{
		public:
			virtual Platform GetArchitecture(void) noexcept = 0;
			virtual XA::Environment GetEnvironment(void) noexcept = 0;
			virtual void HandleAbstractFunction(const string & symbol, const Module::Function & fin, Streaming::Stream * fout) noexcept = 0;
			virtual void HandlePlatformFunction(const string & symbol, const Module::Function & fin, Streaming::Stream * fout) noexcept = 0;
			virtual void HandleNearImport(const string & symbol, const Module::Function & fin, const string & func_name) noexcept = 0;
			virtual void HandleFarImport(const string & symbol, const Module::Function & fin, const string & func_name, const string & lib_name) noexcept = 0;
			virtual void HandleLoadError(const string & symbol, const Module::Function & fin, LoadFunctionError error) noexcept = 0;
		};

		void MakeFunction(Module::Function & dest);
		void MakeFunction(Module::Function & dest, XA::Function & src);
		void MakeFunction(Module::Function & dest, Platform arch, XA::Environment osenv, XA::TranslatedFunction & src);
		void MakeFunction(Module::Function & dest, const string & import_name);
		void MakeFunction(Module::Function & dest, const string & import_name, const string & dl_name);

		void LoadFunction(const string & symbol, const Module::Function & func, IFunctionLoader * loader);

		Array<uint32> * LoadFunctionABI(const Module::Function & func);
		void ReadFunctionABI(uint32 word, Platform & arch, XA::Environment & env);
	}
}