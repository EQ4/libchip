#include "chipkernel.h"

/* Internal workings */
ALLEGRO_EVENT_QUEUE *chip_queue;
ALLEGRO_AUDIO_STREAM *chip_stream;
ALLEGRO_MIXER *chip_mixer;
ALLEGRO_VOICE *chip_voice;
ALLEGRO_THREAD *chip_thread;

uint16_t chip_rate;
uint16_t chip_frag_size;
uint16_t chip_frag_num;
uint16_t chip_rate_mul;
uint16_t chip_num_channels;

int chip_is_init;

void (*chip_engine_ptr)(void);
uint32_t chip_engine_cnt;
uint32_t chip_engine_period;

chip_channel *chip_channels;

void chip_noise_step(chip_channel *ch)
{
	uint16_t feedback = (ch->noise_state & 0x0001) ^ ((ch->noise_state & (1 << ch->noise_tap)) ? 1 : 0);
	ch->noise_state = (feedback << 14) | (ch->noise_state >> 1);
}

void chip_channel_prog(chip_channel *ch)
{
	// Period met, increment wave pointer
	if (ch->counter == 0)
	{
		// Reset period counter
		ch->counter = ch->period - 1;

		if (ch->noise_en)
		{
			chip_noise_step(ch);
		}

		// Loop the wave if necessary
		if (ch->wave_pos >= ch->wave_len - 1)
		{
			if (ch->loop_en)
			{
				ch->wave_pos = 0;
			}
		}
		else
		{
			ch->wave_pos++;
		}
	}
	else
	{
		ch->counter--;
	}
}

// Represents creating one (1 / chip_rate) of a second of audio
void chip_step(int16_t *frame)
{
	memset(frame, 0, sizeof(int16_t) * 2);
	// If there's an attached sound engine, call its function
	if (chip_engine_ptr)
	{
		if (chip_engine_cnt == 0)
		{
			chip_engine_cnt = chip_engine_period - 1;
			chip_engine_ptr();
		}
		else
		{
			chip_engine_cnt--;
		}
	}

	for (uint16_t i = 0; i < chip_num_channels; i++)
	{
		chip_channel *ch = &chip_channels[i];
		al_lock_mutex(ch->mutex);
		int16_t frame_add[2];
		frame_add[0] = 0;
		frame_add[1] = 0;

		// Rate multiplier is for oversampling and averaging
		for (uint16_t k = 0; k < chip_rate_mul; k++)
		{
			chip_channel_prog(ch);
			if (ch->noise_en)
			{
				frame_add[0] += 0xF * (ch->noise_state & 0x0001);
			}
			else
			{
				frame_add[0] += ch->wave_data[ch->wave_pos];
			}
		}
		frame_add[1] = frame_add[0];
		for (int k = 0; k < 2; k++)
		{
			// Now we have 0-16
			frame_add[k] /= chip_rate_mul;
			// Scale the nybble up to an 8-bit value
			frame_add[k] *= ch->amplitude[k];
			// Center the wave at 0
			frame_add[k] -= (0xF * ch->amplitude[k])/2;

			frame_add[k] += (frame_add[k] + (frame_add[k] * 0x11F)); // Bring it to 16
			frame_add[k] /= chip_num_channels;
			frame[k] += (int16_t)frame_add[k];
		}
		al_unlock_mutex(ch->mutex);
	}
}

void* chip_func(ALLEGRO_THREAD *thr, void *arg)
{
	int16_t *frame;
	while (!al_get_thread_should_stop(thr))
	{
		ALLEGRO_TIMEOUT ev_timeout;
		ALLEGRO_EVENT event;
		al_init_timeout(&ev_timeout, 1.0);
		int got_ev = al_wait_for_event_until(chip_queue, &event, &ev_timeout);
		if (got_ev)
		{
			switch (event.type)
			{
				case ALLEGRO_EVENT_AUDIO_STREAM_FRAGMENT:
					frame = (int16_t *)al_get_audio_stream_fragment(chip_stream);
					if (frame)
					{
						for (int i = 0; i < chip_frag_size; i++)
						{
							chip_step(frame + (2*i));
						}
					}
					al_set_audio_stream_fragment(chip_stream, (void *)frame);
					break;
					
				case ALLEGRO_EVENT_AUDIO_STREAM_FINISHED:
					printf("[audio] Stream has finished.\n");	
					al_drain_audio_stream(chip_stream);
					break;
			}
		}
	}
	printf("[audio] Thread received signal to stop.\n");
	return NULL;
}

