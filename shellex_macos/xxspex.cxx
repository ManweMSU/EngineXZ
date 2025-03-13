#include "ertxpc/runtime/EngineRuntime.h"
#include "ertxpc/runtime/PlatformDependent/CocoaInterop2.h"
#define ENGINE_EXPORT_API extern "C" __attribute__((visibility("default")))

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <CoreServices/CoreServices.h>

#include "xi_se/xi_module.h"
#include "xi_se/xi_resources.h"

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::XI;

const uint8 self_guid[] = { 0xA1, 0x94, 0x8B, 0xFB, 0x80, 0x4D, 0x7A, 0xCA, 0xE3, 0x42, 0x75, 0x92, 0xE1, 0x46, 0x7C, 0x0D };
struct Importer;

typedef ULONG (STDMETHODCALLTYPE * func_AddRef) (Importer * self);
typedef ULONG (STDMETHODCALLTYPE * func_Release) (Importer * self);
typedef HRESULT (STDMETHODCALLTYPE * func_QueryInterface) (Importer * self, REFIID iid, void ** ppv);
typedef ::Boolean (* func_ImporterImportData) (Importer * self, CFMutableDictionaryRef attributes, CFStringRef contentTypeUTI, CFStringRef pathToFile);
typedef Engine::Volumes::Dictionary<Engine::ImmutableString, Engine::ImmutableString> XIMetadata;

struct type_ImporterVFT {
	uintptr zero;
	func_QueryInterface ptr_QueryInterface;
	func_AddRef ptr_AddRef;
	func_Release ptr_Release;
	func_ImporterImportData ptr_ImporterImportData;
};
type_ImporterVFT ImporterVFT;

struct Importer
{
	type_ImporterVFT * vft;
	CFUUIDRef factory_uuid;
	uint ref_cnt;

	Importer(CFUUIDRef uuid) : vft(&ImporterVFT), factory_uuid(uuid), ref_cnt(1) { CFRetain(factory_uuid); CFPlugInAddInstanceForFactory(factory_uuid); }
	~Importer(void) { CFPlugInRemoveInstanceForFactory(factory_uuid); CFRelease(factory_uuid); }
};

ULONG STDMETHODCALLTYPE AddRef(Importer * self) { return InterlockedIncrement(self->ref_cnt); }
ULONG STDMETHODCALLTYPE Release(Importer * self) { auto rc = InterlockedDecrement(self->ref_cnt); if (!rc) delete self; return rc; }
HRESULT STDMETHODCALLTYPE QueryInterface(Importer * self, REFIID iid, void ** ppv)
{
	CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid);
	if (!uuid) return E_OUTOFMEMORY;
	HRESULT result;
	if (CFEqual(uuid, kMDImporterInterfaceID)) {
		*ppv = self;
		AddRef(self);
		result = S_OK;
	} else {
		*ppv = 0;
		result = E_NOINTERFACE;
	}
	CFRelease(uuid);
	return result;
}
CFStringRef AllocateString(const string & value)
{
	const widechar * pdata = value;
	return CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(pdata), value.Length() * 4, kCFStringEncodingUTF32LE, FALSE);
}
CFStringRef AllocateString(const char * value) { return CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingASCII); }
void SetAttribute(CFMutableDictionaryRef attributes, CFStringRef key, const string * value) noexcept
{
	auto string = AllocateString(*value);
	auto array = CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(&string), 1, &kCFTypeArrayCallBacks);
	CFDictionarySetValue(attributes, key, array);
	CFRelease(array);
	CFRelease(string);
}
void SetAttribute(CFMutableDictionaryRef attributes, CFStringRef key, const string & value) noexcept
{
	auto string = AllocateString(value);
	CFDictionarySetValue(attributes, key, string);
	CFRelease(string);
}
void SetAttribute(CFMutableDictionaryRef attributes, const char * key, const string & value) noexcept
{
	auto string = AllocateString(value);
	auto keystr = AllocateString(key);
	CFDictionarySetValue(attributes, keystr, string);
	CFRelease(string);
	CFRelease(keystr);
}
::Boolean ImporterImportData(Importer * self, CFMutableDictionaryRef attributes, CFStringRef contentTypeUTI, CFStringRef pathToFile)
{
	string import_name, assembler;
	string * attr;
	SafePointer<XIMetadata> meta;
	try {
		SafePointer<Stream> stream = new FileStream(Cocoa::EngineString(pathToFile), AccessRead, OpenExisting);
		SafePointer<Module> module = new Module(stream);
		import_name = module->module_import_name;
		assembler = module->assembler_name + L" " + string(module->assembler_version.major) + L"." + string(module->assembler_version.minor) +
			L"." + string(module->assembler_version.subver) + L"." + string(module->assembler_version.build);
		meta = LoadModuleMetadata(module->resources);
	} catch (...) { return FALSE; }
	SetAttribute(attributes, "com_EngineSoftware_XI_InternalName", import_name);
	SetAttribute(attributes, kMDItemCreator, assembler);
	if (meta) {
		attr = meta->GetElementByKey(MetadataKeyModuleName);
		if (attr) SetAttribute(attributes, kMDItemTitle, *attr);
		attr = meta->GetElementByKey(MetadataKeyCompanyName);
		if (attr) SetAttribute(attributes, kMDItemOrganizations, attr);
		attr = meta->GetElementByKey(MetadataKeyCopyright);
		if (attr) SetAttribute(attributes, kMDItemCopyright, *attr);
		attr = meta->GetElementByKey(MetadataKeyVersion);
		if (attr) SetAttribute(attributes, kMDItemVersion, *attr);
	}
	return TRUE;
}

void LibraryLoaded(void) {}
void LibraryUnloaded(void) {}

ENGINE_EXPORT_API void * XXClassFactory(CFAllocatorRef allocator, CFUUIDRef factory_uuid)
{
	ImporterVFT.zero = 0;
	ImporterVFT.ptr_QueryInterface = QueryInterface;
	ImporterVFT.ptr_AddRef = AddRef;
	ImporterVFT.ptr_Release = Release;
	ImporterVFT.ptr_ImporterImportData = ImporterImportData;
	if (CFEqual(factory_uuid, kMDImporterTypeID)) {
		CFUUIDRef self = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *reinterpret_cast<const CFUUIDBytes *>(&self_guid));
		if (!self) return 0;
		auto result = new (std::nothrow) Importer(self);
		CFRelease(self);
		return result;
	} else return 0;
}