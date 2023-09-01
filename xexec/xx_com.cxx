#include "xx_com.h"

namespace Engine
{
	namespace XX
	{
		ENGINE_REFLECTED_CLASS(Plugin, Engine::Reflection::Reflected)
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Name);
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Target);
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, File);
		ENGINE_END_REFLECTED_CLASS
		ENGINE_REFLECTED_CLASS(InstalledProduct, Engine::Reflection::Reflected)
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Identifier);
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Version);
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Platform);
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, PackageType);
			ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, Path);
			ENGINE_DEFINE_REFLECTED_ARRAY(STRING, CreatedLinks);
			ENGINE_DEFINE_REFLECTED_ARRAY(STRING, CreatedRegistryKeys);
			ENGINE_DEFINE_REFLECTED_ARRAY(STRING, CreatedRegistryValues);
			ENGINE_DEFINE_REFLECTED_ARRAY(STRING, CreatedPathEntries);
			ENGINE_DEFINE_REFLECTED_ARRAY(STRING, PermanentFiles);
			ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(Plugin, Plugins);
		ENGINE_END_REFLECTED_CLASS
		ENGINE_REFLECTED_CLASS(InstallationDatabase, Engine::Reflection::Reflected)
			ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(InstalledProduct, Products);
		ENGINE_END_REFLECTED_CLASS

		Storage::Registry * LoadConfiguration(const string & from_file)
		{
			SafePointer<Streaming::Stream> conf = new Streaming::FileStream(from_file, Streaming::AccessRead, Streaming::OpenExisting);
			try {
				SafePointer<Storage::Registry> result = Storage::LoadRegistry(conf);
				if (!result) throw InvalidFormatException();
				result->Retain();
				return result;
			} catch (...) {}
			conf->Seek(0, Streaming::Begin);
			SafePointer<Storage::Registry> result = Storage::CompileTextRegistry(conf);
			if (!result) throw InvalidFormatException();
			result->Retain();
			return result;
		}

		void IncludeComponent(XE::StandardLoader & loader, const string & manifest)
		{
			auto root = IO::Path::GetDirectory(IO::ExpandPath(manifest));
			SafePointer<Storage::Registry> com = LoadConfiguration(manifest);
			auto lib_path = com->GetValueString(L"Libri");
			auto dll_path = com->GetValueString(L"Dynamici");
			if (lib_path.Length()) {
				if (!loader.AddModuleSearchPath(IO::ExpandPath(root + L"/" + lib_path))) throw Exception();
			}
			if (dll_path.Length()) {
				if (!loader.AddDynamicLibrarySearchPath(IO::ExpandPath(root + L"/" + dll_path))) throw Exception();
			}
		}
		void IncludeComponent(Array<string> & module_paths, const string & manifest)
		{
			auto root = IO::Path::GetDirectory(IO::ExpandPath(manifest));
			SafePointer<Storage::Registry> com = LoadConfiguration(manifest);
			auto lib_path = com->GetValueString(L"Libri");
			if (lib_path.Length()) module_paths.Append(IO::ExpandPath(root + L"/" + lib_path));
		}
		void IncludeStoreIntegration(XE::StandardLoader & loader, const string & intfile)
		{
			auto root = IO::Path::GetDirectory(IO::ExpandPath(intfile));
			SafePointer<Storage::Registry> com = LoadConfiguration(intfile);
			auto db_path = com->GetValueString(L"DatabasePath");
			SafePointer<Streaming::Stream> db_file = new Streaming::FileStream(db_path, Streaming::AccessRead, Streaming::OpenExisting);
			InstallationDatabase db;
			Reflection::RestoreFromBinaryObject(db, db_file);
			for (auto & p : db.Products) for (auto & p : p.Plugins) if (p.Target == L"XE") {
				try { IncludeComponent(loader, p.File); } catch (...) {}
			}
		}
		void IncludeStoreIntegration(Array<string> & module_paths, const string & intfile)
		{
			auto root = IO::Path::GetDirectory(IO::ExpandPath(intfile));
			SafePointer<Storage::Registry> com = LoadConfiguration(intfile);
			auto db_path = com->GetValueString(L"DatabasePath");
			SafePointer<Streaming::Stream> db_file = new Streaming::FileStream(db_path, Streaming::AccessRead, Streaming::OpenExisting);
			InstallationDatabase db;
			Reflection::RestoreFromBinaryObject(db, db_file);
			for (auto & p : db.Products) for (auto & p : p.Plugins) if (p.Target == L"XE") {
				try { IncludeComponent(module_paths, p.File); } catch (...) {}
			}
		}
	}
}