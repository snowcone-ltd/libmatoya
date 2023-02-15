UNAME_S = $(shell uname -s)
ARCH = $(shell uname -m)

BIN = \
	mty

CFLAGS = \
	-I../src \
	-O3 \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-format-overflow

ifeq ($(UNAME_S), Darwin)

LIBS = \
	../bin/macosx/$(ARCH)/libmatoya.a \
	-framework Foundation \
	-framework Security \
	-framework AppKit

else

LIBS = \
	../bin/linux/$(ARCH)/libmatoya.a \
	-lpthread \
	-lm \
	-ldl
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
