#pragma once

#include "ertxpc/runtime/EngineRuntime.h"
#include <CoreGraphics/CoreGraphics.h>
#include <objc/objc-runtime.h>

typedef Engine::Volumes::Dictionary<Engine::ImmutableString, Engine::ImmutableString> XIMetadata;

extern "C" int NSExtensionMain(int argc, char ** argv);
extern "C" bool XILoadMetadata(CFDataRef data, Engine::ImmutableString & mdname, Engine::ImmutableString & enc, XIMetadata ** mdata);