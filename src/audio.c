#include "audio.h"

/* Internal workings */
static ALLEGRO_EVENT_QUEUE *audio_queue;
static ALLEGRO_AUDIO_STREAM *audio_stream;
static ALLEGRO_MIXER *audio_mixer;
static ALLEGRO_VOICE *audio_voice;
static ALLEGRO_THREAD *audio_thread;

static uint16_t audio_rate;
static uint16_t audio_frag_size;
static uint16_t audio_frag_num;
static uint16_t audio_rate_mul;
static uint16_t audio_num_channels;

static void (*audio_engine_ptr)(void);
static uint32_t audio_engine_cnt;
static uint32_t audio_engine_period;

static audio_channel *audio_channels;

void audio_channel_prog(audio_channel *ch)
{
	// Period met, increment wave pointer
	if (ch->counter == 0)
	{
		// Reset period counter
		ch->counter = ch->period - 1;

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

// Represents creating one (1 / audio_rate) of a second of audio
void audio_step(int16_t *frame)
{

	memset(frame, 0, sizeof(int16_t) * 2);
	// If there's an attached sound engine, call its function
	if (audio_engine_ptr)
	{
		if (audio_engine_cnt == 0)
		{
			audio_engine_cnt = audio_engine_period - 1;
			audio_engine_ptr();
		}
		else
		{
			audio_engine_cnt--;
		}
	}

	for (uint16_t i = 0; i < audio_num_channels; i++)
	{
		audio_channel *ch = &audio_channels[i];
		al_lock_mutex(ch->mutex);
		for (uint16_t chan = 0; chan < 2; chan++)
		{
			int16_t frame_add = 0;

			// Rate multiplier is for oversampling and averaging
			for (uint16_t k = 0; k < audio_rate_mul; k++)
			{
				audio_channel_prog(ch);
				frame_add += ch->wave_data[ch->wave_pos];
			}
			// Now we have 0-16
			frame_add /= audio_rate_mul;
			// Scale the nybble up to an 8-bit value
			frame_add *= ch->amplitude[chan];
				
			// Center the wave at 0
			frame_add -= (0xF * ch->amplitude[chan])/2;

			frame_add += (frame_add + (frame_add * 0x11F)); // Bring it to 16
			frame_add /= audio_num_channels;
			frame[chan] += (int16_t)frame_add;
		}		
		al_unlock_mutex(ch->mutex);
	}
}

static void* audio_func(ALLEGRO_THREAD *thr, void *arg)
{
	int16_t *frame;
	while (!al_get_thread_should_stop(thr))
	{
		ALLEGRO_TIMEOUT ev_timeout;
		ALLEGRO_EVENT event;
		al_init_timeout(&ev_timeout, 1.0);
		int got_ev = al_wait_for_event_until(audio_queue, &event, &ev_timeout);
		if (got_ev)
		{
			switch (event.type)
			{
				case ALLEGRO_EVENT_AUDIO_STREAM_FRAGMENT:
					frame = (int16_t *)al_get_audio_stream_fragment(audio_stream);
					if (frame)
					{
						for (int i = 0; i < audio_frag_size; i++)
						{
							audio_step(frame + (2*i));
						}
					}
					al_set_audio_stream_fragment(audio_stream, (void *)frame);
					break;
					
				case ALLEGRO_EVENT_AUDIO_STREAM_FINISHED:
					printf("[audio] Stream has finished.\n");	
					al_drain_audio_stream(audio_stream);
					break;
			}
		}
	}
	printf("[audio] Thread received signal to stop.\n");
	return NULL;
}

void audio_shutdown(void)
{	
	if (audio_thread)
	{
		al_set_thread_should_stop(audio_thread);
		al_destroy_thread(audio_thread);
		audio_thread = NULL;
	}
	if (audio_queue)
	{
		al_destroy_event_queue(audio_queue);
		audio_queue = NULL;
	}
	if (audio_stream)
	{
		al_destroy_audio_stream(audio_stream);
		audio_stream = NULL;
	}
	if (audio_voice)
	{
		al_destroy_voice(audio_voice);
		audio_voice = NULL;
	}
	if (audio_mixer)
	{
		al_destroy_mixer(audio_mixer);
		audio_mixer = NULL;
	}
	if (audio_channels)
	{
		for (uint16_t i = 0; i < audio_num_channels; i++)
		{
			audio_channel *ch = &audio_channels[i];
			al_unlock_mutex(ch->mutex);
			// Release waves if the channel owns it
			if (ch->own_wave)
			{
				free(ch->wave_data);
				al_destroy_mutex(ch->mutex);
			}
		}
		free(audio_channels);
		audio_channels = NULL;
	}
}

void audio_init(uint16_t rate, uint16_t num_channels, uint16_t frag_size, uint16_t frag_num, uint16_t rate_mul)
{
	audio_shutdown();

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

	audio_rate = rate;
	audio_num_channels = num_channels;
	audio_frag_size = frag_size;
	audio_frag_num = frag_num;
	audio_rate_mul = rate_mul;

	if (!audio_rate)
	{
		fprintf(stderr,"[audio] Error: Invalid sample rate specified.\n");
		return;
	}
	printf("[audio] Sampling rate: %dHz\n",audio_rate);
	if (!audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: At least one channel must be created.\n");
		return;
	}
	printf("[audio] Using %d channels\n",audio_num_channels);
	if (!audio_frag_size)
	{
		fprintf(stderr,"[audio] Warning: No fragment size given. Defaulting to 2048.\n");
		audio_frag_size = AUDIO_SIZE_FRAGMENT;
	}
	printf("[audio] Using %d for fragment size\n",audio_frag_size);
	if (!audio_frag_num)
	{
		fprintf(stderr,"[audio] Warning: No fragment number given. Defaulting to 2.\n");
		audio_frag_num = AUDIO_NUM_FRAGMENTS;
	}
	printf("[audio] Using %d fragments\n",audio_frag_num);
	if (!audio_rate_mul)
	{
		audio_rate_mul = 1;
	}
	printf("[audio] Rate multiplier is %d\n",audio_rate_mul);
	// Voice
	audio_voice = al_create_voice(audio_rate,
		AUDIO_DEPTH,
		AUDIO_CHAN);
	if (!audio_voice)
	{
		fprintf(stderr,"[audio] Error: Failed to create voice.\n");
		return;
	}
	printf("[audio] Created voice at %X\n",(uint16_t)audio_voice);

	// Mixer
	audio_mixer = al_create_mixer(audio_rate,
		AUDIO_DEPTH,
		AUDIO_CHAN);
	if (!audio_mixer)
	{
		fprintf(stderr,"[audio] Error: Failed to create mixer.\n");
		return;
	}
	printf("[audio] Created mixer at %X\n",(uint16_t)audio_mixer);

	if (!al_attach_mixer_to_voice(audio_mixer, audio_voice))
	{
		fprintf(stderr,"[audio] Error: Failed to attach mixer to voice.\n");
		return;
	}
	printf("[audio] Attached mixer to voice\n");

	al_set_default_mixer(audio_mixer);
	al_reserve_samples(audio_frag_num);

	// Build stream
	audio_stream = al_create_audio_stream(
		audio_frag_num,
		audio_frag_size,
		audio_rate,
		AUDIO_DEPTH,
		AUDIO_CHAN);
	printf("[audio] Created stream at %X\n",(uint16_t)audio_stream);
	if (!al_attach_audio_stream_to_mixer(audio_stream, al_get_default_mixer()))
	{
		printf("[audio] Error: Couldn't attach stream to mixer.\n");
		return;
	}
	printf("[audio] Attached stream to mixer.\n");

	// Set up event source for the audio thread
	audio_queue = al_create_event_queue();
	printf("[audio] Created queue at %X\n",(uint16_t)audio_queue);
	al_register_event_source(audio_queue, 
		al_get_audio_stream_event_source(audio_stream));
	printf("[audio] Registered audio event source with queue.\n");

	// Set up channel state
	audio_channels = (audio_channel *)calloc(audio_num_channels,sizeof(audio_channel));
	for (int i = 0; i < num_channels; i++)
	{
		audio_channel *ch = &audio_channels[i];
		ch->period = 1;
		ch->mutex = al_create_mutex();
		ch->wave_data = (uint16_t *)malloc(sizeof(uint16_t));
		ch->wave_len = 1;
		ch->own_wave = 1;
		al_unlock_mutex(ch->mutex);
	}
	printf("[audio] Created channel states at %X\n",(uint16_t)audio_channels);

	// Set up defaults for audio engine pointer
	audio_engine_ptr = NULL;
	audio_engine_cnt = 0;
	audio_engine_period = (uint32_t)(audio_rate / 60.00); // Default to 60Hz

	// Build the thread
	audio_thread = al_create_thread(audio_func, NULL);
	printf("[audio] Created audio thread.\n");
}

void audio_start(void)
{
	al_start_thread(audio_thread);
	printf("[audio] Started audio thread.\n");
}

void audio_set_engine_ptr(void *ptr, uint32_t p)
{
	audio_engine_ptr = ptr;
	if (p)
	{
		audio_engine_period = p;
	}
}

void *audio_get_engine_ptr(void)
{
	return audio_engine_ptr;
}

/* External control fuctions */
void audio_set_freq(uint16_t channel, float f)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
// Resulting frequency: (rate_mul * rate) / (wave_len * period / 2)
	uint32_t set_p = (uint32_t)((audio_rate_mul * audio_rate) / (ch->wave_len * f / 2));
	if (set_p < 1)
	{
		set_p = 1;
	}
	ch->period = set_p;
	printf("[audio] Set channel %d period to %d\n",channel,set_p);
}

void audio_set_period_direct(uint16_t channel, uint32_t period)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
	if (period < 1)
	{
		period = 1;
	}
	ch->period = period;
}

