// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <objc/runtime.h>

#define CLASS_VER              "0"
#define APP_CLASS_NAME         "__mty_app_app_"             CLASS_VER
#define WINDOW_CLASS_NAME      "__mty_app_window_"          CLASS_VER
#define VIEW_CLASS_NAME        "__mty_app_view_"            CLASS_VER
#define WEBVIEW_CLASS_NAME     "__mty_webview_webview_"     CLASS_VER
#define MSG_HANDLER_CLASS_NAME "__mty_webview_msg_handler_" CLASS_VER
#define WEBSOCKET_CLASS_NAME   "__mty_ws_websocket_"        CLASS_VER

#define OBJC_CTX() \
	(*((void **) object_getIndexedIvars(self)))

#define OBJC_ALLOCATE(super, name) \
	objc_allocateClassPair(objc_getClass(super), name, sizeof(void *))

#define OBJC_PROTOCOL(cls, name) \
	class_addProtocol(cls, objc_getProtocol(name))

#define OBJC_OVERRIDE(cls, sel, func) \
	class_addMethod(cls, sel, (IMP) func, method_getTypeEncoding(class_getInstanceMethod(cls, sel)))

typedef void (*OBJC_METHOD)(id self, SEL _cmd, ...);

static id OBJC_NEW(Class cls, void *ctx)
{
	id obj = class_createInstance(cls, sizeof(void *));
	*((void **) object_getIndexedIvars(obj)) = ctx;

	return obj;
}
