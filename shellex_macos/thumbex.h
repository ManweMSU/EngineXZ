#pragma once

#include <CoreGraphics/CoreGraphics.h>
#include <objc/objc-runtime.h>

extern "C" int NSExtensionMain(int argc, char ** argv);
extern "C" CGImageRef XILoadImageIcon(CFDataRef data, int width, int height);
extern "C" void SetIconFlavor(id rpl, int flavor);