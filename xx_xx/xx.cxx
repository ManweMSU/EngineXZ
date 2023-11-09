#include "../xexec/xx_machine.h"

using namespace Engine::XX;

int Main(void)
{
	Engine::XX::EnvironmentDesc env;
	env.xx_init_path = L"xx.ini";
	env.flags_default = EnvironmentLoggerConsole | EnvironmentConsoleSystem | EnvironmentWindowsDelegate;
	env.flags_allow = EnvironmentLoggerMask | EnvironmentConsoleMask | EnvironmentWindowsDelegate;
	return Engine::XX::Main(env);
}