#pragma once

#include "xvm.h"
#include "../xv/xv_manual.h"

void CreateBrowser(const Engine::ImmutableString & path);
void CreateBrowser(const Engine::ImmutableString & path, Engine::XV::ManualVolume * volume);
void CreateBrowser(const Engine::ImmutableString & file, const Engine::ImmutableString & text);