#include "xw_vk_com.h"

#if defined(ESSE_MODULUS_GRAPHICA_VULKAN)
#include <Graphica-Linux/Vulkan.h>
#elif defined(ENGINE_LINUX_FULL)
#include <PlatformDependent/Vulkan.h>
#endif

namespace Engine
{
	namespace XW
	{
		PrecompilationStatus VulkanCompile(const string & in, const string & out, const string & log, VulkanProfileDesc & profile)
		{
			#if defined(ESSE_MODULUS_GRAPHICA_VULKAN)
				ESSE::oref<ESSE::TextEncoder> logger;
				ESSE::oref<ESSE::DataBlock> egsu, error_log;
				try {
					auto input = ESSE::FileStream::Create(static_cast<const ESSE::unichar32 *>(in), ESSE::FileAccess::AccessRead, ESSE::FileCreationMode::OpenExisting);
					if (input->GetLength() > 0x1000000) throw Exception();
					egsu = input->ReadAll();
				} catch (...) { return PrecompilationStatus::InvalidInputFile; }
				if (log.Length()) try {
					auto logstream = ESSE::FileStream::Create(static_cast<const ESSE::unichar32 *>(log), ESSE::FileAccess::AccessWrite, ESSE::FileCreationMode::OpenAlways);
					logstream->Seek(0, ESSE::SeekOrigin::End);
					logger = ESSE::owrap(new ESSE::TextEncoder(logstream, ESSE::Unicode::Encoding::UTF8));
				} catch (...) { return PrecompilationStatus::OutputError; }
				ESSE::Vulkan::VulkanInputDesc desc;
				desc.InputClass = ESSE::Vulkan::VulkanInputUniversalShaderBundle;
				desc.VulkanVersionMajor = profile.vulkan_version_major;
				desc.VulkanVersionMinor = profile.vulkan_version_minor;
				desc.SPIRVVersionMajor = profile.spirv_version_major;
				desc.SPIRVVersionMinor = profile.spirv_version_minor;
				desc.GLSLVersionMajor = profile.glsl_version_major;
				desc.GLSLVersionMinor = profile.glsl_version_minor;
				ESSE::ErrorContext ectx; ESSE::ErrorClear(ectx);
				auto egso = ESSE::Vulkan::PrecompileShaders(egsu->GetBuffer(), egsu->GetLength(), desc, error_log, ectx);
				if (error_log && logger) try {
					logger->WriteLine(ESSE::ucs1_string(reinterpret_cast<const char *>(error_log->GetBuffer()), error_log->GetLength()));
				} catch (...) { return PrecompilationStatus::OutputError; }
				if (ESSE::ErrorTest(ectx)) {
					if (ectx.error_code == 7) {
						if (ectx.error_subcode == 3 || ectx.error_subcode == 6) return PrecompilationStatus::CompilationError;
						else if (ectx.error_subcode == 4) return PrecompilationStatus::NoCodeForPlatform;
						else return PrecompilationStatus::InternalError;
					} else if (ectx.error_code == 6) return PrecompilationStatus::OutputError;
					else if (ectx.error_code == 4) return PrecompilationStatus::InvalidInputFile;
					else if (ectx.error_code == 1) return PrecompilationStatus::NoPlatformCompiler;
					else return PrecompilationStatus::InternalError;
				}
				try {
					auto output = ESSE::FileStream::Create(static_cast<const ESSE::unichar32 *>(out), ESSE::FileAccess::AccessWrite, ESSE::FileCreationMode::CreateAlways);
					output->WriteBlock(egso);
				} catch (...) { return PrecompilationStatus::OutputError; }
				return PrecompilationStatus::Success;
			#elif defined(ENGINE_LINUX_FULL)
				SafePointer<Streaming::ITextWriter> logger;
				SafePointer<DataBlock> egsu;
				try {
					SafePointer<Streaming::Stream> input = new Streaming::FileStream(in, Streaming::AccessRead, Streaming::OpenExisting);
					if (input->Length() > 0x1000000) throw Exception();
					egsu = input->ReadAll();
				} catch (...) { return PrecompilationStatus::InvalidInputFile; }
				if (log.Length()) try {
					SafePointer<Streaming::Stream> logstream = new Streaming::FileStream(log, Streaming::AccessWrite, Streaming::OpenAlways);
					logstream->Seek(0, Streaming::End);
					logger = new Streaming::TextWriter(logstream, Encoding::UTF8);
				} catch (...) { return PrecompilationStatus::OutputError; }
				Vulkan::VulkanInputDesc desc;
				desc.InputClass = Vulkan::VulkanInputUniversalShaderBundle;
				desc.VulkanVersionMajor = profile.vulkan_version_major;
				desc.VulkanVersionMinor = profile.vulkan_version_minor;
				desc.SPIRVVersionMajor = profile.spirv_version_major;
				desc.SPIRVVersionMinor = profile.spirv_version_minor;
				desc.GLSLVersionMajor = profile.glsl_version_major;
				desc.GLSLVersionMinor = profile.glsl_version_minor;
				Graphics::ShaderError error;
				SafePointer<DataBlock> error_log;
				SafePointer<DataBlock> egso = Vulkan::PrecompileShaders(egsu->GetBuffer(), egsu->Length(), desc, &error, error_log.InnerRef());
				if (error_log && logger) try {
					logger->WriteLine(string(error_log->GetBuffer(), error_log->Length(), Encoding::UTF8));
				} catch (...) { return PrecompilationStatus::OutputError; }
				if (error != Graphics::ShaderError::Success || !egso) {
					if (error == Graphics::ShaderError::Compilation) return PrecompilationStatus::CompilationError;
					else if (error == Graphics::ShaderError::InvalidContainerData) return PrecompilationStatus::InvalidInputFile;
					else if (error == Graphics::ShaderError::IO) return PrecompilationStatus::OutputError;
					else if (error == Graphics::ShaderError::NoCompiler) return PrecompilationStatus::NoPlatformCompiler;
					else if (error == Graphics::ShaderError::NoPlatformVersion) return PrecompilationStatus::NoCodeForPlatform;
					else return PrecompilationStatus::InternalError;
				}
				try {
					Streaming::FileStream output(out, Streaming::AccessWrite, Streaming::CreateAlways);
					output.WriteArray(egso);
				} catch (...) { return PrecompilationStatus::OutputError; }
				return PrecompilationStatus::Success;
			#else
				return PrecompilationStatus::NoPlatformCompiler;
			#endif
		}
	}
}