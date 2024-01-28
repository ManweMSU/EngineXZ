#include "xi_pret.h"

#include "xi_function.h"

namespace Engine
{
	namespace XI
	{
		class PretranslationLoader : public IFunctionLoader
		{
		public:
			bool empty;
			bool error;
			XA::Function function;
			PretranslationLoader(void) : empty(true), error(false) {}
			virtual Platform GetArchitecture(void) noexcept override { return Platform::Unknown; }
			virtual XA::CallingConvention GetCallingConvention(void) noexcept override { return XA::CallingConvention::Unknown; }
			virtual void HandleAbstractFunction(const string & symbol, const Module::Function & fin, Streaming::Stream * fout) noexcept override { try { function.Load(fout); empty = false; } catch (...) { error = true; } }
			virtual void HandlePlatformFunction(const string & symbol, const Module::Function & fin, Streaming::Stream * fout) noexcept override {}
			virtual void HandleNearImport(const string & symbol, const Module::Function & fin, const string & func_name) noexcept override {}
			virtual void HandleFarImport(const string & symbol, const Module::Function & fin, const string & func_name, const string & lib_name) noexcept override {}
			virtual void HandleLoadError(const string & symbol, const Module::Function & fin, LoadFunctionError error) noexcept override {}
		};
		void PretranslateFunction(Module::Function & subject, const PretranslateDesc * sys_list, int sys_list_length)
		{
			PretranslationLoader loader;
			LoadFunction(L"", subject, &loader);
			if (loader.error) throw InvalidFormatException();
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
			auto resxdl = MakeResourceID(L"XDL", 0);
			for (auto & f : subject.functions) PretranslateFunction(f.value, sys_list, sys_list_length);
			for (auto & c : subject.classes) for (auto & f : c.value.methods) PretranslateFunction(f.value, sys_list, sys_list_length);
			for (auto & r : subject.resources) if (r.key & 0xFFFFFFFF == resxdl) {
				SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(r.value->GetBuffer(), r.value->Length());
				Module submodule(stream);
				PretranslateModule(submodule, sys_list, sys_list_length);
				stream->SetLength(0);
				stream->Seek(0, Streaming::Begin);
				submodule.Save(stream);
				stream->Seek(0, Streaming::Begin);
				r.value = stream->ReadAll();
			}
		}
	}
}