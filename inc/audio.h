#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>

#define AUDIO_NUM_FRAGMENTS 2
#define AUDIO_SIZE_FRAGMENT 2048
#define AUDIO_RATE 44100
#define AUDIO_DEPTH ALLEGRO_AUDIO_DEPTH_INT16
#define AUDIO_CHAN ALLEGRO_CHANNEL_CONF_2

typedef struct audio_channel audio_channel;
struct audio_channel
{
	ALLEGRO_MUTEX *mutex; // Lock for reading/writing wave data
	uint16_t period; // Division of sample rate / rate multiplier.
	uint16_t counter; // Countdown until wave pos increment
	uint16_t amplitude[2]; // Left and right amplitude;
	uint16_t *wave_data; // Pointer to wave nybbles array
	uint16_t own_wave; // Mark if it should be destroyed
	uint16_t wave_len; // Number of samples in the wave
	uint16_t wave_pos; // Pointer within wave
	uint16_t loop_en; // Will the wave loop at the end or stop playing?
	uint16_t noise_en; // When nonzero, make LSFR noise like NES APU
};

void audio_shutdown(void);
void audio_init(uint16_t rate, uint16_t num_channels, uint16_t frag_size, uint16_t frag_num, uint16_t rate_mul);
void audio_start(void);

void audio_set_freq(uint16_t channel, float f);
void audio_set_period_direct(uint16_t channel, uint16_t period);
void audio_set_amp(uint16_t channel, uint16_t amp_l, uint16_t amp_r);
void audio_set_noise(uint16_t channel, uint16_t noise_en);
void audio_set_loop(uint16_t channel, uint16_t loop_en);
void audio_set_wave(uint16_t channel, uint16_t *wave_data, uint16_t len, uint16_t loop_en);
void audio_create_wave(uint16_t channel, uint16_t len, uint16_t loop_en);
void audio_set_wave_pos(uint16_t channel, uint16_t pos);

uint16_t audio_get_period(uint16_t channel);
uint16_t audio_get_amp(uint16_t channel, uint16_t side);
uint16_t audio_get_noise(uint16_t channel);
uint16_t audio_get_loop(uint16_t channel);
uint16_t *audio_get_wave(uint16_t channel);
uint16_t audio_get_wave_len(uint16_t channel);
audio_channel *audio_get_channel(uint16_t channel);
uint16_t audio_get_wave_pos(uint16_t channel);

#endif
