#pragma once

#include "xvm.h"
#include "../xv/xv_manual.h"

extern Engine::ImmutableString ColorGrayBlue;
extern Engine::ImmutableString ColorGray;
extern Engine::ImmutableString ColorRed;
extern Engine::ImmutableString ColorBlue;
extern Engine::ImmutableString ColorGreen;
extern Engine::ImmutableString ColorCyan;
extern Engine::ImmutableString ColorPurple;

void CreateBrowser(const Engine::ImmutableString & path, bool xw = false);
void CreateBrowser(const Engine::ImmutableString & path, Engine::XV::ManualVolume * volume);
void CreateBrowser(const Engine::ImmutableString & file, const Engine::ImmutableString & text);