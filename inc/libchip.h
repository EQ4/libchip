#ifndef chip_H
#define chip_H

#include <stdio.h>
#include <stdlib.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>

#define chip_NUM_FRAGMENTS 2
#define chip_SIZE_FRAGMENT 2048
#define chip_RATE 44100
#define chip_DEPTH ALLEGRO_AUDIO_DEPTH_INT16
#define chip_CHAN ALLEGRO_CHANNEL_CONF_2

typedef struct chip_channel chip_channel;
struct chip_channel
{
	ALLEGRO_MUTEX *mutex; // Lock for reading/writing wave data
	uint32_t period; // Division of sample rate / rate multiplier.
	uint32_t counter; // Countdown until wave pos increment
	uint16_t amplitude[2]; // Left and right amplitude;
	uint16_t *wave_data; // Pointer to wave nybbles array
	uint16_t own_wave; // Mark if it should be destroyed
	uint16_t wave_len; // Number of samples in the wave
	uint16_t wave_pos; // Pointer within wave
	uint16_t loop_en; // Will the wave loop at the end or stop playing?
	uint16_t noise_en; // When nonzero, make LSFR noise like NES APU
	uint16_t noise_state;
	uint16_t noise_tap;
};

void chip_shutdown(void);
void chip_init(uint16_t rate, uint16_t num_channels, uint16_t frag_size, uint16_t frag_num, uint16_t rate_mul);
void chip_start(void);

void chip_set_engine_ptr(void *ptr, uint32_t p);
void *chip_get_engine_ptr(void);

void chip_set_freq(uint16_t channel, float f);
void chip_set_period_direct(uint16_t channel, uint32_t period);
void chip_set_amp(uint16_t channel, uint16_t amp_l, uint16_t amp_r);
void chip_set_noise(uint16_t channel, uint16_t noise_en);
void chip_set_loop(uint16_t channel, uint16_t loop_en);
void chip_set_wave(uint16_t channel, uint16_t *wave_data, uint16_t len, uint16_t loop_en);
void chip_create_wave(uint16_t channel, uint16_t len, uint16_t loop_en);
void chip_set_wave_pos(uint16_t channel, uint16_t pos);
void chip_set_noise_tap(uint16_t channel, uint16_t tap);

uint16_t chip_get_period(uint32_t channel);
uint16_t chip_get_amp(uint16_t channel, uint16_t side);
uint16_t chip_get_noise(uint16_t channel);
uint16_t chip_get_loop(uint16_t channel);
uint16_t *chip_get_wave(uint16_t channel);
uint16_t chip_get_wave_len(uint16_t channel);
chip_channel *chip_get_channel(uint16_t channel);
uint16_t chip_get_wave_pos(uint16_t channel);
uint16_t chip_get_noise_tap(uint16_t channel);

#endif