void audio_set_amp(uint16_t channel, uint16_t amp_l, uint16_t amp_r)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
	ch->amplitude[0] = amp_l;
	ch->amplitude[1] = amp_r;
}

void audio_set_noise(uint16_t channel, uint16_t noise_en)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
	ch->noise_en = noise_en;
}

void audio_set_loop(uint16_t channel, uint16_t loop_en)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
	ch->loop_en = loop_en;
}

// Point to user-owned wave data
void audio_set_wave(uint16_t channel, uint16_t *wave_data, uint16_t len, uint16_t loop_en)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
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
void audio_create_wave(uint16_t channel, uint16_t len, uint16_t loop_en)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
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

void audio_set_wave_pos(uint16_t channel, uint16_t pos)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return;
	}
	audio_channel *ch = &audio_channels[channel];
	ch->wave_pos = pos;
}

uint16_t audio_get_period(uint32_t channel)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return 0;
	}
	audio_channel *ch = &audio_channels[channel];
	return ch->period;
}
uint16_t audio_get_amp(uint16_t channel, uint16_t side)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return 0;
	}
	audio_channel *ch = &audio_channels[channel];
	return ch->amplitude[side % 2];
}

uint16_t audio_get_noise(uint16_t channel)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return 0;
	}
	audio_channel *ch = &audio_channels[channel];
	return ch->noise_en;
}

uint16_t audio_get_loop(uint16_t channel)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return 0;
	}
	audio_channel *ch = &audio_channels[channel];
	return ch->loop_en;
}

uint16_t *audio_get_wave(uint16_t channel)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return NULL;
	}
	audio_channel *ch = &audio_channels[channel];
	return ch->wave_data;
}

uint16_t audio_get_wave_len(uint16_t channel)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return 0;
	}
	audio_channel *ch = &audio_channels[channel];
	return ch->wave_len;
}

audio_channel *audio_get_channel(uint16_t channel)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return 0;
	}
	return &audio_channels[channel];
}

uint16_t audio_get_wave_pos(uint16_t channel)
{
	if (channel >= audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: Channel out of range (%d > %d)\n",channel,audio_num_channels);
		return 0;
	}
	audio_channel *ch = &audio_channels[channel];
	return ch->wave_pos;
}
