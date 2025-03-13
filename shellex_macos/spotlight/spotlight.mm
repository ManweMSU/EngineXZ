#include "xxspex.h"
#include "xi_se/xi_resources.h"
#include "ertxpc/runtime/PlatformDependent/CocoaInterop.h"

@import CoreSpotlight;

@interface XIImportor : CSImportExtension
@end
@implementation XIImportor
- (BOOL) updateAttributes: (CSSearchableItemAttributeSet *) attributes forFileAtURL: (NSURL *) contentURL error: (NSError **) error {
	auto data = [NSData dataWithContentsOfURL: contentURL];
	Engine::SafePointer<XIMetadata> meta;
	Engine::ImmutableString module_name, encoder;
	if (data && XILoadMetadata(reinterpret_cast<CFDataRef>(data), module_name, encoder, meta.InnerRef())) {
		NSString * string;
		if (meta) {
			Engine::ImmutableString * attr;
			attr = meta->GetElementByKey(Engine::XI::MetadataKeyModuleName);
			if (attr) {
				string = Engine::Cocoa::CocoaString(*attr);
				[attributes setTitle: string];
				[string release];
			}
			attr = meta->GetElementByKey(Engine::XI::MetadataKeyCompanyName);
			if (attr) {
				string = Engine::Cocoa::CocoaString(*attr);
				[attributes setOrganizations: @[ string ]];
				[string release];
			}
			attr = meta->GetElementByKey(Engine::XI::MetadataKeyCopyright);
			if (attr) {
				string = Engine::Cocoa::CocoaString(*attr);
				[attributes setCopyright: string];
				[string release];
			}
			attr = meta->GetElementByKey(Engine::XI::MetadataKeyVersion);
			if (attr) {
				string = Engine::Cocoa::CocoaString(*attr);
				[attributes setVersion: string];
				[string release];
			}
		}
		string = Engine::Cocoa::CocoaString(module_name);
		[attributes setOriginalSource: string];
		[string release];
		string = Engine::Cocoa::CocoaString(encoder);
		[attributes setCreator: string];
		[string release];
	}
	if (error) *error = 0;
	return true;
}
@end