#pragma once

#include "xa_types.h"

namespace Engine
{
	namespace XA
	{
		enum class Environment { Unknown = 0x00, Windows = 0x01, MacOSX = 0x02, Linux = 0x12, EFI = 0x11, XSO = 0x03 };

		class IAssemblyTranslator : public Object
		{
		public:
			virtual bool Translate(TranslatedFunction & dest, const Function & src) noexcept = 0;
			virtual uint GetWordSize(void) noexcept = 0;
			virtual Platform GetPlatform(void) noexcept = 0;
			virtual Environment GetEnvironment(void) noexcept = 0;
		};
		class IReferenceResolver
		{
		public:
			virtual uintptr ResolveReference(const string & to) noexcept = 0;
		};
		class IMemoryAllocator : public Object
		{
		public:
			virtual void * ExecutableAllocate(uintptr length) noexcept = 0;
			virtual void ExecutableFree(void * block) noexcept = 0;
			virtual void FlushExecutionCache(const void * block, uintptr length) noexcept = 0;
		};
		class IExecutable : public Object
		{
		public:
			virtual void * GetEntryPoint(const string & name) noexcept = 0;
			virtual const Volumes::Dictionary<string, void *> & GetFunctionTable(void) noexcept = 0;
			template<class F> F GetEntryPoint(const string & name) noexcept { return reinterpret_cast<F>(GetEntryPoint(name)); }
		};
		class IExecutableLinker : public Object
		{
		public:
			virtual IExecutable * LinkFunctions(const Volumes::Dictionary<string, TranslatedFunction *> & functions, IReferenceResolver * resolver = 0) noexcept = 0;
		};

		Environment GetApplicationEnvironment(void);
		IExecutableLinker * CreateLinker(void);
		IMemoryAllocator * CreateMemoryAllocator(void);
	}
}