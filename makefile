# libchip
# Copyright (C) Michael Moffitt 2015

# C compiler configuration
CC := clang
CFLAGS := -std=c99 -O2 -g -Wall
INCLUDE := -Iinc 
LDFLAGS := 
# Archiver for static building
AR := ar
ARFLAGS := cvq

all: libchip.a libchip.o 

libchip.o: src/libchip.c
	$(CC) $(CFLAGS) $(INCLUDE) $(LDFLAGS) -c src/libchip.c -o libchip.a 

libchip.a: src/libchip.o 
	$(AR) $(ARFLAGS) libchip.a src/libchip.o
	rm src/libchip.o

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
	$(RM) src/libchip.o libchip.a
