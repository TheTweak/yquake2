/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * Local defines for the sound system.
 *
 * =======================================================================
 */

#ifndef CL_SOUND_LOCAL_H
#define CL_SOUND_LOCAL_H

#define MAX_CHANNELS 32
#define MAX_RAW_SAMPLES 8192

/*
 * Holds one sample with 2 channels
 */
typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

/*
 * Holds a cached SFX and
 * it's meta data
 */
typedef struct
{
	int length;
	int loopstart;
	int speed;
	int width;
#if USE_OPENAL
	int size;
	int bufnum;
#endif
	int stereo;
	/* effect length */
	/* begin<->attack..fade<->end */
	int begin;
	int end;
	int attack;
	int fade;
	/* effect volume */
	short volume;
	byte data[1];
} sfxcache_t;

/*
 * Holds a SFX
 */
typedef struct sfx_s
{
	char name[MAX_QPATH];
	int registration_sequence;
	sfxcache_t *cache;
	char *truename;
	bool is_silenced_muzzle_flash;
} sfx_t;

/* A playsound_t will be generated by each call
 * to S_StartSound. When the mixer reaches
 * playsound->begin, the playsound will
 * be assigned to a channel. */
typedef struct playsound_s
{
	struct playsound_s *prev, *next;
	sfx_t *sfx;
	float volume;
	float attenuation;
	int entnum;
	int entchannel;
	bool fixed_origin;
	vec3_t origin;
	unsigned begin;
} playsound_t;

/*
 * Interface to pass data and metadata
 * between the frontend and the backends.
 * Mainly used by the SDL backend, since
 * the OpenAL backend has it's own AL
 * based magic.
 */
typedef struct
{
	int channels;
	int samples;          /* mono samples in buffer */
	int submission_chunk; /* don't mix less than this */
	int samplepos;        /* in mono samples */
	int samplebits;
	int speed;
	unsigned char *buffer;
} sound_t;

/*
 * Hold all information for one
 * playback channel.
 */
typedef struct
{
	sfx_t *sfx;                 /* sfx number */
	int leftvol;                /* 0-255 volume */
	int rightvol;               /* 0-255 volume */
	int end;                    /* end time in global paintsamples */
	int pos;                    /* sample position in sfx */
	int looping;                /* where to loop, -1 = no looping */
	int entnum;                 /* to allow overriding a specific sound */
	int entchannel;
	vec3_t origin;              /* only use if fixed_origin is set */
	vec_t dist_mult;            /* distance multiplier (attenuation/clipK) */
	int master_vol;             /* 0-255 master volume */
	bool fixed_origin;      /* use origin instead of fetching entnum's origin */
	bool autosound;         /* from an entity->sound, cleared each frame */
#if USE_OPENAL
	int autoframe;
	float oal_vol;
	int srcnum;
#endif
} channel_t;

/*
 * Information read from
 * wave file header.
 */
typedef struct
{
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs; /* chunk starts this many bytes from file start */
} wavinfo_t;

/*
 * Type of active sound backend
 */
typedef enum
{
	SS_NOT = 0, /* soundsystem not started */
	SS_SDL,     /* soundsystem started, using SDL */
	SS_OAL      /* soundsystem started, using OpenAL */
} sndstarted_t;

/*
 * cvars
 */
extern cvar_t *s_volume;
extern cvar_t *s_nosound;
extern cvar_t *s_loadas8bit;
extern cvar_t *s_khz;
extern cvar_t *s_show;
extern cvar_t *s_mixahead;
extern cvar_t *s_testsound;
extern cvar_t *s_ambient;
extern cvar_t* s_underwater;
extern cvar_t* s_underwater_gain_hf;
extern cvar_t* s_doppler;
extern cvar_t* s_ps_sorting;

/*
 * Globals
 */
extern channel_t channels[MAX_CHANNELS];
extern int paintedtime;
extern int s_numchannels;
extern int s_rawend;
extern playsound_t s_pendingplays;
extern portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];
extern sndstarted_t sound_started;
extern sound_t sound;
extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;

extern bool snd_is_underwater;
extern bool snd_is_underwater_enabled;

/*
 * Returns the header infos
 * of a wave file
 */
wavinfo_t GetWavinfo(char *name, byte *wav, int wavlength);

/*
 * Loads one sample into
 * the cache
 */
sfxcache_t *S_LoadSound(sfx_t *s);

/*
 * Plays one sound sample
 */
void S_IssuePlaysound(playsound_t *ps);

/*
 * picks a channel based on priorities,
 * empty slots, number of channels
 */
channel_t *S_PickChannel(int entnum, int entchannel);

/*
 * Builds a list of all
 * sound still in flight
 */
void S_BuildSoundList(int *sounds);

/* ----------------------------------------------------------------- */

/*
 * Initalizes the SDL backend
 */
bool SDL_BackendInit(void);

/*
 * Shuts the SDL backend down
 */
void SDL_BackendShutdown(void);

/*
 * Print information about
 * the SDL backend
 */
void SDL_SoundInfo(void);

/*
 * Alters start position of
 * sound playback
 */
int  SDL_DriftBeginofs(float);

/*
 * Clears all playback buffers
 */
void SDL_ClearBuffer(void);

/*
 * Caches an sample for use
 * the SDL backend
 */
bool SDL_Cache(sfx_t *sfx, wavinfo_t *info, byte *data, short volume,
				 int begin_length, int  end_length,
				 int attack_length, int fade_length);

/*
 * Performs all sound calculations
 * for the SDL backendend and fills
 * the buffer
 */
void SDL_Update(void);

/*
 * Queues raw samples for
 * playback
 */
void SDL_RawSamples(int samples, int rate, int width,
		int channels, byte *data, float volume);

/*
 * Spartializes a sample
 */
void SDL_Spatialize(channel_t *ch);

/* ----------------------------------------------------------------- */

#if USE_OPENAL

 /* Only begin attenuating sound volumes
    when outside the FULLVOLUME range */
 #define     SOUND_FULLVOLUME 1.0
 #define     SOUND_LOOPATTENUATE 0.003

/* number of buffers in flight (needed for ogg) */
extern int active_buffers;

/*
 * Informations about the
 * OpenAL backend
 */
void AL_SoundInfo(void);

/* Initializes the OpenAL backend
 */
bool AL_Init(void);

/*
 * Shuts the OpenAL backend down
 */
void AL_Shutdown(void);

/*
 * Upload ("cache") one sample
 * into OpenAL
 */
sfxcache_t *AL_UploadSfx(sfx_t *s, wavinfo_t *s_info, byte *data, short volume,
						 int begin_length, int  end_length,
						 int attack_length, int fade_length);

/*
 * Deletes one sample from OpenAL
 */
void AL_DeleteSfx(sfx_t *s);

/*
 * Stops playback of a channel
 */
void AL_StopChannel(channel_t *ch);

/*
 * Starts playback of a channel
 */
void AL_PlayChannel(channel_t *ch);

/*
 * Stops playback of all channels
 */
void AL_StopAllChannels(void);

/*
 * Perform caculations and fill
 * OpenALs buffer
 */
void AL_Update(void);

/*
 * Plays raw samples
 */
void AL_RawSamples(int samples, int rate, int width,
		int channels, byte *data, float volume);

/*
 * Unqueues any raw samples
 * still in flight
 */
void AL_UnqueueRawSamples();

#endif /* USE_OPENAL */
#endif /* CL_SOUND_LOCAL_H */

