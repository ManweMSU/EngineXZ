#include "ertxpc/runtime/EngineBase.h"
#include "ertxpc/runtime/PlatformDependent/CocoaInterop2.h"

#include "thumbex.h"
#include "xi_se/xi_module.h"
#include "xi_se/xi_resources.h"

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Codec;
using namespace Engine::Cocoa;
using namespace Engine::XI;

int main(int argc, char ** argv) { Storage::CreateVolumeCodec(); return NSExtensionMain(argc, argv); }
extern "C" CGImageRef XILoadImageIcon(CFDataRef data, int width, int height)
{
	CGImageRef result = 0;
	try {
		SafePointer<MemoryStream> stream = new MemoryStream(CFDataGetBytePtr(data), CFDataGetLength(data));
		SafePointer<Module> module = new Module(stream);
		SafePointer<Frame> frame = LoadModuleIcon(module->resources, 1, width, height);
		if (!frame) throw Exception();
		result = CocoaCoreImage(frame);
		if (!result) throw OutOfMemoryException();
	} catch (...) {
		if (result) CGImageRelease(result);
		return 0;
	}
	return result;
}
extern "C" void SetIconFlavor(id rpl, int flavor)
{
	typedef void (* objc_msgSend_int) (id, SEL, int);
	auto setIconFlavor_sel = sel_registerName("setIconFlavor:");
	if (setIconFlavor_sel) reinterpret_cast<objc_msgSend_int>(objc_msgSend)(rpl, setIconFlavor_sel, flavor);
}