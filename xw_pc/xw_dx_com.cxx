#include "xw_dx_com.h"

#ifdef ENGINE_WINDOWS
#include <d3dcompiler.h>
typedef HRESULT (WINAPI * func_D3DCompile) (LPCVOID src_data, SIZE_T src_size, LPCSTR name, const D3D_SHADER_MACRO * defines,
	ID3DInclude * include, LPCSTR entrypoint, LPCSTR target_model, UINT flags1, UINT flags2, ID3DBlob ** result, ID3DBlob ** errors);
#endif

namespace Engine
{
	namespace XW
	{
		struct DXCompileBlob : public Object
		{
			SafePointer<DataBlock> shader;
			string name;
			uint usage;
		};
		PrecompilationStatus DXCompile(const string & in, const string & out, const string & log, DXProfileDesc & profile)
		{
			#ifdef ENGINE_WINDOWS
			try {
				SafePointer<Streaming::ITextWriter> logger;
				ObjectArray<DXCompileBlob> blobs(0x100);
				if (log.Length()) try {
					SafePointer<Streaming::Stream> logstream = new Streaming::FileStream(log, Streaming::AccessWrite, Streaming::OpenAlways);
					logstream->Seek(0, Streaming::End);
					logger = new Streaming::TextWriter(logstream, Encoding::UTF8);
				} catch (...) { return PrecompilationStatus::OutputError; }
				SafePointer<Storage::Archive> egsu;
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::FileStream(in, Streaming::AccessRead, Streaming::OpenExisting);
					egsu = Storage::OpenArchive(stream, Storage::ArchiveMetadataUsage::IgnoreMetadata);
					if (!egsu) throw InvalidFormatException();
				} catch (...) { return PrecompilationStatus::InvalidInputFile; }
				HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
				if (!compiler) return PrecompilationStatus::NoPlatformCompiler;
				auto Compile = reinterpret_cast<func_D3DCompile>(GetProcAddress(compiler, "D3DCompile"));
				if (!Compile) {
					FreeLibrary(compiler);
					return PrecompilationStatus::NoPlatformCompiler;
				}
				for (Storage::ArchiveFile file = 1; file <= egsu->GetFileCount(); file++) if ((egsu->GetFileCustomData(file) & 0xFFFF0000) == 0x10000) {
					SafePointer<DXCompileBlob> blob = new DXCompileBlob;
					blob->usage = egsu->GetFileCustomData(file) & 0xFFFF;
					string local_profile, full_name = egsu->GetFileName(file);
					if (blob->usage == 0x01) local_profile = profile.vertex_shader_profile;
					else if (blob->usage == 0x02) local_profile = profile.pixel_shader_profile;
					else return PrecompilationStatus::InvalidInputFile;
					int del = full_name.FindFirst(L'!');
					if (del < 0) return PrecompilationStatus::InvalidInputFile;
					blob->name = full_name.Fragment(0, del);
					auto entry_point = full_name.Fragment(del + 1, -1);
					SafePointer<Streaming::Stream> stream = egsu->QueryFileStream(file, Storage::ArchiveStream::Native);
					int source_length = stream->Length();
					int entry_length = entry_point.GetEncodedLength(Encoding::ANSI) + 1;
					int profile_length = local_profile.GetEncodedLength(Encoding::ANSI) + 1;
					int entry_offset = source_length;
					int profile_offset = entry_offset + entry_length;
					Array<char> data(1);
					data.SetLength(source_length + entry_length + profile_length);
					stream->Read(data.GetBuffer(), source_length);
					entry_point.Encode(data.GetBuffer() + entry_offset, Encoding::ANSI, true);
					local_profile.Encode(data.GetBuffer() + profile_offset, Encoding::ANSI, true);
					ID3DBlob * output = 0, * errors = 0;
					if (Compile(data.GetBuffer(), source_length, "", 0, 0, data.GetBuffer() + entry_offset, data.GetBuffer() + profile_offset,
						D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &output, &errors) == S_OK) {
						if (errors) {
							if (logger) logger->WriteLine(string(errors->GetBufferPointer(), errors->GetBufferSize(), Encoding::ANSI));
							errors->Release();
						}
						if (output) {
							blob->shader = new DataBlock(1);
							blob->shader->Append(reinterpret_cast<const uint8 *>(output->GetBufferPointer()), output->GetBufferSize());
							output->Release();
						} else PrecompilationStatus::CompilationError;
					} else {
						if (errors) {
							if (logger) logger->WriteLine(string(errors->GetBufferPointer(), errors->GetBufferSize(), Encoding::ANSI));
							errors->Release();
						}
						return PrecompilationStatus::CompilationError;
					}
					blobs.Append(blob);
				}
				if (!blobs.Length()) return PrecompilationStatus::NoCodeForPlatform;
				SafePointer<Streaming::Stream> output_raw = new Streaming::MemoryStream(0x10000);
				SafePointer<Storage::NewArchive> archive = Storage::CreateArchive(output_raw, blobs.Length(), Storage::NewArchiveFlags::UseFormat32);
				Storage::ArchiveFile file = 0;
				for (auto & blob : blobs) {
					file++;
					archive->SetFileName(file, blob.name);
					archive->SetFileData(file, blob.shader->GetBuffer(), blob.shader->Length());
					archive->SetFileCustom(file, blob.usage);
				}
				archive->Finalize();
				archive.SetReference(0);
				try {
					Streaming::FileStream output(out, Streaming::AccessWrite, Streaming::CreateAlways);
					output_raw->Seek(0, Streaming::Begin);
					output_raw->CopyTo(&output);
				} catch (...) { return PrecompilationStatus::OutputError; }
				return PrecompilationStatus::Success;
			} catch (...) { return PrecompilationStatus::InternalError; }
			#else
			return PrecompilationStatus::NoPlatformCompiler;
			#endif
		}
	}
}