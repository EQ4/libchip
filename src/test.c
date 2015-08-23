#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

int main(int argc, char **argv)
{
	audio_init(44100,1,1024,2,1);
	printf("[main] Waiting for stop signal.\n");
	char c = 0;
	while(c != 'z')
	{
		c = getchar();
		printf("%c",c);
	}
	audio_shutdown();
	return 0;
}
