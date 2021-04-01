First, [build `libmatoya`](https://github.com/matoya/libmatoya/wiki/Building). The following compiler commands assume that your working directory is this example directory, the `libmatoya` static library resides in the `../bin` directory, and you're on an `x86_64` platform.

Change the file name in the commands below to compile different examples.

#### Windows

`cl 0-minimal.c /Fe:example /I..\src ..\bin\windows\x64\matoya.lib bcrypt.lib d3d11.lib d3d9.lib hid.lib uuid.lib dxguid.lib opengl32.lib ws2_32.lib user32.lib gdi32.lib xinput9_1_0.lib ole32.lib shell32.lib windowscodecs.lib shlwapi.lib imm32.lib winmm.lib winhttp.lib secur32.lib crypt32.lib`

#### Linux

`cc 0-minimal.c -o example -I../src ../bin/linux/x86_64/libmatoya.a -lm -lpthread -ldl`

#### macOS

`cc 0-minimal.c -o example -I../src ../bin/macosx/x86_64/libmatoya.a -framework OpenGL -framework AppKit -framework Metal -framework IOKit -framework CoreVideo -framework Carbon -framework QuartzCore`
