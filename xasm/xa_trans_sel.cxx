#include "xa_trans_sel.h"

#include "xa_t_i386.h"
#include "xa_t_x64.h"
#include "xa_t_armv8.h"

namespace Engine
{
	namespace XA
	{
		IAssemblyTranslator * CreatePlatformTranslator(void) { return CreatePlatformTranslator(ApplicationPlatform, GetApplicationEnvironment()); }
		IAssemblyTranslator * CreatePlatformTranslator(Platform platform, Environment env)
		{
			if (platform == Platform::X86) return CreateTranslatorX86i386(env);
			else if (platform == Platform::X64) return CreateTranslatorX64(env);
			else if (platform == Platform::ARM) return 0;
			else if (platform == Platform::ARM64) return CreateTranslatorARMv8(env);
			else return 0;
		}
	}
}