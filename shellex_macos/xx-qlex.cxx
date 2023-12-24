#include <EngineRuntime.h>
#include <PlatformDependent/QuartzDevice.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#include "../ximg/xi_module.h"
#include "../ximg/xi_resources.h"

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Codec;
using namespace Engine::Graphics;
using namespace Engine::XI;

const uint8 self_guid[] = { 0xA1, 0x94, 0x8B, 0xFB, 0x80, 0x4D, 0x7A, 0xCA, 0xE3, 0x42, 0x75, 0x92, 0xE1, 0x46, 0x7C, 0x0D };

void LibraryLoaded(void) { InitializeDefaultCodecs(); }
void LibraryUnloaded(void) {}

typedef ULONG (STDMETHODCALLTYPE * func_AddRef) (void * self);
typedef ULONG (STDMETHODCALLTYPE * func_Release) (void * self);
typedef HRESULT (STDMETHODCALLTYPE * func_QueryInterface) (void * self, REFIID iid, void ** ppv);
typedef OSStatus (* func_GenerateThumbnailForURL) (void * self, QLThumbnailRequestRef thumbnail, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CGSize maxSize);
typedef void (* func_CancelThumbnailGeneration) (void * self, QLThumbnailRequestRef thumbnail);
typedef OSStatus (* func_GeneratePreviewForURL) (void * self, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options);
typedef void (* func_CancelPreviewGeneration) (void * self, QLPreviewRequestRef preview);

struct QuickLookGeneratorVFT
{
	uintptr zero;
	func_QueryInterface QueryInterface;
	func_AddRef AddRef;
	func_Release Release;
	func_GenerateThumbnailForURL GenerateThumbnailForURL;
	func_CancelThumbnailGeneration CancelThumbnailGeneration;
	func_GeneratePreviewForURL GeneratePreviewForURL;
	func_CancelPreviewGeneration CancelPreviewGeneration;
};
struct QuickLookGenerator
{
	QuickLookGeneratorVFT * vft_ptr;
	QuickLookGeneratorVFT vft;
	CFUUIDRef factory_uuid;
	uint ref_cnt;

