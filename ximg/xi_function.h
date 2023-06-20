#pragma once

#include "xi_module.h"
#include "../xasm/xa_trans.h"

namespace Engine
{
	namespace XI
	{
		class IFunctionLoader
		{
		public:
			// virtual Platform GetArchitecture(void) noexcept = 0;
			// virtual XA::CallingConvention GetCallingConvention(void) noexcept = 0;
			// virtual XA::IAssemblyTranslator * GetTranslator(void) noexcept = 0;
			// virtual handle LoadDynamicLibrary(const string & name) noexcept = 0;
			// virtual void * ImportFunction(const string & name) noexcept = 0;

			// virtual void HandleFunction(const string & symbol, const Module::Function & fin, const XA::TranslatedFunction & fout) noexcept = 0;
			// virtual void HandleFunction(const string & symbol, const Module::Function & fin, void * fout) noexcept = 0;
		};

		void MakeFunction(Module::Function & dest);
		void MakeFunction(Module::Function & dest, XA::Function & src);
		void MakeFunction(Module::Function & dest, Platform arch, XA::CallingConvention cc, XA::TranslatedFunction & src);
		void MakeFunction(Module::Function & dest, const string & import_name);
		void MakeFunction(Module::Function & dest, const string & import_name, const string & dl_name);

		void LoadFunction(const string & symbol, const Module::Function & func, IFunctionLoader * loader);
	}
}