#ifndef CHIP_H
#define CHIP_H

#include <stdio.h>
#include <stdlib.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>

#define CHIP_NUM_FRAGMENTS 4
#define CHIP_SIZE_FRAGMENT 1024
#define CHIP_RATE 44100
#define CHIP_DEPTH ALLEGRO_AUDIO_DEPTH_INT16
#define CHIP_CHAN ALLEGRO_CHANNEL_CONF_2

typedef struct chip_channel chip_channel;
struct chip_channel
{
	ALLEGRO_MUTEX *mutex; // Lock for reading/writing wave data
	uint16_t *wave_data; // Pointer to wave nybbles array
	uint32_t period; // Division of sample rate / rate multiplier.
	uint32_t counter; // Countdown until wave pos increment
	unsigned int amplitude[2]; // Left and right amplitude;
	unsigned int own_wave; // Mark if it should be destroyed
	unsigned int wave_len; // Number of samples in the wave
	unsigned int wave_pos; // Pointer within wave
	unsigned int loop_en; // Will the wave loop at the end or stop playing?
	unsigned int noise_en; // When nonzero, make LSFR noise like NES APU
	unsigned int noise_state;
	unsigned int noise_tap;
};

void chip_shutdown(void);
void chip_init(unsigned int rate, unsigned int num_channels, unsigned int frag_size, unsigned int frag_num, unsigned int rate_mul);
void chip_start(void);

void chip_set_engine_ptr(void *ptr, uint32_t p);
void *chip_get_engine_ptr(void);

void chip_set_freq(unsigned int channel, float f);
void chip_set_period_direct(unsigned int channel, uint32_t period);
void chip_set_amp(unsigned int channel, unsigned int amp_l, unsigned int amp_r);
void chip_set_noise(unsigned int channel, unsigned int noise_en);
void chip_set_loop(unsigned int channel, unsigned int loop_en);
void chip_set_wave(unsigned int channel, uint16_t *wave_data, unsigned int len, unsigned int loop_en);
void chip_create_wave(unsigned int channel, unsigned int len, unsigned int loop_en);
void chip_set_wave_pos(unsigned int channel, unsigned int pos);
void chip_set_noise_tap(unsigned int channel, unsigned int tap);

unsigned int chip_get_period(uint32_t channel);
unsigned int chip_get_amp(unsigned int channel, unsigned int side);
unsigned int chip_get_noise(unsigned int channel);
unsigned int chip_get_loop(unsigned int channel);
uint16_t *chip_get_wave(unsigned int channel);
unsigned int chip_get_wave_len(unsigned int channel);
chip_channel *chip_get_channel(unsigned int channel);
unsigned int chip_get_wave_pos(unsigned int channel);
unsigned int chip_get_noise_tap(unsigned int channel);

#endif
