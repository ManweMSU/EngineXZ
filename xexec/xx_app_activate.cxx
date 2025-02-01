#include "xx_app_activate.h"

#ifdef ENGINE_MACOSX
#include <objc/objc-runtime.h>
#endif

namespace Engine
{
	namespace XX
	{
		void EnforceApplicationActivation(void) noexcept
		{
			#ifdef ENGINE_MACOSX
			typedef id (* objc_msgSend_0) (Class self, SEL op);
			typedef void (* objc_msgSend_1) (id self, SEL op, BOOL arg1);
			auto nsapp = objc_getClass("NSApplication");
			if (nsapp) {
				auto sharedApplication_sel = sel_registerName("sharedApplication");
				auto activateIgnoringOtherApps_sel = sel_registerName("activateIgnoringOtherApps:");
				if (sharedApplication_sel && activateIgnoringOtherApps_sel) {
					auto app_obj = reinterpret_cast<objc_msgSend_0>(objc_msgSend)(nsapp, sharedApplication_sel);
					reinterpret_cast<objc_msgSend_1>(objc_msgSend)(app_obj, activateIgnoringOtherApps_sel, YES);
				}
			}
			#endif
		}
	}
}