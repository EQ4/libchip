#include "libchip.h"

/* Internal workings */
static ALLEGRO_EVENT_QUEUE *chip_queue;
static ALLEGRO_AUDIO_STREAM *chip_stream;
static ALLEGRO_MIXER *chip_mixer;
static ALLEGRO_VOICE *chip_voice;
static ALLEGRO_THREAD *chip_thread;

static uint16_t chip_rate;
static uint16_t chip_frag_size;
static uint16_t chip_frag_num;
static uint16_t chip_rate_mul;
static uint16_t chip_num_channels;

static void (*chip_engine_ptr)(void);
static uint32_t chip_engine_cnt;
static uint32_t chip_engine_period;

static chip_channel *chip_channels;

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

static void* chip_func(ALLEGRO_THREAD *thr, void *arg)
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

void chip_shutdown(void)
{	
	if (chip_thread)
	{
		al_set_thread_should_stop(chip_thread);
		al_destroy_thread(chip_thread);
		chip_thread = NULL;
	}
	if (chip_queue)
	{
		al_destroy_event_queue(chip_queue);
		chip_queue = NULL;
	}
	if (chip_stream)
	{
		al_destroy_audio_stream(chip_stream);
		chip_stream = NULL;
	}
	if (chip_voice)
	{
		al_destroy_voice(chip_voice);
		chip_voice = NULL;
	}
	if (chip_mixer)
	{
		al_destroy_mixer(chip_mixer);
		chip_mixer = NULL;
	}
	if (chip_channels)
	{
		for (uint16_t i = 0; i < chip_num_channels; i++)
		{
			chip_channel *ch = &chip_channels[i];
			al_unlock_mutex(ch->mutex);
			// Release waves if the channel owns it
			if (ch->own_wave)
			{
				free(ch->wave_data);
				al_destroy_mutex(ch->mutex);
			}
		}
		free(chip_channels);
		chip_channels = NULL;
	}
}

void chip_init(uint16_t rate, uint16_t num_channels, uint16_t frag_size, uint16_t frag_num, uint16_t rate_mul)
{
	chip_shutdown();

	if (!al_is_system_installed())
	{
		if (!al_init())
		{
			fprintf(stderr,"[audio] Error: Could not initialize Allegro.\n");
			return;
		}
	}
	printf("[audio] Allegro is installed\n");
	if (!al_is_audio_installed())
	{
	
		if (!al_install_audio())
		{
			fprintf(stderr,"[audio] Error: Could not install audio addon.\n");
			return;
		}
	}
	printf("[audio] Audio addon is installed\n");

	chip_rate = rate;
	chip_num_channels = num_channels;
	chip_frag_size = frag_size;
	chip_frag_num = frag_num;
	chip_rate_mul = rate_mul;

	if (!chip_rate)
	{
		fprintf(stderr,"[audio] Error: Invalid sample rate specified.\n");
		return;
	}
	printf("[audio] Sampling rate: %dHz\n",chip_rate);
	if (!chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: At least one channel must be created.\n");
		return;
	}
	printf("[audio] Using %d channels\n",chip_num_channels);
	if (!chip_frag_size)
	{
		fprintf(stderr,"[audio] Warning: No fragment size given. Defaulting to 2048.\n");
		chip_frag_size = chip_SIZE_FRAGMENT;
	}
	printf("[audio] Using %d for fragment size\n",chip_frag_size);
	if (!chip_frag_num)
	{
		fprintf(stderr,"[audio] Warning: No fragment number given. Defaulting to 2.\n");
		chip_frag_num = chip_NUM_FRAGMENTS;
	}
	printf("[audio] Using %d fragments\n",chip_frag_num);
	if (!chip_rate_mul)
	{
		chip_rate_mul = 1;
	}
	printf("[audio] Rate multiplier is %d\n",chip_rate_mul);
	// Voice
	chip_voice = al_create_voice(chip_rate,
		chip_DEPTH,
		chip_CHAN);
	if (!chip_voice)
	{
		fprintf(stderr,"[audio] Error: Failed to create voice.\n");
		return;
	}
	printf("[audio] Created voice at %X\n",(uint16_t)chip_voice);

	// Mixer
	chip_mixer = al_create_mixer(chip_rate,
		chip_DEPTH,
		chip_CHAN);
	if (!chip_mixer)
	{
		fprintf(stderr,"[audio] Error: Failed to create mixer.\n");
		return;
	}
	printf("[audio] Created mixer at %X\n",(uint16_t)chip_mixer);

	if (!al_attach_mixer_to_voice(chip_mixer, chip_voice))
	{
		fprintf(stderr,"[audio] Error: Failed to attach mixer to voice.\n");
		return;
	}
	printf("[audio] Attached mixer to voice\n");

	al_set_default_mixer(chip_mixer);
	al_reserve_samples(chip_frag_num);

	// Build stream
	chip_stream = al_create_audio_stream(
		chip_frag_num,
		chip_frag_size,
		chip_rate,
		chip_DEPTH,
		chip_CHAN);
	printf("[audio] Created stream at %X\n",(uint16_t)chip_stream);
	if (!al_attach_audio_stream_to_mixer(chip_stream, al_get_default_mixer()))
	{
		printf("[audio] Error: Couldn't attach stream to mixer.\n");
		return;
	}
	printf("[audio] Attached stream to mixer.\n");

	// Set up event source for the audio thread
	chip_queue = al_create_event_queue();
	printf("[audio] Created queue at %X\n",(uint16_t)chip_queue);
	al_register_event_source(chip_queue, 
		al_get_audio_stream_event_source(chip_stream));
	printf("[audio] Registered audio event source with queue.\n");

	// Set up channel state
	chip_channels = (chip_channel *)calloc(chip_num_channels,sizeof(chip_channel));
	for (int i = 0; i < num_channels; i++)
	{
		chip_channel *ch = &chip_channels[i];
		ch->period = 1;
		ch->mutex = al_create_mutex();
		ch->wave_data = (uint16_t *)malloc(sizeof(uint16_t));
		ch->wave_len = 1;
		ch->own_wave = 1;
		ch->noise_tap = 7;
		ch->noise_state = 0x0001;
		al_unlock_mutex(ch->mutex);
	}
	printf("[audio] Created channel states at %X\n",(uint16_t)chip_channels);

	// Set up defaults for audio engine pointer
	chip_engine_ptr = NULL;
	chip_engine_cnt = 0;
	chip_engine_period = (uint32_t)(chip_rate / 60.00); // Default to 60Hz

	// Build the thread
	chip_thread = al_create_thread(chip_func, NULL);
	printf("[audio] Created audio thread.\n");
}

