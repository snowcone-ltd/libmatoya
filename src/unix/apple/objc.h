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

#define OBJC_CTX_CLEAR(obj) \
	*((void **) object_getIndexedIvars(obj)) = nil;

#define OBJC_ALLOCATE(super, name) \
	objc_allocateClassPair(objc_getClass(super), name, sizeof(void *))

typedef void (*OBJC_METHOD)(id self, SEL _cmd, ...);

static inline void OBJC_POVERRIDE(Class cls, Protocol *proto, BOOL required, SEL sel, void *func)
{
	struct objc_method_description desc = protocol_getMethodDescription(proto, sel, required, YES);
	if (!desc.types) {
		MTY_Log("'protocol_getMethodDescription failed for '%s:%s'", protocol_getName(proto), sel_getName(sel));
		return;
	}

	if (!class_addMethod(cls, sel, (IMP) func, desc.types))
		MTY_Log("'class_addMethod' failed for '%s:%s'", protocol_getName(proto), sel_getName(sel));
}

static inline void OBJC_OVERRIDE(Class cls, SEL sel, void *func)
{
	Method method = class_getInstanceMethod(cls, sel);
	if (!method) {
		MTY_Log("'class_getInstanceMethod' failed for '%s'", sel_getName(sel));
		return;
	}

	const char *type_encoding = method_getTypeEncoding(method);
	if (!type_encoding) {
		MTY_Log("'method_getTypeEncoding' failed for '%s'", sel_getName(sel));
		return;
	}

	if (!class_addMethod(cls, sel, (IMP) func, type_encoding))
		MTY_Log("'class_addMethod' failed for '%s'", sel_getName(sel));
}

static inline Protocol *OBJC_PROTOCOL(Class cls, Protocol *proto)
{
	if (proto) {
		if (!class_addProtocol(cls, proto))
			MTY_Log("'class_addProtocol' failed for '%s'", protocol_getName(proto));

	} else {
		MTY_Log("'OBJC_PROTOCOL' was called with NULL argument");
	}

	return proto;
}

static inline id OBJC_NEW(Class cls, void *ctx)
{
	id obj = class_createInstance(cls, sizeof(void *));
	*((void **) object_getIndexedIvars(obj)) = ctx;

	return obj;
}
