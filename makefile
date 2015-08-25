# libchip
# Copyright (C) Michael Moffitt 2015

# C compiler configuration
CC := clang
CFLAGS := -fvisibility=hidden -std=c99 -O2 -g -Wall
INCLUDE := -Iinc

# Archiver for static building
AR := ar
ARFLAGS := rvs

LDFLAGS :=
LIBRARIES := -lallegro -lallegro_audio-static

OUTPUT := libchip.a

.PHONY: all

all: $(OUTPUT)

.PHONY: shared
shared: libchip.o
	$(CC) -shared -o libchip.so libchip.o

libchip.a: libchip.o 
	$(AR) $(ARFLAGS) libchip.a libchip.o

libchip.o: src/libchip.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -fPIC src/libchip.c

.PHONY: install
install:
	cp libchip.a /usr/local/lib/
	chown root /usr/local/lib/libchip.a
	chmod 0775 /usr/local/lib/libchip.a
	cp inc/libchip.h /usr/local/include/
	chown root /usr/local/include/libchip.h
	chmod 0775 /usr/local/include/libchip.h

.PHONY: clean
clean:
	$(RM) $(OBJECTS) $(OUTPUT)
