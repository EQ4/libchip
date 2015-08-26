# libchip test programs
# Copyright (C) Michael Moffitt 2015
# Depends on Allegro 5 static - of course you can link it dynamically, but this
# makefile expects a static library to be built. 

# C compiler configuration
CC := clang
CFLAGS := -std=c99 -O2 -g -Wall
LIBS := -lchip `pkg-config --cflags --libs --static allegro-static-5 allegro_audio-static-5`
EXEC := wavetest

.PHONY: all clean

all: wavetest

clean:
	rm wavetest wavetest.o

wavetest: wavetest.c
	$(CC) $(CFLAGS) wavetest.c -o $(EXEC) $(LIBS)
