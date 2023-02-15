ARCH = %%Platform%%

BIN = \
	mty.exe

CFLAGS =\
	/I..\src \
	/MT \
	/O2 \
	/W4 \
	/wd4201 \
	/wd4100 \
	/volatile:iso \
	/nologo

LIBS = \
	..\bin\windows\$(ARCH)\matoya.lib \
	advapi32.lib \
	bcrypt.lib \
	d3d9.lib \
	d3d11.lib \
	d3d12.lib \
	dxgi.lib \
	dxguid.lib \
	gdi32.lib \
	hid.lib \
	imm32.lib \
	ole32.lib \
	opengl32.lib \
	shcore.lib \
	shell32.lib \
	shlwapi.lib \
	user32.lib \
	userenv.lib \
	windowscodecs.lib \
	winhttp.lib \
	winmm.lib \
	ws2_32.lib \
	xinput.lib

test: clean clear
	cl $(CFLAGS) /Fe:$(BIN) src\$@.c $(LIBS)
	@mty

0-minimal: clean clear
	cl $(CFLAGS) /Fe:$(BIN) src\$@.c $(LIBS)
	@mty

1-draw: clean clear
	cl $(CFLAGS) /Fe:$(BIN) src\$@.c $(LIBS)
	@mty

2-threaded: clean clear
	cl $(CFLAGS) /Fe:$(BIN) src\$@.c $(LIBS)
	@mty

clean:
	@-del /q $(BIN) 2>nul
	@-del /q *.obj 2>nul
	@-rmdir /s /q test_dir 2>nul

clear:
	cls
