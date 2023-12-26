#pragma once

#include "xe_ctx.h"

#define ENGINE_XE_DL_INIT_ROUTINE_NAME		InitiaXE
#define ENGINE_XE_DL_INIT_ROUTINE(PATH)		ENGINE_EXPORT_API bool ENGINE_XE_DL_INIT_ROUTINE_NAME(const char * PATH)

namespace Engine
{
	namespace XE
	{
		enum StandardLoaderFlags {
			UseStandardFPU	= 0x0001,
			UseStandardMMU	= 0x0002,
			UseStandardSPU	= 0x0004,
			UseStandardLD	= 0x0008,
			UseStandardMisc	= 0x0010,

			UseStandard		= UseStandardFPU | UseStandardMMU | UseStandardSPU | UseStandardLD | UseStandardMisc,
		};

		class IExtension : public Object {};
		class IAPIExtension : public IExtension
		{
		public:
			virtual const void * ExposeRoutine(const string & routine_name) noexcept = 0;
			virtual const void * ExposeInterface(const string & interface) noexcept = 0;
		};
		class IModuleExtension : public IExtension
		{
		public:
			virtual Streaming::Stream * OpenModule(const string & module_name) noexcept = 0;
		};

		class StandardLoader : public Object, public ILoaderCallback
		{
		public:
			// Module search paths
			virtual bool AddModuleSearchPath(const string & path) noexcept = 0;
			virtual Array<string> & GetModuleSearchPaths(void) noexcept = 0;
			// DL search paths
			virtual bool AddDynamicLibrarySearchPath(const string & path) noexcept = 0;
			virtual Array<string> & GetDynamicLibrarySearchPaths(void) noexcept = 0;
			// Module loader extensions
			virtual bool RegisterModuleExtension(IModuleExtension * ext) noexcept = 0;
			virtual bool UnregisterModuleExtension(IModuleExtension * ext) noexcept = 0;
			virtual ObjectArray<IModuleExtension> & GetModuleExtensions(void) noexcept = 0;
			// API extensions
			virtual bool RegisterAPIExtension(IAPIExtension * ext) noexcept = 0;
			virtual bool UnregisterAPIExtension(IAPIExtension * ext) noexcept = 0;
			virtual ObjectArray<IAPIExtension> & GetAPIExtensions(void) noexcept = 0;
			// Error reporting
			virtual bool IsAlive(void) noexcept = 0;
			virtual ModuleLoadError GetLastError(void) noexcept = 0;
			virtual const string & GetLastErrorModule(void) noexcept = 0;
			virtual const string & GetLastErrorSubject(void) noexcept = 0;
		};

		StandardLoader * CreateStandardLoader(uint flags);
		handle LoadLibraryXE(const string & path);
	}
}