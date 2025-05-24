#include "xw_vk_com.h"

#ifdef ENGINE_LINUX_FULL
#include <PlatformDependent/Vulkan.h>
#endif

namespace Engine
{
	namespace XW
	{
		PrecompilationStatus VulkanCompile(const string & in, const string & out, const string & log, VulkanProfileDesc & profile)
		{
			#ifdef ENGINE_LINUX_FULL
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