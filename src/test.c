#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

int main(int argc, char **argv)
{
	audio_init(44100,6,2048,2,1);
	return 0;
}
