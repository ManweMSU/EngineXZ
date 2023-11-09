#include "../xexec/xx_machine.h"

using namespace Engine::XX;

int Main(void)
{
	Engine::XX::EnvironmentDesc env;
	env.xx_init_path = L"xx.ini";
	env.flags_default = EnvironmentLoggerNull | EnvironmentConsoleXC | EnvironmentWindowsAllow;
	env.flags_allow = EnvironmentLoggerNull | EnvironmentLoggerFile | EnvironmentLoggerCluster | EnvironmentConsoleXC | EnvironmentWindowsAllow;
	return Engine::XX::Main(env);
}