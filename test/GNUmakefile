UNAME_S = $(shell uname -s)

OBJS = \
	main.o

BIN = \
	test

CFLAGS = \
	-I../src \
	-fPIC \
	-O3 \
	-Wall \
	-Wextra \
	-Wno-unused-parameter

ifeq ($(UNAME_S), Darwin)
LIBS = \
	../bin/macosx/x86_64/libmatoya.a \
	-framework Foundation \
	-framework Security \
	-framework AppKit

else
CFLAGS := $(CFLAGS) -Wno-format-overflow

LIBS = \
	../bin/linux/x86_64/libmatoya.a \
	-lpthread \
	-lm \
	-ldl
endif

all: clean clear $(OBJS)
	$(CC) -o $(BIN) $(OBJS) $(LIBS)
	@./test

clean:
	-rm -f $(BIN)
	-rm -f *.o

clear:
	clear