void chip_start(void)
{
	al_start_thread(chip_thread);
	printf("[audio] Started audio thread.\n");
}

void chip_set_engine_ptr(void *ptr, uint32_t p)
{
	chip_engine_ptr = ptr;
	if (p)
	{
		chip_engine_period = p;
	}
}

void *chip_get_engine_ptr(void)
{
	return chip_engine_ptr;
}

/* External control fuctions */
void chip_set_freq(uint16_t channel, float f)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
// Resulting frequency: (rate_mul * rate) / (wave_len * period)
	uint32_t set_p = (uint32_t)((chip_rate_mul * chip_rate) / (ch->wave_len * f));
	if (set_p < 1)
	{
		set_p = 1;
	}
	ch->period = set_p;
	printf("[audio] Set channel %d period to %d\n",channel,set_p);
}

void chip_set_period_direct(uint16_t channel, uint32_t period)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	if (period < 1)
	{
		period = 1;
	}
	ch->period = period;
}

void chip_set_amp(uint16_t channel, uint16_t amp_l, uint16_t amp_r)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	ch->amplitude[0] = amp_l;
	ch->amplitude[1] = amp_r;
}

void chip_set_noise(uint16_t channel, uint16_t noise_en)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	ch->noise_en = noise_en;
}

void chip_set_loop(uint16_t channel, uint16_t loop_en)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	ch->loop_en = loop_en;
}

// Point to user-owned wave data
void chip_set_wave(uint16_t channel, uint16_t *wave_data, uint16_t len, uint16_t loop_en)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	al_lock_mutex(ch->mutex);
	// Clear out the previous wave if we own it
	if (ch->own_wave && ch->wave_data)
	{
		free(ch->wave_data);
		ch->wave_data = NULL;
	}
	ch->wave_data = wave_data;
	ch->wave_len = len;
	ch->loop_en = loop_en;
	ch->own_wave = 0;
	al_unlock_mutex(ch->mutex);
}

// Create a buffer for wave data owned by the library
void chip_create_wave(uint16_t channel, uint16_t len, uint16_t loop_en)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	al_lock_mutex(ch->mutex);
	if (!len)
	{
		fprintf(stderr,"[audio] Error: Wave length of 0 specified. The engine may crash.\n");
		return;
	}
	// Clear out the previous wave if we own it
	if (ch->own_wave && ch->wave_data)
	{
		free(ch->wave_data);
		ch->wave_data = NULL;
	}
	ch->wave_data = (uint16_t *)calloc(len,sizeof(uint16_t));
	ch->own_wave = 1;
	ch->wave_len = len;
	ch->loop_en = loop_en;
	al_unlock_mutex(ch->mutex);
}

void chip_set_wave_pos(uint16_t channel, uint16_t pos)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	ch->wave_pos = pos;
}

void chip_set_noise_tap(uint16_t channel, uint16_t tap)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return;
	}
	chip_channel *ch = &chip_channels[channel];
	if (tap > 15)
	{
		tap = 0;
	}
	ch->noise_tap = tap;
}

uint16_t chip_get_period(uint32_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->period;
}
uint16_t chip_get_amp(uint16_t channel, uint16_t side)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->amplitude[side % 2];
}

uint16_t chip_get_noise(uint16_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->noise_en;
}

uint16_t chip_get_loop(uint16_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->loop_en;
}

uint16_t *chip_get_wave(uint16_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return NULL;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->wave_data;
}

uint16_t chip_get_wave_len(uint16_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->wave_len;
}

chip_channel *chip_get_channel(uint16_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	return &chip_channels[channel];
}

uint16_t chip_get_wave_pos(uint16_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->wave_pos;
}

uint16_t chip_get_noise_tap(uint16_t channel)
{
	if (channel >= chip_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,chip_num_channels);
		return 0;
	}
	chip_channel *ch = &chip_channels[channel];
	return ch->noise_tap;
	
}
