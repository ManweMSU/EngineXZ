#include "xe_loader.h"

#include "xe_stdext.h"
#include "xe_logger.h"
#include "../ximg/xi_module.h"

#ifdef ENGINE_X64
	#ifdef ENGINE_ARM
		#define XE_CURRENT_MACHINE L"arm64"
	#else
		#define XE_CURRENT_MACHINE L"x64"
	#endif
#else
	#ifdef ENGINE_ARM
		#define XE_CURRENT_MACHINE L"arm"
	#else
		#define XE_CURRENT_MACHINE L"x86"
	#endif
#endif

namespace Engine
{
	namespace XE
	{
		class EStandardLoader : public StandardLoader
		{
			Array<string> _xi_paths, _dl_paths;
			ObjectArray<IAPIExtension> _apis;
			ObjectArray<IModuleExtension> _mdl;
			ObjectArray<ISecurityExtension> _sec;
			ModuleLoadError _error;
			string _module, _subject;
		private:
			bool _open_module_stream(const string & module_path, Streaming::Stream ** stream, uintptr & module_stream_context) noexcept
			{
				try {
					SafePointer<Streaming::Stream> result = new Streaming::FileStream(module_path, Streaming::AccessRead, Streaming::OpenExisting);
					uintptr context = 0;
					if (_sec.Length()) context = _sec[0].EvaluateTrustContext(module_path);
					*stream = result.Inner();
					module_stream_context = context;
					result->Retain();
					return true;
				} catch (...) { return false; }
			}
		public:
			EStandardLoader(void) : _xi_paths(0x10), _dl_paths(0x10), _apis(0x10), _mdl(0x10), _sec(0x10), _error(ModuleLoadError::Success) {}
			virtual ~EStandardLoader(void) override {}
			virtual Streaming::Stream * OpenModule(const string & module_name, uintptr & module_stream_context) noexcept override
			{
				for (auto & e : _mdl) {
					auto stream = e.OpenModule(module_name, module_stream_context);
					if (stream) return stream;
				}
				for (auto & p : _xi_paths) {
					SafePointer<Streaming::Stream> stream;
					if (_open_module_stream(IO::ExpandPath(p + L"/" + module_name + L"." + XI::FileExtensionLibrary), stream.InnerRef(), module_stream_context)) { stream->Retain(); return stream; }
					if (_open_module_stream(IO::ExpandPath(p + L"/" + module_name + L"." + XI::FileExtensionExecutable), stream.InnerRef(), module_stream_context)) { stream->Retain(); return stream; }
					if (_open_module_stream(IO::ExpandPath(p + L"/" + module_name + L"." + XI::FileExtensionExecutable2), stream.InnerRef(), module_stream_context)) { stream->Retain(); return stream; }
				}
				return 0;
			}
			virtual void * GetRoutineAddress(const string & routine_name) noexcept override
			{
				for (auto & e : _apis) {
					auto addr = e.ExposeRoutine(routine_name);
					if (addr) return const_cast<void *>(addr);
				}
				return 0;
			}
			virtual handle LoadDynamicLibrary(const string & library_name) noexcept override
			{
				for (auto & p : _dl_paths) {
					auto dl = LoadLibraryXE(IO::ExpandPath(p + L"/" + library_name + L"." + string(ENGINE_LIBRARY_EXTENSION)));
					if (dl) return dl;
					dl = LoadLibraryXE(IO::ExpandPath(p + L"/" + library_name + L"_" + string(XE_CURRENT_MACHINE) + L"." + string(ENGINE_LIBRARY_EXTENSION)));
					if (dl) return dl;
					dl = LoadLibraryXE(IO::ExpandPath(p + L"/" + library_name));
					if (dl) return dl;
				}
				return 0;
			}
			virtual void HandleModuleLoadError(const string & module_name, const string & subject, ModuleLoadError error) noexcept override
			{
				_error = error;
				_module = module_name;
				_subject = subject;
			}
			virtual Object * ExposeObject(void) noexcept override { return this; }
			virtual void * ExposeInterface(const string & interface) noexcept override
			{
				for (auto & e : _apis) {
					auto addr = e.ExposeInterface(interface);
					if (addr) return const_cast<void *>(addr);
				}
				return 0;
			}
			virtual ModuleLoadError EvaluateTrust(Streaming::Stream * module_stream, uintptr module_stream_context) noexcept override
			{
				if (!_sec.Length()) return ModuleLoadError::Success;
				try {
					SafePointer<DataBlock> mdata = XI::ReadConsistencyData(module_stream);
					return _sec[0].EvaluateTrust(mdata->GetBuffer(), mdata->Length(), module_stream_context, module_stream);
				} catch (...) { return ModuleLoadError::InvalidImageFormat; }
			}
			virtual bool AddModuleSearchPath(const string & path) noexcept override
			{
				for (auto & p : _xi_paths) if (p == path) return true;
				try { _xi_paths.Append(path); } catch (...) { return false; }
				return true;
			}
			virtual Array<string> & GetModuleSearchPaths(void) noexcept override { return _xi_paths; }
			virtual bool AddDynamicLibrarySearchPath(const string & path) noexcept override
			{
				for (auto & p : _dl_paths) if (p == path) return true;
				try { _dl_paths.Append(path); } catch (...) { return false; }
				return true;
			}
			virtual Array<string> & GetDynamicLibrarySearchPaths(void) noexcept override { return _dl_paths; }
			virtual bool RegisterModuleExtension(IModuleExtension * ext) noexcept override
			{
				for (auto & e : _mdl) if (&e == ext) return true;
				try { _mdl.Append(ext); } catch (...) { return false; }
				return true;
			}
			virtual bool UnregisterModuleExtension(IModuleExtension * ext) noexcept override
			{
				for (int i = 0; i < _mdl.Length(); i++) if (_mdl.ElementAt(i) == ext) { _mdl.Remove(i); return true; }
				return false;
			}
			virtual ObjectArray<IModuleExtension> & GetModuleExtensions(void) noexcept override { return _mdl; }
			virtual bool RegisterAPIExtension(IAPIExtension * ext) noexcept override
			{
				for (auto & e : _apis) if (&e == ext) return true;
				try { _apis.Append(ext); } catch (...) { return false; }
				return true;
			}
			virtual bool UnregisterAPIExtension(IAPIExtension * ext) noexcept override
			{
				for (int i = 0; i < _apis.Length(); i++) if (_apis.ElementAt(i) == ext) { _apis.Remove(i); return true; }
				return false;
			}
			virtual ObjectArray<IAPIExtension> & GetAPIExtensions(void) noexcept override { return _apis; }
			virtual bool RegisterSecurityExtension(ISecurityExtension * ext) noexcept override
			{
				for (auto & e : _sec) if (&e == ext) return true;
				try { _sec.Append(ext); } catch (...) { return false; }
				return true;
			}
			virtual bool UnregisterSecurityExtension(ISecurityExtension * ext) noexcept override
			{
				for (int i = 0; i < _sec.Length(); i++) if (_sec.ElementAt(i) == ext) { _sec.Remove(i); return true; }
				return false;
			}
			virtual ObjectArray<ISecurityExtension> & GetSecurityExtensions(void) noexcept override { return _sec; }
			virtual bool IsAlive(void) noexcept override { return _error == ModuleLoadError::Success; }
			virtual ModuleLoadError GetLastError(void) noexcept override { return _error; }
			virtual const string & GetLastErrorModule(void) noexcept override { return _module; }
			virtual const string & GetLastErrorSubject(void) noexcept override { return _subject; }
		};

