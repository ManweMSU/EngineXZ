#include "../xexec/xx_machine.h"

using namespace Engine::XX;

int Main(void)
{
	#ifdef ENGINE_MACOSX
	Engine::IO::SetStandardOutput(Engine::IO::InvalidHandle);
	Engine::IO::SetStandardInput(Engine::IO::InvalidHandle);
	Engine::IO::SetStandardError(Engine::IO::InvalidHandle);
	#endif
	Engine::XX::EnvironmentDesc env;
	env.xx_init_path = L"xx.ini";
	env.flags_default = EnvironmentLoggerNull | EnvironmentConsoleXC | EnvironmentWindowsAllow;
	env.flags_allow = EnvironmentLoggerNull | EnvironmentLoggerFile | EnvironmentConsoleXC | EnvironmentWindowsAllow;
	return Engine::XX::Main(env);
}