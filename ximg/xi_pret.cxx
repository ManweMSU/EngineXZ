﻿#include "xi_pret.h"

#include "xi_function.h"

namespace Engine
{
	namespace XI
	{
		class PretranslationLoader : public IFunctionLoader
		{
		public:
			bool empty;
			XA::Function function;
			PretranslationLoader(void) : empty(true) {}
			virtual Platform GetArchitecture(void) noexcept override { return Platform::Unknown; }
			virtual XA::CallingConvention GetCallingConvention(void) noexcept override { return XA::CallingConvention::Unknown; }
			virtual void HandleAbstractFunction(const string & symbol, const Module::Function & fin, Streaming::Stream * fout) noexcept override { function.Load(fout); empty = false; }
			virtual void HandlePlatformFunction(const string & symbol, const Module::Function & fin, Streaming::Stream * fout) noexcept override {}
			virtual void HandleNearImport(const string & symbol, const Module::Function & fin, const string & func_name) noexcept override {}
			virtual void HandleFarImport(const string & symbol, const Module::Function & fin, const string & func_name, const string & lib_name) noexcept override {}
			virtual void HandleLoadError(const string & symbol, const Module::Function & fin, LoadFunctionError error) noexcept override {}
		};
		void PretranslateFunction(Module::Function & subject, const PretranslateDesc * sys_list, int sys_list_length)
		{
			PretranslationLoader loader;
			LoadFunction(L"", subject, &loader);
			if (!loader.empty) {
				MakeFunction(subject);
				for (int i = 0; i < sys_list_length; i++) {
					auto & desc = sys_list[i];
					SafePointer<XA::IAssemblyTranslator> trs = XA::CreatePlatformTranslator(desc.arch, desc.conv);
					XA::TranslatedFunction trs_func;
					if (!trs) throw InvalidArgumentException();
					if (!trs->Translate(trs_func, loader.function)) throw InvalidArgumentException();
					MakeFunction(subject, desc.arch, desc.conv, trs_func);
				}
			}
		}
		void PretranslateModule(Module & subject, const PretranslateDesc * sys_list, int sys_list_length)
		{
			for (auto & f : subject.functions) PretranslateFunction(f.value, sys_list, sys_list_length);
			for (auto & c : subject.classes) for (auto & f : c.value.methods) PretranslateFunction(f.value, sys_list, sys_list_length);
		}
	}
}