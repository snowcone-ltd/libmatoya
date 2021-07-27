TARGET = windows
ARCH = $(PROCESSOR_ARCHITECTURE)
NAME = matoya

.SUFFIXES : .ps4 .vs4 .ps3 .vs3 .vert .frag

.vs4.h:
	@fxc $(FXCFLAGS) /Fh $@ /T vs_4_0 /Vn $(*B) $<

.ps4.h:
	@fxc $(FXCFLAGS) /Fh $@ /T ps_4_0 /Vn $(*B) $<

.vs3.h:
	@fxc $(FXCFLAGS) /Fh $@ /T vs_3_0 /Vn $(*B) $<

.ps3.h:
	@fxc $(FXCFLAGS) /Fh $@ /T ps_3_0 /Vn $(*B) $<

.vert.h:
	@echo static const GLchar VERT[]={ > $@ && powershell "Format-Hex $< | %{$$_ -replace '^.*?   ',''} | %{$$_ -replace '  .*',' '} | %{$$_ -replace '(\w\w)','0x$$1,'}" >> $@ && echo 0x00}; >> $@

.frag.h:
	@echo static const GLchar FRAG[]={ > $@ && powershell "Format-Hex $< | %{$$_ -replace '^.*?   ',''} | %{$$_ -replace '  .*',' '} | %{$$_ -replace '(\w\w)','0x$$1,'}" >> $@ && echo 0x00}; >> $@

OBJS = \
	src\app.obj \
	src\crypto.obj \
	src\file.obj \
	src\hash.obj \
	src\image.obj \
	src\json.obj \
	src\list.obj \
	src\log.obj \
	src\memory.obj \
	src\queue.obj \
	src\render.obj \
	src\system.obj \
	src\thread.obj \
	src\tlocal.obj \
	src\tls.obj \
	src\version.obj \
	src\gfx\gl.obj \
	src\gfx\gl-ui.obj \
	src\hid\hid.obj \
	src\hid\utils.obj \
	src\net\async.obj \
	src\net\gzip.obj \
	src\net\http.obj \
	src\net\net.obj \
	src\net\secure.obj \
	src\net\tcp.obj \
	src\net\ws.obj \
	src\windows\aes-gcm.obj \
	src\windows\appw.obj \
	src\windows\audio.obj \
	src\windows\cryptow.obj \
	src\windows\dialog.obj \
	src\windows\filew.obj \
	src\windows\hidw.obj \
	src\windows\imagew.obj \
	src\windows\memoryw.obj \
	src\windows\systemw.obj \
	src\windows\threadw.obj \
	src\windows\time.obj \
	src\windows\tlsw.obj \
	src\windows\gfx\d3d11.obj \
	src\windows\gfx\d3d11-ctx.obj \
	src\windows\gfx\d3d11-ui.obj \
	src\windows\gfx\d3d9.obj \
	src\windows\gfx\d3d9-ctx.obj \
	src\windows\gfx\d3d9-ui.obj \
	src\windows\gfx\gl-ctx.obj


SHADERS = \
	src\gfx\shaders\gl\fs.h \
	src\gfx\shaders\gl\vs.h \
	src\gfx\shaders\gl\fsui.h \
	src\gfx\shaders\gl\vsui.h \
	src\windows\gfx\shaders\d3d11\ps.h \
	src\windows\gfx\shaders\d3d11\vs.h \
	src\windows\gfx\shaders\d3d11\psui.h \
	src\windows\gfx\shaders\d3d11\vsui.h \
	src\windows\gfx\shaders\d3d9\ps.h \
	src\windows\gfx\shaders\d3d9\vs.h

INCLUDES = \
	-Ideps \
	-Isrc \
	-Isrc\windows

DEFS = \
	-DUNICODE \
	-DWIN32_LEAN_AND_MEAN

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
	/wd4204 \
	/nologo

LIB_FLAGS = \
	/nologo

LIBS = \
	libvcruntime.lib \
	libucrt.lib \
	libcmt.lib \
	kernel32.lib \
	windowscodecs.lib \
	user32.lib \
	shell32.lib \
	d3d9.lib \
	d3d11.lib \
	dxguid.lib \
	ole32.lib \
	uuid.lib \
	winmm.lib \
	shcore.lib \
	bcrypt.lib \
	userenv.lib \
	shlwapi.lib \
	advapi32.lib \
	opengl32.lib \
	ws2_32.lib \
	xinput.lib \
	gdi32.lib \
	imm32.lib \
	winhttp.lib \
	crypt32.lib \
	secur32.lib \
	hid.lib

!IFDEF MTY_NO_WINHTTP
OBJS = $(OBJS) src\unix\net\request.obj
!ELSE
OBJS = $(OBJS) src\windows\net\request.obj
!ENDIF

!IFDEF DEBUG
FLAGS = $(FLAGS) /Ob0 /Zi
!ELSE
DEFS  = $(DEFS) -DMTY_EXPORT=__declspec(dllexport)
FLAGS = $(FLAGS) /O2 /GS- /Gw
!ENDIF

CFLAGS = $(INCLUDES) $(DEFS) $(FLAGS)

all: static

prepare: clean-build clear $(SHADERS) $(OBJS)
	mkdir bin\$(TARGET)\$(ARCH)

static: prepare
	lib /out:bin\$(TARGET)\$(ARCH)\$(NAME).lib $(LIB_FLAGS) *.obj

shared: prepare
	link /dll /out:bin\$(TARGET)\$(ARCH)\$(NAME).dll $(LIB_FLAGS) $(LIBS) *.obj

clean: clean-build
	@-del /q $(SHADERS) 2>nul

clean-build:
	@-rmdir /s /q bin 2>nul
	@-del /q *.obj 2>nul
	@-del /q *.lib 2>nul
	@-del /q *.pdb 2>nul

clear:
	@cls
