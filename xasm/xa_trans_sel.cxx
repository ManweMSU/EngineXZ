#include "xa_trans_sel.h"

#include "xa_t_i386.h"
#include "xa_t_x64.h"
#include "xa_t_armv8.h"

namespace Engine
{
	namespace XA
	{
		IAssemblyTranslator * CreatePlatformTranslator(void) { return CreatePlatformTranslator(ApplicationPlatform, GetApplicationCallingConvention()); }
		IAssemblyTranslator * CreatePlatformTranslator(Platform platform, CallingConvention conv)
		{
			if (platform == Platform::X86) return CreateTranslatorX86i386(conv);
			else if (platform == Platform::X64) return CreateTranslatorX64(conv);
			else if (platform == Platform::ARM) return 0;
			else if (platform == Platform::ARM64) return CreateTranslatorARMv8(conv);
			else return 0;
		}
	}
}