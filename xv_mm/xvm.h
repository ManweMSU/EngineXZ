#pragma once

#include <EngineRuntime.h>

extern Engine::UI::InterfaceTemplate interface;

void RegisterWindow(Engine::Windows::IWindow * window, Engine::Windows::IWindowCallback * callback);
void UnregisterWindow(Engine::Windows::IWindow * window);