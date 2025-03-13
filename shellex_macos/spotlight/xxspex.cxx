#include "ertxpc/runtime/EngineBase.h"
#include "ertxpc/runtime/PlatformDependent/CocoaInterop2.h"

#include "xxspex.h"
#include "xi_se/xi_module.h"
#include "xi_se/xi_resources.h"

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::Cocoa;
using namespace Engine::XI;

int main(int argc, char ** argv) { return NSExtensionMain(argc, argv); }
extern "C" bool XILoadMetadata(CFDataRef data, Engine::ImmutableString & mdname, Engine::ImmutableString & enc, XIMetadata ** mdata)
{
	try {
		SafePointer<MemoryStream> stream = new MemoryStream(CFDataGetBytePtr(data), CFDataGetLength(data));
		SafePointer<Module> module = new Module(stream);
		mdname = module->module_import_name;
		enc = module->assembler_name + L" " + string(module->assembler_version.major) + L"." + string(module->assembler_version.minor) +
			L"." + string(module->assembler_version.subver) + L"." + string(module->assembler_version.build);
		*mdata = LoadModuleMetadata(module->resources);
		return true;
	} catch (...) { return false; }
}