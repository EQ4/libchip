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

static audio_channel *audio_channels;

void audio_step(int16_t *frame)
{
	memset(frame, 0, sizeof(int16_t) * 2);
	for (uint16_t chan = 0; chan < 2; chan++)
	{
		for (uint16_t i = 0; i < audio_num_channels; i++)
		{
			audio_channel *ch = &audio_channels[i];
			for (uint16_t k = 0; k < audio_rate_mul; k++)
			{
				frame[chan] += ch->wave_data[ch->wave_pos];
				if (ch->counter != 0)
				{
					ch->counter--;
				}
				else
				{
					ch->counter = ch->period - 1;
					if (ch->wave_pos == ch->wave_len)
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
			}
			frame[chan] /= audio_rate_mul;
			frame[chan] *= ch->amplitude[chan];
			frame[chan] = (frame[chan] << 7) / audio_num_channels;
		}		
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
			// Release waves if the channel owns it
			if (ch->own_wave)
			{
				free(ch->wave_data);
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
	printf("[audio] Created channel states at %X\n",(uint16_t)audio_channels);

	// Build the thread
	audio_thread = al_create_thread(audio_func, NULL);
	printf("[audio] Created audio thread.\n");
	al_start_thread(audio_thread);
	printf("[audio] Started audio thread.\n");

	// Test garbage
	for (int i = 0; i < audio_num_channels; i++)
	{
		audio_channel *ch = &audio_channels[i];
		ch->wave_len = 32;
		switch (i)
		{
			case 0:

				ch->period = 4;
				break;

			case 1:

				ch->period = 900;
				break;

			case 2:

				ch->period = 2000;
				break;
		}
		ch->amplitude[0] = 0xF;
		ch->amplitude[1] = 0xF;
		ch->wave_data = (uint16_t *)malloc(sizeof(uint16_t) * ch->wave_len);
		for (int k = 0; k < ch->wave_len; k++)
		{

			ch->wave_data[k] = k / 2;
		}
		ch->own_wave = 1;
		ch->loop_en = 1;
	}
}

/* External control fuctions */


