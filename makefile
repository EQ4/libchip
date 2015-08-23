CC := clang
# CC := gcc # for wingnuts
CFLAGS := -fvisibility=hidden -std=c99 -O2 -g
CPPFLAGS := -Iinc

CPPFLAGS := $(CPPFLAGS) -Wall

LDFLAGS :=
LIBRARIES := -lpoly `pkg-config --cflags --static --libs allegro-static-5 allegro_ttf-static-5 allegro_font-static-5 allegro_audio-static-5 allegro_color-static-5 allegro_image-static-5 allegro_main-static-5 allegro_primitives-static-5`

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)
EDITOR_EXEC := chiped

.PHONY: all clean

all: $(EDITOR_EXEC)

clean:
	$(RM) $(OBJECTS) $(EDITOR_EXEC)

$(EDITOR_EXEC): $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(OBJECTS) -o $@ $(LIBRARIES)

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
