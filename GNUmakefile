UNAME_S = $(shell uname -s)
ARCH = $(shell uname -m)
NAME = libmatoya

.SUFFIXES: .vert .frag .fragvk .vertvk

.vert.h:
	@hexdump -ve '1/1 "0x%.2x,"' $< | (echo 'static const GLchar VERT[]={' && cat && echo '0x00};') > $@

.frag.h:
	@hexdump -ve '1/1 "0x%.2x,"' $< | (echo 'static const GLchar FRAG[]={' && cat && echo '0x00};') > $@

.vertvk.h:
	@deps/bin/glslangValidator -S vert -V --vn VERT $< -o $@

.fragvk.h:
	@deps/bin/glslangValidator -S frag -V --vn FRAG $< -o $@

.m.o:
	$(CC) $(OCFLAGS)  -c -o $@ $<

OBJS = \
	src/app.o \
	src/crypto.o \
	src/file.o \
	src/hash.o \
	src/image.o \
	src/json.o \
	src/list.o \
	src/log.o \
	src/memory.o \
	src/queue.o \
	src/render.o \
	src/resample.o \
	src/system.o \
	src/thread.o \
	src/tlocal.o \
	src/tls.o \
	src/version.o \
	src/gfx/gl/gl.o \
	src/gfx/gl/gl-ui.o \
	src/hid/utils.o \
	src/unix/file.o \
	src/unix/memory.o \
	src/unix/system.o \
	src/unix/thread.o \
	src/unix/time.o

SHADERS = \
	src/gfx/gl/shaders/fs.h \
	src/gfx/gl/shaders/vs.h \
	src/gfx/gl/shaders/fsui.h \
	src/gfx/gl/shaders/vsui.h

INCLUDES = \
	-Ideps \
	-Isrc \
	-Isrc/unix

FLAGS = \
	-Wall \
	-Wextra \
	-Wshadow \
	-Wno-switch \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-std=c99 \
	-fPIC

ifdef DEBUG
FLAGS := $(FLAGS) -O0 -g
DEFS := $(DEFS) -DMTY_VK_DEBUG
else
FLAGS := $(FLAGS) -O3 -g0 -fvisibility=hidden
endif

############
### WASM ###
############

# github.com/WebAssembly/wasi-sdk/releases -> ~/wasi-sdk-xx

ifdef WASM
WASI_SDK = $(HOME)/wasi-sdk-16.0

CC = $(WASI_SDK)/bin/clang --sysroot=$(WASI_SDK)/share/wasi-sysroot
AR = $(WASI_SDK)/bin/ar

ARCH := wasm32

OBJS := $(OBJS) \
	src/unix/web/app.o \
	src/unix/web/dialog.o \
	src/unix/web/system.o \
	src/unix/web/webview.o \
	src/unix/web/gfx/gl-ctx.o

DEFS := $(DEFS) \
	-D_WASI_EMULATED_SIGNAL \
	-DCLOCK_MONOTONIC_RAW=CLOCK_MONOTONIC \
	-DMTY_GLUI_CLEAR_ALPHA=0.0f \
	-DMTY_GL_EXTERNAL \
	-DMTY_GL_ES

TARGET = web
INCLUDES := $(INCLUDES) -Isrc/unix/web

else
#############
### LINUX ###
#############
ifeq ($(UNAME_S), Linux)

ifdef STEAM
WEBVIEW_OBJ = src/swebview.o
else
WEBVIEW_OBJ = src/unix/linux/x11/webview.o
endif

OBJS := $(OBJS) \
	src/gfx/vk/vk.o \
	src/gfx/vk/vk-ctx.o \
	src/gfx/vk/vk-ui.o \
	src/net/async.o \
	src/net/dns.o \
	src/net/http-parse.o \
	src/net/http-proxy.o \
	src/unix/image.o \
	src/unix/linux/dialog.o \
	src/unix/linux/x11/aes-gcm.o \
	src/unix/linux/x11/app.o \
	src/unix/linux/x11/audio.o \
	src/unix/linux/x11/crypto.o \
	src/unix/linux/x11/evdev.o \
	src/unix/linux/x11/request.o \
	src/unix/linux/x11/system.o \
	src/unix/linux/x11/tls.o \
	src/unix/linux/x11/gfx/gl-ctx.o \
	src/unix/net/gzip.o \
	src/unix/net/http.o \
	src/unix/net/net.o \
	src/unix/net/secure.o \
	src/unix/net/tcp.o \
	src/unix/net/ws.o \
	$(WEBVIEW_OBJ)

