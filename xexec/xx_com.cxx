#include "xx_com.h"
#include "xx_com_store.h"

namespace Engine
{
	namespace XX
	{
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
		void IncludeComponent(Array<string> & module_paths, const string & manifest)
		{
			auto root = IO::Path::GetDirectory(IO::ExpandPath(manifest));
			SafePointer<Storage::Registry> com = LoadConfiguration(manifest);
			auto lib_path = com->GetValueString(L"Libri");
			if (lib_path.Length()) module_paths.Append(IO::ExpandPath(root + L"/" + lib_path));
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