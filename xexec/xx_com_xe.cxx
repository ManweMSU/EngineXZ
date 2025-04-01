#include "xx_com_xe.h"
#include "xx_com_store.h"

namespace Engine
{
	namespace XX
	{
		void IncludeComponent(XE::StandardLoader & loader, const string & manifest, SecuritySettings * ss)
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
			if (ss) LoadSecuritySettings(*ss, com);
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
	}
}