#pragma once

#include <EngineRuntime.h>

#include "../xexec/xx_com.h"

extern Engine::UI::InterfaceTemplate interface;

void RegisterWindow(Engine::Windows::IWindow * window, Engine::Windows::IWindowCallback * callback);
void UnregisterWindow(Engine::Windows::IWindow * window);

void OpenFileDialog(Engine::Windows::OpenFileInfo & info, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task);
void SaveFileDialog(Engine::Windows::SaveFileInfo & info, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task);
void ChooseDirectoryDialog(Engine::Windows::ChooseDirectoryInfo & info, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task);
void MessageBox(Engine::Windows::MessageBoxResult * result, const Engine::string & text, const Engine::string & title, Engine::Windows::IWindow * parent, Engine::Windows::MessageBoxButtonSet buttons, Engine::Windows::MessageBoxStyle style, Engine::IDispatchTask * task);
void PasswordInput(Engine::string & input, bool & status, Engine::Windows::IWindow * modally, Engine::IDispatchTask * task);