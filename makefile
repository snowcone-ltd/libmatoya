TARGET = windows
ARCH = %%Platform%%
NAME = matoya

.SUFFIXES : .ps .vs .fragvk .vertvk

.vs.h:
	@fxc $(FXCFLAGS) /Fh $@ /T vs_4_0 /Vn $(*B) $<

.ps.h:
	@fxc $(FXCFLAGS) /Fh $@ /T ps_4_0 /Vn $(*B) $<

.vertvk.h:
	@deps\bin\glslangValidator -S vert -V --vn VERT $< -o $@

.fragvk.h:
	@deps\bin\glslangValidator -S frag -V --vn FRAG $< -o $@

!IFDEF STEAM
WEBVIEW_OBJ = src\swebview.obj
!ELSE
WEBVIEW_OBJ = src\windows\webview.obj
!ENDIF

OBJS = \
	src\app.obj \
	src\async.obj \
	src\crypto.obj \
	src\dtls.obj \
	src\file.obj \
	src\hash.obj \
	src\http.obj \
	src\image.obj \
	src\json.obj \
	src\list.obj \
	src\log.obj \
	src\memory.obj \
	src\queue.obj \
	src\resample.obj \
	src\system.obj \
	src\thread.obj \
	src\tlocal.obj \
	src\version.obj \
	src\gfx\vk\vk.obj \
	src\gfx\vk\vk-ctx.obj \
	src\gfx\vk\vk-ui.obj \
	src\hid\hid.obj \
	src\hid\utils.obj \
	src\windows\aes-gcm.obj \
	src\windows\appw.obj \
	src\windows\audio.obj \
	src\windows\cryptow.obj \
	src\windows\dialog.obj \
	src\windows\dtlsw.obj \
	src\windows\filew.obj \
	src\windows\hidw.obj \
	src\windows\imagew.obj \
	src\windows\memoryw.obj \
	src\windows\request.obj \
	src\windows\systemw.obj \
	src\windows\threadw.obj \
	src\windows\time.obj \
	src\windows\ws.obj \
	src\windows\gfx\d3d12.obj \
	src\windows\gfx\d3d12-ctx.obj \
	src\windows\gfx\d3d12-ui.obj \
	src\windows\gfx\d3d11.obj \
	src\windows\gfx\d3d11-ctx.obj \
	src\windows\gfx\d3d11-ui.obj \
	$(WEBVIEW_OBJ)

SHADERS = \
	src\gfx\vk\shaders\fs.h \
	src\gfx\vk\shaders\vs.h \
	src\gfx\vk\shaders\fsui.h \
	src\gfx\vk\shaders\vsui.h \
	src\windows\gfx\shaders\ps.h \
	src\windows\gfx\shaders\vs.h \
	src\windows\gfx\shaders\psui.h \
	src\windows\gfx\shaders\vsui.h

INCLUDES = \
	-Ideps \
	-Isrc \
	-Isrc\windows

DEFS = \
	-DUNICODE \
	-DWIN32_LEAN_AND_MEAN \
	-DMTY_VK_WIN32

FXCFLAGS = \
	/O3 \
	/Ges \
	/nologo

FLAGS = \
	/W4 \
	/MT \
	/MP \
	/volatile:iso \
	/wd4100 \
	/wd4152 \
	/wd4201 \
	/nologo

LIB_FLAGS = \
	/nologo

!IFDEF DEBUG
FLAGS = $(FLAGS) /Ob0 /Zi /Oy-
DEFS = $(DEFS) -DMTY_VK_DEBUG
!ELSE
FLAGS = $(FLAGS) /O2 /GS- /Gw
!ENDIF

CFLAGS = $(INCLUDES) $(DEFS) $(FLAGS)

all: clean-build clear $(SHADERS) $(OBJS)
	mkdir bin\$(TARGET)\$(ARCH)
	lib /out:bin\$(TARGET)\$(ARCH)\$(NAME).lib $(LIB_FLAGS) *.obj

clean: clean-build
	@-del /q $(SHADERS) 2>nul

clean-build:
	@-rmdir /s /q bin 2>nul
	@-del /q *.obj 2>nul
	@-del /q *.lib 2>nul
	@-del /q *.pdb 2>nul

clear:
	@cls
