UNAME_S = $(shell uname -s)
ARCH = $(shell uname -m)

BIN = \
	mty

CFLAGS = \
	-I../src \
	-O3 \
	-Wall \
	-Wextra \
	-Wno-unused-parameter

ifeq ($(UNAME_S), Darwin)

LIBS = \
	../bin/macosx/$(ARCH)/libmatoya.a \
	-framework AppKit \
	-framework Carbon \
	-framework IOKit \
	-framework Metal \
	-framework QuartzCore \
	-framework WebKit

else

CFLAGS := $(CFLAGS) -Wno-format-overflow

LIBS = \
	../bin/linux/$(ARCH)/libmatoya.a \
	-lc \
	-lm
endif

test: clean clear
	$(CC) $(CFLAGS) -o $(BIN) src/$@.c $(LIBS)
	@./mty

0-minimal: clean clear
	$(CC) $(CFLAGS) -o $(BIN) src/$@.c $(LIBS)
	@./mty

1-draw: clean clear
	$(CC) $(CFLAGS) -o $(BIN) src/$@.c $(LIBS)
	@./mty

2-threaded: clean clear
	$(CC) $(CFLAGS) -o $(BIN) src/$@.c $(LIBS)
	@./mty

clean:
	@rm -f $(BIN)
	@rm -rf test_dir

clear:
	clear