	static ULONG STDMETHODCALLTYPE AddRef(void * self)
	{
		return InterlockedIncrement(reinterpret_cast<QuickLookGenerator *>(self)->ref_cnt);
	}
	static ULONG STDMETHODCALLTYPE Release(void * self)
	{
		auto object = reinterpret_cast<QuickLookGenerator *>(self);
		auto rc = InterlockedDecrement(object->ref_cnt);
		if (!rc) delete object;
		return rc;
	}
	static HRESULT STDMETHODCALLTYPE QueryInterface(void * self, REFIID iid, void ** ppv)
	{
		CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid);
		HRESULT result;
		if (CFEqual(uuid, kQLGeneratorCallbacksInterfaceID)) {
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
	static OSStatus GenerateThumbnailForURL(void * self, QLThumbnailRequestRef thumbnail, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CGSize maxSize)
	{
		CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
		try {
			int length = CFStringGetLength(path);
			Array<UniChar> path_chars(1);
			path_chars.SetLength(length + 1);
			CFStringGetCharacters(path, CFRangeMake(0, length), path_chars.GetBuffer());
			path_chars[length] = 0;
			auto path_str = string(path_chars.GetBuffer(), -1, Encoding::UTF16);
			SafePointer<Stream> stream = new FileStream(path_str, AccessRead, OpenExisting);
			SafePointer<Module> module = new Module(stream, Module::ModuleLoadFlags::LoadResources);
			SafePointer<Image> image = LoadModuleIcon(module->resources, 1);
			if (!image->Frames.Length()) throw Exception();
			double scale = 1.0;
			auto scale_value = (CFNumberRef) CFDictionaryGetValue(options, kQLThumbnailOptionScaleFactorKey);
			if (scale_value) CFNumberGetValue(scale_value, kCFNumberFloat64Type, &scale);
			auto frame = image->GetFrameBestSizeFit(maxSize.width * scale, maxSize.height * scale);
			SafePointer<I2DDeviceContextFactory> factory = CreateDeviceContextFactory();
			if (!factory) throw Exception();
			SafePointer<IBitmap> bitmap = factory->LoadBitmap(frame);
			if (!bitmap) throw Exception();
			CGImageRef image_ref = reinterpret_cast<CGImageRef>(Cocoa::GetBitmapCoreImage(bitmap));
			intptr prop_value_init = 0;
			CFStringRef prop_key = CFStringCreateWithCString(kCFAllocatorDefault, "IconFlavour", kCFStringEncodingASCII);
			CFStringRef prop_key_2 = CFStringCreateWithCString(kCFAllocatorDefault, "icon", kCFStringEncodingASCII);
			CFNumberRef prop_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberNSIntegerType, &prop_value_init);
			CFMutableDictionaryRef prop = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionaryAddValue(prop, prop_key, prop_value);
			CFDictionaryAddValue(prop, prop_key_2, prop_value);
			CFRelease(prop_key_2);
			CFRelease(prop_key);
			CFRelease(prop_value);
			QLThumbnailRequestSetImage(thumbnail, image_ref, prop);
			CFRelease(prop);
		} catch (...) {
			CFRelease(path);
			CFBundleRef bundle = QLThumbnailRequestGetGeneratorBundle(thumbnail);
			CFStringRef icon_name = CFStringCreateWithCString(kCFAllocatorDefault, "Icon.icns", kCFStringEncodingASCII);
			CFURLRef icon_url = CFBundleCopyResourceURL(bundle, icon_name, 0, 0);
			CFRelease(icon_name);
			intptr prop_value_init = 0;
			CFStringRef prop_key = CFStringCreateWithCString(kCFAllocatorDefault, "IconFlavour", kCFStringEncodingASCII);
			CFStringRef prop_key_2 = CFStringCreateWithCString(kCFAllocatorDefault, "icon", kCFStringEncodingASCII);
			CFNumberRef prop_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberNSIntegerType, &prop_value_init);
			CFMutableDictionaryRef prop = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionaryAddValue(prop, prop_key, prop_value);
			CFDictionaryAddValue(prop, prop_key_2, prop_value);
			CFRelease(prop_key_2);
			CFRelease(prop_key);
			CFRelease(prop_value);
			QLThumbnailRequestSetImageAtURL(thumbnail, icon_url, prop);
			CFRelease(icon_url);
			CFRelease(prop);
			return noErr;
		}
		CFRelease(path);
		return noErr;
	}
	static void CancelThumbnailGeneration(void * self, QLThumbnailRequestRef thumbnail) {}
	static OSStatus GeneratePreviewForURL(void * self, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options) { return noErr; }
	static void CancelPreviewGeneration(void * self, QLPreviewRequestRef preview) {}
	QuickLookGenerator(CFUUIDRef uuid)
	{
		vft_ptr = &vft;
		factory_uuid = uuid;
		CFRetain(factory_uuid);
		CFPlugInAddInstanceForFactory(factory_uuid);
		ref_cnt = 1;
		vft.zero = 0;
		vft.QueryInterface = QueryInterface;
		vft.AddRef = AddRef;
		vft.Release = Release;
		vft.GenerateThumbnailForURL = GenerateThumbnailForURL;
		vft.CancelThumbnailGeneration = CancelThumbnailGeneration;
		vft.GeneratePreviewForURL = GeneratePreviewForURL;
		vft.CancelPreviewGeneration = CancelPreviewGeneration;
	}
	~QuickLookGenerator(void)
	{
		CFPlugInRemoveInstanceForFactory(factory_uuid);
		CFRelease(factory_uuid);
	}
};

ENGINE_EXPORT_API void * XXClassFactory(CFAllocatorRef allocator, CFUUIDRef factory_uuid)
{
	if (CFEqual(factory_uuid, kQLGeneratorTypeID)) {
		CFUUIDRef self = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *reinterpret_cast<const CFUUIDBytes *>(&self_guid));
		auto result = new (std::nothrow) QuickLookGenerator(self);
		CFRelease(self);
		return result;
	} else return 0;
}