SHADERS := $(SHADERS) \
	src/gfx/vk/shaders/fs.h \
	src/gfx/vk/shaders/vs.h \
	src/gfx/vk/shaders/fsui.h \
	src/gfx/vk/shaders/vsui.h

DEFS := $(DEFS) \
	-DMTY_VK_XLIB

TARGET = linux
INCLUDES := $(INCLUDES) -Isrc/unix/linux -Isrc/unix/linux/x11
endif

#############
### APPLE ###
#############
ifeq ($(UNAME_S), Darwin)

.SUFFIXES: .metal

.metal.h:
	hexdump -ve '1/1 "0x%.2x,"' $< | (echo 'static const char MTL_LIBRARY[]={' && cat && echo '0x00};') > $@

ifndef TARGET
TARGET = macosx
endif

ifndef ARCH
ARCH = x86_64
endif

ifeq ($(TARGET), macosx)
MIN_VER = 10.14

#TODO Remove after phase3
FLAGS := $(FLAGS) -Wno-deprecated-declarations

OBJS := $(OBJS) \
	src/hid/hid.o \
	src/unix/apple/image.o \
	src/unix/apple/macosx/hid.o \
	src/unix/apple/macosx/gfx/gl-ctx.o \
	src/unix/apple/macosx/gfx/metal-ctx.o

else
MIN_VER = 11.0
FLAGS := $(FLAGS) -fembed-bitcode
DEFS := $(DEFS) -DMTY_GL_ES

ifeq ($(ARCH), x86_64)
FLAGS := $(FLAGS) -maes -mpclmul
endif

endif

OBJS := $(OBJS) \
	src/net/async.o \
	src/net/dns.o \
	src/net/http-parse.o \
	src/net/http-proxy.o \
	src/unix/apple/request.o \
	src/unix/apple/audio.o \
	src/unix/apple/crypto.o \
	src/unix/apple/tls.o \
	src/unix/apple/webview.o \
	src/unix/apple/gfx/metal.o \
	src/unix/apple/gfx/metal-ui.o \
	src/unix/apple/$(TARGET)/aes-gcm.o \
	src/unix/apple/$(TARGET)/app.o \
	src/unix/apple/$(TARGET)/dialog.o \
	src/unix/apple/$(TARGET)/system.o \
	src/unix/net/gzip.o \
	src/unix/net/http.o \
	src/unix/net/net.o \
	src/unix/net/secure.o \
	src/unix/net/tcp.o \
	src/unix/net/ws.o

SHADERS := $(SHADERS) \
	src/unix/apple/gfx/shaders/metal/quad.h \
	src/unix/apple/gfx/shaders/metal/ui.h

DEFS := $(DEFS) \
	-DMTY_GL_EXTERNAL

FLAGS := $(FLAGS) \
	-m$(TARGET)-version-min=$(MIN_VER) \
	-isysroot $(shell xcrun --sdk $(TARGET) --show-sdk-path) \
	-arch $(ARCH)

INCLUDES := $(INCLUDES) -Isrc/unix/apple -Isrc/unix/apple/$(TARGET)
endif
endif

CFLAGS = $(INCLUDES) $(DEFS) $(FLAGS)
OCFLAGS = $(CFLAGS) -fobjc-arc

all: clean-build clear $(SHADERS)
	make objs -j4

objs: $(OBJS)
	mkdir -p bin/$(TARGET)/$(ARCH)
	$(AR) -crs bin/$(TARGET)/$(ARCH)/$(NAME).a $(OBJS)

###############
### ANDROID ###
###############

# developer.android.com/ndk/downloads -> ~/android-ndk-xxx

ifndef ANDROID_NDK_ROOT
ANDROID_NDK_ROOT = $(HOME)/android-ndk-r25
endif

android: clean clear $(SHADERS)
	@$(ANDROID_NDK_ROOT)/ndk-build -j4 \
		APP_BUILD_SCRIPT=Android.mk \
		APP_PLATFORM=android-26 \
		NDK_PROJECT_PATH=. \
		--no-print-directory \
		| grep -v 'fcntl(): Operation not supported'

clean: clean-build
	@rm -rf obj
	@rm -f $(SHADERS)

clean-build:
	@rm -rf bin
	@rm -f $(OBJS)
	@rm -f $(NAME).so

clear:
	@clear
