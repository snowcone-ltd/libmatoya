TARGET = windows
ARCH = %%Platform%%
NAME = matoya

.SUFFIXES : .ps4 .vs4 .ps3 .vs3 .vert .frag .vert .fragvk .vertvk

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

.vertvk.h:
	@deps\bin\glslangValidator -S vert -V --vn VERT $< -o $@

.fragvk.h:
	@deps\bin\glslangValidator -S frag -V --vn FRAG $< -o $@

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
	src\resample.obj \
	src\system.obj \
	src\thread.obj \
	src\tlocal.obj \
	src\tls.obj \
	src\version.obj \
	src\gfx\gl\gl.obj \
	src\gfx\gl\gl-ui.obj \
	src\gfx\vk\vk.obj \
	src\gfx\vk\vk-ctx.obj \
	src\gfx\vk\vkproc.obj \
	src\gfx\vk\vk-ui.obj \
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
	src\windows\wintab.obj \
	src\windows\gfx\d3d12.obj \
	src\windows\gfx\d3d12-ctx.obj \
	src\windows\gfx\d3d12-ui.obj \
	src\windows\gfx\d3d11.obj \
	src\windows\gfx\d3d11-ctx.obj \
	src\windows\gfx\d3d11-ui.obj \
	src\windows\gfx\d3d9.obj \
	src\windows\gfx\d3d9-ctx.obj \
	src\windows\gfx\d3d9-ui.obj \
	src\windows\gfx\gl-ctx.obj \


SHADERS = \
	src\gfx\gl\shaders\fs.h \
	src\gfx\gl\shaders\vs.h \
	src\gfx\gl\shaders\fsui.h \
	src\gfx\gl\shaders\vsui.h \
	src\gfx\vk\shaders\fs.h \
	src\gfx\vk\shaders\vs.h \
	src\gfx\vk\shaders\fsui.h \
	src\gfx\vk\shaders\vsui.h \
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

!IFDEF MTY_NO_WINHTTP
OBJS = $(OBJS) src\unix\net\request.obj
!ELSE
OBJS = $(OBJS) src\windows\net\request.obj
!ENDIF

!IFDEF DEBUG
FLAGS = $(FLAGS) /Ob0 /Zi
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
