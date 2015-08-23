#include "audio.h"

/* Internal workings */
static ALLEGRO_EVENT_QUEUE *audio_queue;
static ALLEGRO_AUDIO_STREAM *audio_stream;
static ALLEGRO_THREAD *audio_thread;

static uint audio_rate;
static uint audio_frag_size;
static uint audio_frag_num;
static uint audio_rate_mul;
static uint audio_num_channels;

static audio_channel *audio_channels;

void audio_step(int16_t *frame)
{
	memset(frame, 0, sizeof(int16_t) * 2);
	for (uint chan = 0; chan < 2; chan++)
	{
		for (uint i = 0; i < audio_num_channels; i++)
		{
			audio_channel *ch = &audio_channels[i];
			for (uint k = 0; k < audio_rate_mul; k++)
			{
				frame[chan] += ch->wave_data[ch->wave_pos];
			}
			frame[chan] /= audio_rate_mul;
			frame[chan] *= ch->amplitude[chan];
			frame[chan] = (frame[chan] << 7);
		}
	}
}

static void* audio_func(ALLEGRO_THREAD *thr, void *arg)
{
	int16_t *frame;
	while (!al_get_thread_should_stop(thr))
	{
		ALLEGRO_EVENT event;
		while (!al_is_event_queue_empty(audio_queue))
		{
			al_get_next_event(audio_queue, &event);
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
					
					al_drain_audio_stream(audio_stream);
					break;
			}
		}
	}
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
	if (audio_channels)
	{
		for (uint i = 0; i < audio_num_channels; i++)
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

void audio_init(uint rate, uint num_channels, uint frag_size, uint frag_num, uint rate_mul)
{
	audio_shutdown();
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
	if (!audio_num_channels)
	{
		fprintf(stderr,"[audio] Error: At least one channel must be created.\n");
		return;
	}
	if (!audio_frag_size)
	{
		fprintf(stderr,"[audio] Warning: No fragment size given. Defaulting to 2048.\n");
		audio_frag_size = AUDIO_SIZE_FRAGMENT;
	}
	if (!audio_frag_num)
	{
		fprintf(stderr,"[audio] Warning: No fragment number given. Defaulting to 2.\n");
		audio_frag_num = AUDIO_NUM_FRAGMENTS;
	}
	if (!audio_rate_mul)
	{
		audio_rate_mul = 1;
	}
	// Build stream
	audio_stream = al_create_audio_stream(
		audio_frag_num,
		audio_frag_size,
		audio_rate,
		AUDIO_DEPTH,
		AUDIO_CHAN);
	al_attach_audio_stream_to_mixer(audio_stream, al_get_default_mixer());

	// Set up event source for the audio thread
	audio_queue = al_create_event_queue();
	al_register_event_source(audio_queue, 
		al_get_audio_stream_event_source(audio_stream));

	// Set up channel state
	audio_channels = (audio_channel *)calloc(audio_num_channels,sizeof(audio_channel));

	// Build the thread
	audio_thread = al_create_thread(audio_func, NULL);
}

/* External control fuctions */


