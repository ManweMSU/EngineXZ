#pragma once

#include "../xv/xv_compiler.h"

bool LaunchInteractiveEditor(const Engine::string & file, Engine::Array<Engine::uint32> * code, Engine::XV::ICompilerCallback * callback, Engine::IO::Console & console);