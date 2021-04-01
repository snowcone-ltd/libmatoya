OBJS = \
	main.obj

BIN = \
	test.exe

CFLAGS =\
	/I..\src \
	/W4 \
	/O2 \
	/MT \
	/MP \
	/GS- \
	/wd4201 \
	/wd4100 \
	/volatile:iso \
	/nologo

LIBS = \
	..\bin\windows\x64\matoya.lib \
	bcrypt.lib \
	d3d11.lib \
	d3d9.lib \
	hid.lib \
	uuid.lib \
	dxguid.lib \
	opengl32.lib \
	ws2_32.lib \
	user32.lib \
	gdi32.lib \
	xinput9_1_0.lib \
	ole32.lib \
	shell32.lib \
	windowscodecs.lib \
	shlwapi.lib \
	imm32.lib \
	winmm.lib \
	advapi32.lib \
	userenv.lib \
	Crypt32.lib \
	Secur32.lib \
	Winhttp.lib

all: clean clear $(OBJS)
	link /nologo /out:$(BIN) $(OBJS) $(LIBS)
	@test

clean:
	-del $(BIN)
	-del *.obj

clear:
	cls
