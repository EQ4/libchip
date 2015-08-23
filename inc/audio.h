#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include "types.h"

#define AUDIO_NUM_FRAGMENTS 2
#define AUDIO_SIZE_FRAGMENT 2048
#define AUDIO_RATE 44100
#define AUDIO_DEPTH ALLEGRO_AUDIO_DEPTH_INT16
#define AUDIO_CHAN ALLEGRO_CHANNEL_CONF_2

typedef struct audio_channel audio_channel;
struct audio_channel
{
	uint period; // Division of sample rate / rate multiplier.
	uint amplitude[2]; // Left and right amplitude;
	uint *wave_data; // Pointer to wave nybbles array
	uint own_wave; // Mark if it should be destroyed
	uint wave_len; // Number of samples in the wave
	uint wave_pos; // Pointer within wave
	uint loop_en; // Will the wave loop at the end or stop playing?
	uint noise_en; // When nonzero, make LSFR noise like NES APU
};

void audio_init(uint rate, uint num_channels, uint frag_size, uint frag_num, uint rate_mul);
void audio_shutdown(void);

void audio_set_period(uint channel, uint period);
void audio_set_amp(uint channel, uint amp_l, uint amp_r);
void audio_set_noise(uint channel, uint noise_en);
void audio_set_loop(uint channel, uint loop_en);
void audio_set_wave(uint channel, uint *wave_data, uint len, uint loop_en);
void audio_create_wave(uint channel, uint len, uint loop_en);

uint audio_get_period(uint channel);
uint audio_get_amp(uint channel, uint side);
uint audio_get_noise(uint channel);
uint audio_get_loop(uint channel);
uint *audio_get_wave(uint channel);
audio_channel *audio_get_channel(uint channel);

#endif
