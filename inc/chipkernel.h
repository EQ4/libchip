#ifndef CHIPKERNEL_H
#define CHIPKERNEL_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include "libchip.h"
#include <stdlib.h>
/* Internal workings */
extern ALLEGRO_EVENT_QUEUE *chip_queue;
extern ALLEGRO_AUDIO_STREAM *chip_stream;
extern ALLEGRO_MIXER *chip_mixer;
extern ALLEGRO_VOICE *chip_voice;
extern ALLEGRO_THREAD *chip_thread;

extern unsigned int chip_rate;
extern unsigned int chip_frag_size;
extern unsigned int chip_frag_num;
extern unsigned int chip_rate_mul;
extern unsigned int chip_num_channels;

extern int chip_is_init;

// Libchip state
extern void (*chip_engine_ptr)(void);
extern unsigned int chip_engine_cnt;
extern unsigned int chip_engine_period;
extern chip_channel *chip_channels;

void chip_noise_step(chip_channel *ch);
void chip_channel_prog(chip_channel *ch);
void chip_step(int16_t *frame);
void *chip_func(ALLEGRO_THREAD *thr, void *arg);

#endif
