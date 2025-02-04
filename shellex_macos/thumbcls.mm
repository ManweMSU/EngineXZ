#include "thumbex.h"
#include "ertxpc/runtime/EngineBase.h"

@import QuickLookThumbnailing;
@import Quartz;

@interface XIProvisor : QLThumbnailProvider
@end
@implementation XIProvisor
- (void) provideThumbnailForFileRequest: (QLFileThumbnailRequest *) request completionHandler: (void (^) (QLThumbnailReply *, NSError *)) handler {
	auto rq_mx_size = [request maximumSize];
	auto rq_scale = [request scale];
	auto rq_url = [request fileURL];
	auto size = Engine::min(rq_mx_size.width, rq_mx_size.height);
	auto size_scaled = Engine::max(int(size * rq_scale), 1);
	auto data = [NSData dataWithContentsOfURL: rq_url];
	CGImageRef icon = 0;
	if (data) icon = XILoadImageIcon(reinterpret_cast<CFDataRef>(data), size_scaled, size_scaled);
	if (icon) {
		auto block = ^ BOOL {
			CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
			auto w = CGImageGetWidth(icon) / rq_scale;
			auto h = CGImageGetHeight(icon) / rq_scale;
			CGContextDrawImage(ctx, NSMakeRect((rq_mx_size.width - w) / 2.0, (rq_mx_size.height - h) / 2.0, w, h), icon);
			CGImageRelease(icon);
			return YES;
		};
		if (!block) CGImageRelease(icon);
		auto rpl = [QLThumbnailReply replyWithContextSize: rq_mx_size currentContextDrawingBlock: block];
		SetIconFlavor(rpl, 0);
		handler(rpl, 0);
	} else {
		auto rpl = [QLThumbnailReply replyWithImageFileURL: [[NSBundle mainBundle] URLForResource: @"Icon" withExtension: @"icns"]];
		SetIconFlavor(rpl, 0);
		handler(rpl, 0);
	}
}
@end