		StandardLoader * CreateStandardLoader(uint flags)
		{
			SafePointer<StandardLoader> result = new EStandardLoader;
			if (flags & UseStandardFPU) {
				SafePointer<IAPIExtension> ext = CreateFPU();
				result->RegisterAPIExtension(ext);
			}
			if (flags & UseStandardMMU) {
				SafePointer<IAPIExtension> ext = CreateMMU();
				result->RegisterAPIExtension(ext);
			}
			if (flags & UseStandardSPU) {
				SafePointer<IAPIExtension> ext = CreateSPU();
				result->RegisterAPIExtension(ext);
			}
			if (flags & UseStandardLD) {
				SafePointer<IAPIExtension> ext = CreateLogger();
				result->RegisterAPIExtension(ext);
			}
			if (flags & UseStandardMisc) {
				SafePointer<IAPIExtension> ext = CreateMiscUnit();
				result->RegisterAPIExtension(ext);
			}
			result->Retain();
			return result;
		}
		handle LoadLibraryXE(const string & path)
		{
			auto library = LoadLibrary(path);
			if (!library) return 0;
			auto init_routine = reinterpret_cast<InitializerRoutine>(GetLibraryRoutine(library, "InitiaXE"));
			if (init_routine) {
				try {
					Array<char> utf8(1);
					utf8.SetLength(path.GetEncodedLength(Encoding::UTF8) + 1);
					path.Encode(utf8, Encoding::UTF8, true);
					if (!init_routine(utf8)) throw Exception();
				} catch (...) { ReleaseLibrary(library); return 0; }
			}
			return library;
		}
	}
}