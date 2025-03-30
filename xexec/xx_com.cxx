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
		void IncludeComponent(Array<string> * module_paths_v, Array<string> * module_paths_w, const string & manifest)
		{
			auto root = IO::Path::GetDirectory(IO::ExpandPath(manifest));
			SafePointer<Storage::Registry> com = LoadConfiguration(manifest);
			auto lib_path_v = com->GetValueString(L"Libri");
			auto lib_path_w = com->GetValueString(L"LibriXW");
			if (lib_path_v.Length() && module_paths_v) module_paths_v->Append(IO::ExpandPath(root + L"/" + lib_path_v));
			if (lib_path_w.Length() && module_paths_w) module_paths_w->Append(IO::ExpandPath(root + L"/" + lib_path_w));
		}
		void IncludeStoreIntegration(Array<string> * module_paths_v, Array<string> * module_paths_w, const string & intfile)
		{
			SafePointer<Storage::Registry> com = LoadConfiguration(intfile);
			auto db_path = com->GetValueString(L"DatabasePath");
			SafePointer<Streaming::Stream> db_file = new Streaming::FileStream(db_path, Streaming::AccessRead, Streaming::OpenExisting);
			InstallationDatabase db;
			Reflection::RestoreFromBinaryObject(db, db_file);
			for (auto & p : db.Products) for (auto & p : p.Plugins) if (p.Target == L"XE") {
				try { IncludeComponent(module_paths_v, module_paths_w, p.File); } catch (...) {}
			}
		}
		string LocateEnvironmentConfiguration(const string & intfile)
		{
			SafePointer<Storage::Registry> com = LoadConfiguration(intfile);
			auto db_path = com->GetValueString(L"DatabasePath");
			SafePointer<Streaming::Stream> db_file = new Streaming::FileStream(db_path, Streaming::AccessRead, Streaming::OpenExisting);
			InstallationDatabase db;
			Reflection::RestoreFromBinaryObject(db, db_file);
			for (auto & p : db.Products) if (p.Identifier == L"xx") return IO::ExpandPath(p.Path + L"/xe.ini");
			throw InvalidStateException();
		}
		void LoadSecuritySettings(SecuritySettings & settings, const Storage::RegistryNode * xe)
		{
			settings.TrustedCertificates = xe->GetValueString(L"CertificatiFisi");
			settings.UntrustedCertificates = xe->GetValueString(L"CertificatiInfisi");
			settings.ValidateTrust = xe->GetValueBoolean(L"ConvalidaConfisionem");
		}
		void LoadSecuritySettings(SecuritySettings & settings, const string & xe_conf)
		{
			SafePointer<Storage::Registry> xe = LoadConfiguration(xe_conf);
			LoadSecuritySettings(settings, xe);
		}
		void UpdateSecuritySettings(const SecuritySettings & settings, const string & xe_conf)
		{
			SafePointer<Storage::Registry> xe = LoadConfiguration(xe_conf);
			try {
				xe->RemoveValue(L"CertificatiFisi");
				xe->RemoveValue(L"CertificatiInfisi");
				xe->RemoveValue(L"ConvalidaConfisionem");
			} catch (...) {}
			xe->CreateValue(L"CertificatiFisi", Storage::RegistryValueType::String);
			xe->CreateValue(L"CertificatiInfisi", Storage::RegistryValueType::String);
			xe->CreateValue(L"ConvalidaConfisionem", Storage::RegistryValueType::Boolean);
			xe->SetValue(L"CertificatiFisi", settings.TrustedCertificates);
			xe->SetValue(L"CertificatiInfisi", settings.UntrustedCertificates);
			xe->SetValue(L"ConvalidaConfisionem", settings.ValidateTrust);
			SafePointer<Streaming::Stream> stream;
			IO::MoveFile(xe_conf, xe_conf + L".tmp");
			try {
				stream = new Streaming::FileStream(xe_conf, Streaming::AccessReadWrite, Streaming::CreateNew);
			} catch (...) {
				IO::MoveFile(xe_conf + L".tmp", xe_conf);
				throw;
			}
			try {
				Storage::RegistryToText(xe, stream, Encoding::UTF8);
			} catch (...) {
				stream.SetReference(0);
				IO::RemoveFile(xe_conf);
				IO::MoveFile(xe_conf + L".tmp", xe_conf);
				throw;
			}
			stream.SetReference(0);
			IO::RemoveFile(xe_conf + L".tmp");
		}
	}
}