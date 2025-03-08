#include "xw_metal_com.h"

namespace Engine
{
	namespace XW
	{
		PrecompilationStatus MetalCompile(const string & in, const string & out, const string & log, MetalProfileDesc & profile)
		{
			try {
				SafePointer<Storage::Archive> egsu;
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::FileStream(in, Streaming::AccessRead, Streaming::OpenExisting);
					egsu = Storage::OpenArchive(stream, Storage::ArchiveMetadataUsage::IgnoreMetadata);
					if (!egsu) throw InvalidFormatException();
				} catch (...) { return PrecompilationStatus::InvalidInputFile; }
				Storage::ArchiveFile msl = 0;
				for (Storage::ArchiveFile file = 1; file <= egsu->GetFileCount(); file++) if (egsu->GetFileCustomData(file) == 0x20000) { msl = file; break; }
				if (!msl) return PrecompilationStatus::NoCodeForPlatform;
				string inter = IO::ExpandPath(IO::Path::GetDirectory(out) + L"/" + IO::Path::GetFileNameWithoutExtension(out) + L".interproc.metal");
				try {
					SafePointer<Streaming::Stream> dest = new Streaming::FileStream(inter, Streaming::AccessWrite, Streaming::CreateAlways);
					SafePointer<Streaming::Stream> src = egsu->QueryFileStream(msl, Storage::ArchiveStream::Native);
					src->CopyTo(dest);
				} catch (...) {
					try { IO::RemoveFile(inter); } catch (...) {}
					return PrecompilationStatus::OutputError;
				}
				try {
					if (log.Length()) {
						handle logfile = IO::CreateFile(log, Streaming::AccessWrite, Streaming::OpenAlways);
						IO::Seek(logfile, 0, Streaming::End);
						IO::SetStandardInput(IO::InvalidHandle);
						IO::SetStandardOutput(logfile);
						IO::SetStandardError(logfile);
					} else {
						IO::SetStandardInput(IO::InvalidHandle);
						IO::SetStandardOutput(IO::InvalidHandle);
						IO::SetStandardError(IO::InvalidHandle);
					}
				} catch (...) {
					try { IO::RemoveFile(inter); } catch (...) {}
					return PrecompilationStatus::OutputError;
				}
				Array<string> cmds(0x10);
				cmds << L"-sdk";
				cmds << L"macosx";
				cmds << L"metal";
				cmds << inter;
				cmds << L"-o";
				cmds << out;
				cmds << (L"-std=" + profile.metal_language_version);
				cmds << (L"--target=" + profile.target_system);
				SafePointer<Process> process = CreateCommandProcess(L"xcrun", &cmds);
				if (!process) {
					try { IO::RemoveFile(inter); } catch (...) {}
					return PrecompilationStatus::NoPlatformCompiler;
				}
				process->Wait();
				try { IO::RemoveFile(inter); } catch (...) {}
				if (process->GetExitCode()) return PrecompilationStatus::CompilationError;
				return PrecompilationStatus::Success;
			} catch (...) { return PrecompilationStatus::InternalError; }
		}
	}
}