/*
 * Copyright (C) 2012 Yamagi Burmeister
 * Copyright (C) 2010 skuller.net
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
 * Low level, platform depended "qal" API implementation. This files
 * provides functions to load, initialize, shutdown und unload the
 * OpenAL library and connects the "qal" funtion pointers to the
 * OpenAL functions. It shopuld work on Windows and unixoid Systems,
 * other platforms may need an own implementation. This source file
 * was taken from Q2Pro and modified by the YQ2 authors.
 *
 * =======================================================================
 */

#ifdef USE_OPENAL

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include "../../common/header/common.h"
#include "../../client/sound/header/local.h"
#include "header/qal.h"

static ALCcontext *context;
static ALCdevice *device;
static cvar_t *al_device;
static cvar_t *al_driver;
static bool hasAlcExtDisconnect;
static void *handle;

/* Function pointers for OpenAL management */
static LPALCCREATECONTEXT qalcCreateContext;
static LPALCMAKECONTEXTCURRENT qalcMakeContextCurrent;
static LPALCPROCESSCONTEXT qalcProcessContext;
static LPALCSUSPENDCONTEXT qalcSuspendContext;
static LPALCDESTROYCONTEXT qalcDestroyContext;
static LPALCGETCURRENTCONTEXT qalcGetCurrentContext;
static LPALCGETCONTEXTSDEVICE qalcGetContextsDevice;
static LPALCOPENDEVICE qalcOpenDevice;
static LPALCCLOSEDEVICE qalcCloseDevice;
static LPALCGETERROR qalcGetError;
static LPALCISEXTENSIONPRESENT qalcIsExtensionPresent;
static LPALCGETPROCADDRESS qalcGetProcAddress;
static LPALCGETENUMVALUE qalcGetEnumValue;
static LPALCGETSTRING qalcGetString;
static LPALCGETINTEGERV qalcGetIntegerv;
static LPALCCAPTUREOPENDEVICE qalcCaptureOpenDevice;
static LPALCCAPTURECLOSEDEVICE qalcCaptureCloseDevice;
static LPALCCAPTURESTART qalcCaptureStart;
static LPALCCAPTURESTOP qalcCaptureStop;
static LPALCCAPTURESAMPLES qalcCaptureSamples ;

/* Declaration of function pointers used
   to connect OpenAL to our internal API */
LPALENABLE qalEnable;
LPALDISABLE qalDisable;
LPALISENABLED qalIsEnabled;
LPALGETSTRING qalGetString;
LPALGETBOOLEANV qalGetBooleanv;
LPALGETINTEGERV qalGetIntegerv;
LPALGETFLOATV qalGetFloatv;
LPALGETDOUBLEV qalGetDoublev;
LPALGETBOOLEAN qalGetBoolean;
LPALGETINTEGER qalGetInteger;
LPALGETFLOAT qalGetFloat;
LPALGETDOUBLE qalGetDouble;
LPALGETERROR qalGetError;
LPALISEXTENSIONPRESENT qalIsExtensionPresent;
LPALGETPROCADDRESS qalGetProcAddress;
LPALGETENUMVALUE qalGetEnumValue;
LPALLISTENERF qalListenerf;
LPALLISTENER3F qalListener3f;
LPALLISTENERFV qalListenerfv;
LPALLISTENERI qalListeneri;
LPALLISTENER3I qalListener3i;
LPALLISTENERIV qalListeneriv;
LPALGETLISTENERF qalGetListenerf;
LPALGETLISTENER3F qalGetListener3f;
LPALGETLISTENERFV qalGetListenerfv;
LPALGETLISTENERI qalGetListeneri;
LPALGETLISTENER3I qalGetListener3i;
LPALGETLISTENERIV qalGetListeneriv;
LPALGENSOURCES qalGenSources;
LPALDELETESOURCES qalDeleteSources;
LPALISSOURCE qalIsSource;
LPALSOURCEF qalSourcef;
LPALSOURCE3F qalSource3f;
LPALSOURCEFV qalSourcefv;
LPALSOURCEI qalSourcei;
LPALSOURCE3I qalSource3i;
LPALSOURCEIV qalSourceiv;
LPALGETSOURCEF qalGetSourcef;
LPALGETSOURCE3F qalGetSource3f;
LPALGETSOURCEFV qalGetSourcefv;
LPALGETSOURCEI qalGetSourcei;
LPALGETSOURCE3I qalGetSource3i;
LPALGETSOURCEIV qalGetSourceiv;
LPALSOURCEPLAYV qalSourcePlayv;
LPALSOURCESTOPV qalSourceStopv;
LPALSOURCEREWINDV qalSourceRewindv;
LPALSOURCEPAUSEV qalSourcePausev;
LPALSOURCEPLAY qalSourcePlay;
LPALSOURCESTOP qalSourceStop;
LPALSOURCEREWIND qalSourceRewind;
LPALSOURCEPAUSE qalSourcePause;
LPALSOURCEQUEUEBUFFERS qalSourceQueueBuffers;
LPALSOURCEUNQUEUEBUFFERS qalSourceUnqueueBuffers;
LPALGENBUFFERS qalGenBuffers;
LPALDELETEBUFFERS qalDeleteBuffers;
LPALISBUFFER qalIsBuffer;
LPALBUFFERDATA qalBufferData;
LPALBUFFERF qalBufferf;
LPALBUFFER3F qalBuffer3f;
LPALBUFFERFV qalBufferfv;
LPALBUFFERI qalBufferi;
LPALBUFFER3I qalBuffer3i;
LPALBUFFERIV qalBufferiv;
LPALGETBUFFERF qalGetBufferf;
LPALGETBUFFER3F qalGetBuffer3f;
LPALGETBUFFERFV qalGetBufferfv;
LPALGETBUFFERI qalGetBufferi;
LPALGETBUFFER3I qalGetBuffer3i;
LPALGETBUFFERIV qalGetBufferiv;
LPALDOPPLERFACTOR qalDopplerFactor;
LPALDOPPLERVELOCITY qalDopplerVelocity;
LPALSPEEDOFSOUND qalSpeedOfSound;
LPALDISTANCEMODEL qalDistanceModel;
LPALGENFILTERS qalGenFilters;
LPALFILTERI qalFilteri;
LPALFILTERF qalFilterf;
LPALDELETEFILTERS qalDeleteFilters;

/*
 * Gives information over the OpenAL
 * implementation and it's state
 */
void QAL_SoundInfo()
{
	Com_Printf("OpenAL settings:\n");
	Com_Printf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
	Com_Printf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
	Com_Printf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
	Com_Printf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));

	if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
	{
		const char *devs = qalcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);

		Com_Printf("\nAvailable OpenAL devices:\n");

		if (devs == NULL)
		{
			Com_Printf("- No devices found. Depending on your\n");
			Com_Printf("  platform this may be expected and\n");
			Com_Printf("  doesn't indicate a problem!\n");
		}
		else
		{
			while (devs && *devs)
			{
				Com_Printf("- %s\n", devs);
				devs += strlen(devs) + 1;
			}
		}
	}

   	if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
	{
		const char *devs = qalcGetString(device, ALC_DEVICE_SPECIFIER);

		Com_Printf("\nCurrent OpenAL device:\n");

		if (devs == NULL)
		{
			Com_Printf("- No OpenAL device in use\n");
		}
		else
		{
			Com_Printf("- %s\n", devs);
		}
	}
}

/*
 * Checks if the output device is still connected. Returns true
 * if it is, false otherwise. Should be called every frame, if
 * disconnected a 'snd_restart' is injected after waiting for 2.5
 * seconds.
 *
 * This is mostly a work around for broken Sound driver. For
 * example the _good_ Intel display driver for Windows 10
 * destroys the DisplayPort sound device when the display
 * resolution changes and recreates it after an unspecified
 * amount of time...
 */
bool
QAL_RecoverLostDevice()
{
	static int discoCount = 0;

	if (!hasAlcExtDisconnect)
	{
		return true;
	}

	if (discoCount == 0)
	{
		ALCint connected;
		qalcGetIntegerv(device, ALC_CONNECTED, 1, &connected);

		if (!connected)
		{
			discoCount++;
			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		/* Wait for about 2.5 seconds
		   before trying to reconnect. */
		if (discoCount < (int)(Cvar_VariableValue("cl_maxfps") * 2.5))
		{
			discoCount++;
			return false;
		}
		else
		{
			Cbuf_AddText("snd_restart\n");
			discoCount = 0;
			return false;
		}
	}
}

/*
 * Shuts OpenAL down, frees all context and
 * device handles and unloads the shared lib.
 */
void
QAL_Shutdown()
{
	if (context)
	{
		qalcMakeContextCurrent( NULL );
		qalcDestroyContext( context );
		context = NULL;
	}

	if (device)
	{
		qalcCloseDevice( device );
		device = NULL;
	}

	/* Disconnect function pointers used
	   for OpenAL management calls */
	qalcCreateContext = NULL;
	qalcMakeContextCurrent = NULL;
	qalcProcessContext = NULL;
	qalcSuspendContext = NULL;
	qalcDestroyContext = NULL;
	qalcGetCurrentContext = NULL;
	qalcGetContextsDevice = NULL;
	qalcOpenDevice = NULL;
	qalcCloseDevice = NULL;
	qalcGetError = NULL;
	qalcIsExtensionPresent = NULL;
	qalcGetProcAddress = NULL;
	qalcGetEnumValue = NULL;
	qalcGetString = NULL;
	qalcGetIntegerv = NULL;
	qalcCaptureOpenDevice = NULL;
	qalcCaptureCloseDevice = NULL;
	qalcCaptureStart = NULL;
	qalcCaptureStop = NULL;
	qalcCaptureSamples = NULL;

	/* Disconnect OpenAL
	 * function pointers */
	qalEnable = NULL;
	qalDisable = NULL;
	qalIsEnabled = NULL;
	qalGetString = NULL;
	qalGetBooleanv = NULL;
	qalGetIntegerv = NULL;
	qalGetFloatv = NULL;
	qalGetDoublev = NULL;
	qalGetBoolean = NULL;
	qalGetInteger = NULL;
	qalGetFloat = NULL;
	qalGetDouble = NULL;
	qalGetError = NULL;
	qalIsExtensionPresent = NULL;
	qalGetProcAddress = NULL;
	qalGetEnumValue = NULL;
	qalListenerf = NULL;
	qalListener3f = NULL;
	qalListenerfv = NULL;
	qalListeneri = NULL;
	qalListener3i = NULL;
	qalListeneriv = NULL;
	qalGetListenerf = NULL;
	qalGetListener3f = NULL;
	qalGetListenerfv = NULL;
	qalGetListeneri = NULL;
	qalGetListener3i = NULL;
	qalGetListeneriv = NULL;
	qalGenSources = NULL;
	qalDeleteSources = NULL;
	qalIsSource = NULL;
	qalSourcef = NULL;
	qalSource3f = NULL;
	qalSourcefv = NULL;
	qalSourcei = NULL;
	qalSource3i = NULL;
	qalSourceiv = NULL;
	qalGetSourcef = NULL;
	qalGetSource3f = NULL;
	qalGetSourcefv = NULL;
	qalGetSourcei = NULL;
	qalGetSource3i = NULL;
	qalGetSourceiv = NULL;
	qalSourcePlayv = NULL;
	qalSourceStopv = NULL;
	qalSourceRewindv = NULL;
	qalSourcePausev = NULL;
	qalSourcePlay = NULL;
	qalSourceStop = NULL;
	qalSourceRewind = NULL;
	qalSourcePause = NULL;
	qalSourceQueueBuffers = NULL;
	qalSourceUnqueueBuffers = NULL;
	qalGenBuffers = NULL;
	qalDeleteBuffers = NULL;
	qalIsBuffer = NULL;
	qalBufferData = NULL;
	qalBufferf = NULL;
	qalBuffer3f = NULL;
	qalBufferfv = NULL;
	qalBufferi = NULL;
	qalBuffer3i = NULL;
	qalBufferiv = NULL;
	qalGetBufferf = NULL;
	qalGetBuffer3f = NULL;
	qalGetBufferfv = NULL;
	qalGetBufferi = NULL;
	qalGetBuffer3i = NULL;
	qalGetBufferiv = NULL;
	qalDopplerFactor = NULL;
	qalDopplerVelocity = NULL;
	qalSpeedOfSound = NULL;
	qalDistanceModel = NULL;
	qalGenFilters = NULL;
	qalFilteri = NULL;
	qalFilterf = NULL;
	qalDeleteFilters = NULL;

	if (handle)
	{
		/* Unload the shared lib */
		Sys_FreeLibrary(handle);
		handle = NULL;
	}
}

/*
 * Loads the OpenAL shared lib, creates
 * a context and device handle.
 */
bool
QAL_Init()
{
	al_device = Cvar_Get("al_device", "", CVAR_ARCHIVE);

	/* DEFAULT_OPENAL_DRIVER is defined at compile time via the compiler */
	al_driver = Cvar_Get("al_driver", DEFAULT_OPENAL_DRIVER, CVAR_ARCHIVE);

	Com_Printf("Loading library: %s\n", al_driver->string);

	/* Load the library */
	Sys_LoadLibrary(al_driver->string, NULL, &handle);

	if (!handle)
	{
		Com_Printf("Loading %s failed! Disabling OpenAL.\n", al_driver->string);
		return false;
	}

	#define ALSYMBOL(handle, sym) Sys_GetProcAddress(handle, #sym)

	/* Connect function pointers to management functions */
	qalcCreateContext = reinterpret_cast<LPALCCREATECONTEXT>(ALSYMBOL(handle, alcCreateContext));
	qalcMakeContextCurrent = reinterpret_cast<LPALCMAKECONTEXTCURRENT>(ALSYMBOL(handle, alcMakeContextCurrent));
	qalcProcessContext = reinterpret_cast<LPALCPROCESSCONTEXT>(ALSYMBOL(handle, alcProcessContext));
	qalcSuspendContext = reinterpret_cast<LPALCSUSPENDCONTEXT>(ALSYMBOL(handle, alcSuspendContext));
	qalcDestroyContext = reinterpret_cast<LPALCDESTROYCONTEXT>(ALSYMBOL(handle, alcDestroyContext));
	qalcGetCurrentContext = reinterpret_cast<LPALCGETCURRENTCONTEXT>(ALSYMBOL(handle, alcGetCurrentContext));
	qalcGetContextsDevice = reinterpret_cast<LPALCGETCONTEXTSDEVICE>(ALSYMBOL(handle, alcGetContextsDevice));
	qalcOpenDevice = reinterpret_cast<LPALCOPENDEVICE>(ALSYMBOL(handle, alcOpenDevice));
	qalcCloseDevice = reinterpret_cast<LPALCCLOSEDEVICE>(ALSYMBOL(handle, alcCloseDevice));
	qalcGetError = reinterpret_cast<LPALCGETERROR>(ALSYMBOL(handle, alcGetError));
	qalcIsExtensionPresent = reinterpret_cast<LPALCISEXTENSIONPRESENT>(ALSYMBOL(handle, alcIsExtensionPresent));
	qalcGetProcAddress = reinterpret_cast<LPALCGETPROCADDRESS>(ALSYMBOL(handle, alcGetProcAddress));
	qalcGetEnumValue = reinterpret_cast<LPALCGETENUMVALUE>(ALSYMBOL(handle, alcGetEnumValue));
	qalcGetString = reinterpret_cast<LPALCGETSTRING>(ALSYMBOL(handle, alcGetString));
	qalcGetIntegerv = reinterpret_cast<LPALCGETINTEGERV>(ALSYMBOL(handle, alcGetIntegerv));
	qalcCaptureOpenDevice = reinterpret_cast<LPALCCAPTUREOPENDEVICE>(ALSYMBOL(handle, alcCaptureOpenDevice));
	qalcCaptureCloseDevice = reinterpret_cast<LPALCCAPTURECLOSEDEVICE>(ALSYMBOL(handle, alcCaptureCloseDevice));
	qalcCaptureStart = reinterpret_cast<LPALCCAPTURESTART>(ALSYMBOL(handle, alcCaptureStart));
	qalcCaptureStop = reinterpret_cast<LPALCCAPTURESTOP>(ALSYMBOL(handle, alcCaptureStop));
	qalcCaptureSamples = reinterpret_cast<LPALCCAPTURESAMPLES>(ALSYMBOL(handle, alcCaptureSamples));

	/* Connect function pointers to
	   to OpenAL API functions */
	qalEnable = reinterpret_cast<LPALENABLE>(ALSYMBOL(handle, alEnable));
    qalDisable = reinterpret_cast<LPALDISABLE>(ALSYMBOL(handle, alDisable));
    qalIsEnabled = reinterpret_cast<LPALISENABLED>(ALSYMBOL(handle, alIsEnabled));
    qalGetString = reinterpret_cast<LPALGETSTRING>(ALSYMBOL(handle, alGetString));
    qalGetBooleanv = reinterpret_cast<LPALGETBOOLEANV>(ALSYMBOL(handle, alGetBooleanv));
    qalGetIntegerv = reinterpret_cast<LPALGETINTEGERV>(ALSYMBOL(handle, alGetIntegerv));
    qalGetFloatv = reinterpret_cast<LPALGETFLOATV>(ALSYMBOL(handle, alGetFloatv));
    qalGetDoublev = reinterpret_cast<LPALGETDOUBLEV>(ALSYMBOL(handle, alGetDoublev));
    qalGetBoolean = reinterpret_cast<LPALGETBOOLEAN>(ALSYMBOL(handle, alGetBoolean));
    qalGetInteger = reinterpret_cast<LPALGETINTEGER>(ALSYMBOL(handle, alGetInteger));
    qalGetFloat = reinterpret_cast<LPALGETFLOAT>(ALSYMBOL(handle, alGetFloat));
    qalGetDouble = reinterpret_cast<LPALGETDOUBLE>(ALSYMBOL(handle, alGetDouble));
    qalGetError = reinterpret_cast<LPALGETERROR>(ALSYMBOL(handle, alGetError));
    qalIsExtensionPresent = reinterpret_cast<LPALISEXTENSIONPRESENT>(ALSYMBOL(handle, alIsExtensionPresent));
    qalGetProcAddress = reinterpret_cast<LPALGETPROCADDRESS>(ALSYMBOL(handle, alGetProcAddress));
    qalGetEnumValue = reinterpret_cast<LPALGETENUMVALUE>(ALSYMBOL(handle, alGetEnumValue));
    qalListenerf = reinterpret_cast<LPALLISTENERF>(ALSYMBOL(handle, alListenerf));
    qalListener3f = reinterpret_cast<LPALLISTENER3F>(ALSYMBOL(handle, alListener3f));
    qalListenerfv = reinterpret_cast<LPALLISTENERFV>(ALSYMBOL(handle, alListenerfv));
    qalListeneri = reinterpret_cast<LPALLISTENERI>(ALSYMBOL(handle, alListeneri));
    qalListener3i = reinterpret_cast<LPALLISTENER3I>(ALSYMBOL(handle, alListener3i));
    qalListeneriv = reinterpret_cast<LPALLISTENERIV>(ALSYMBOL(handle, alListeneriv));
    qalGetListenerf = reinterpret_cast<LPALGETLISTENERF>(ALSYMBOL(handle, alGetListenerf));
    qalGetListener3f = reinterpret_cast<LPALGETLISTENER3F>(ALSYMBOL(handle, alGetListener3f));
    qalGetListenerfv = reinterpret_cast<LPALGETLISTENERFV>(ALSYMBOL(handle, alGetListenerfv));
    qalGetListeneri = reinterpret_cast<LPALGETLISTENERI>(ALSYMBOL(handle, alGetListeneri));
    qalGetListener3i = reinterpret_cast<LPALGETLISTENER3I>(ALSYMBOL(handle, alGetListener3i));
    qalGetListeneriv = reinterpret_cast<LPALGETLISTENERIV>(ALSYMBOL(handle, alGetListeneriv));
    qalGenSources = reinterpret_cast<LPALGENSOURCES>(ALSYMBOL(handle, alGenSources));
    qalDeleteSources = reinterpret_cast<LPALDELETESOURCES>(ALSYMBOL(handle, alDeleteSources));
    qalIsSource = reinterpret_cast<LPALISSOURCE>(ALSYMBOL(handle, alIsSource));
    qalSourcef = reinterpret_cast<LPALSOURCEF>(ALSYMBOL(handle, alSourcef));
    qalSource3f = reinterpret_cast<LPALSOURCE3F>(ALSYMBOL(handle, alSource3f));
    qalSourcefv = reinterpret_cast<LPALSOURCEFV>(ALSYMBOL(handle, alSourcefv));
    qalSourcei = reinterpret_cast<LPALSOURCEI>(ALSYMBOL(handle, alSourcei));
    qalSource3i = reinterpret_cast<LPALSOURCE3I>(ALSYMBOL(handle, alSource3i));
    qalSourceiv = reinterpret_cast<LPALSOURCEIV>(ALSYMBOL(handle, alSourceiv));
    qalGetSourcef = reinterpret_cast<LPALGETSOURCEF>(ALSYMBOL(handle, alGetSourcef));
    qalGetSource3f = reinterpret_cast<LPALGETSOURCE3F>(ALSYMBOL(handle, alGetSource3f));
    qalGetSourcefv = reinterpret_cast<LPALGETSOURCEFV>(ALSYMBOL(handle, alGetSourcefv));
    qalGetSourcei = reinterpret_cast<LPALGETSOURCEI>(ALSYMBOL(handle, alGetSourcei));
    qalGetSource3i = reinterpret_cast<LPALGETSOURCE3I>(ALSYMBOL(handle, alGetSource3i));
    qalGetSourceiv = reinterpret_cast<LPALGETSOURCEIV>(ALSYMBOL(handle, alGetSourceiv));
    qalSourcePlayv = reinterpret_cast<LPALSOURCEPLAYV>(ALSYMBOL(handle, alSourcePlayv));
    qalSourceStopv = reinterpret_cast<LPALSOURCESTOPV>(ALSYMBOL(handle, alSourceStopv));
    qalSourceRewindv = reinterpret_cast<LPALSOURCEREWINDV>(ALSYMBOL(handle, alSourceRewindv));
    qalSourcePausev = reinterpret_cast<LPALSOURCEPAUSEV>(ALSYMBOL(handle, alSourcePausev));
    qalSourcePlay = reinterpret_cast<LPALSOURCEPLAY>(ALSYMBOL(handle, alSourcePlay));
    qalSourceStop = reinterpret_cast<LPALSOURCESTOP>(ALSYMBOL(handle, alSourceStop));
    qalSourceRewind = reinterpret_cast<LPALSOURCEREWIND>(ALSYMBOL(handle, alSourceRewind));
    qalSourcePause = reinterpret_cast<LPALSOURCEPAUSE>(ALSYMBOL(handle, alSourcePause));
    qalSourceQueueBuffers = reinterpret_cast<LPALSOURCEQUEUEBUFFERS>(ALSYMBOL(handle, alSourceQueueBuffers));
    qalSourceUnqueueBuffers = reinterpret_cast<LPALSOURCEUNQUEUEBUFFERS>(ALSYMBOL(handle, alSourceUnqueueBuffers));
    qalGenBuffers = reinterpret_cast<LPALGENBUFFERS>(ALSYMBOL(handle, alGenBuffers));
    qalDeleteBuffers = reinterpret_cast<LPALDELETEBUFFERS>(ALSYMBOL(handle, alDeleteBuffers));
    qalIsBuffer = reinterpret_cast<LPALISBUFFER>(ALSYMBOL(handle, alIsBuffer));
    qalBufferData = reinterpret_cast<LPALBUFFERDATA>(ALSYMBOL(handle, alBufferData));
    qalBufferf = reinterpret_cast<LPALBUFFERF>(ALSYMBOL(handle, alBufferf));
    qalBuffer3f = reinterpret_cast<LPALBUFFER3F>(ALSYMBOL(handle, alBuffer3f));
    qalBufferfv = reinterpret_cast<LPALBUFFERFV>(ALSYMBOL(handle, alBufferfv));
    qalBufferi = reinterpret_cast<LPALBUFFERI>(ALSYMBOL(handle, alBufferi));
    qalBuffer3i = reinterpret_cast<LPALBUFFER3I>(ALSYMBOL(handle, alBuffer3i));
    qalBufferiv = reinterpret_cast<LPALBUFFERIV>(ALSYMBOL(handle, alBufferiv));
    qalGetBufferf = reinterpret_cast<LPALGETBUFFERF>(ALSYMBOL(handle, alGetBufferf));
    qalGetBuffer3f = reinterpret_cast<LPALGETBUFFER3F>(ALSYMBOL(handle, alGetBuffer3f));
    qalGetBufferfv = reinterpret_cast<LPALGETBUFFERFV>(ALSYMBOL(handle, alGetBufferfv));
    qalGetBufferi = reinterpret_cast<LPALGETBUFFERI>(ALSYMBOL(handle, alGetBufferi));
    qalGetBuffer3i = reinterpret_cast<LPALGETBUFFER3I>(ALSYMBOL(handle, alGetBuffer3i));
    qalGetBufferiv = reinterpret_cast<LPALGETBUFFERIV>(ALSYMBOL(handle, alGetBufferiv));
    qalDopplerFactor = reinterpret_cast<LPALDOPPLERFACTOR>(ALSYMBOL(handle, alDopplerFactor));
    qalDopplerVelocity = reinterpret_cast<LPALDOPPLERVELOCITY>(ALSYMBOL(handle, alDopplerVelocity));
    qalSpeedOfSound = reinterpret_cast<LPALSPEEDOFSOUND>(ALSYMBOL(handle, alSpeedOfSound));
    qalDistanceModel = reinterpret_cast<LPALDISTANCEMODEL>(ALSYMBOL(handle, alDistanceModel));


	/* Open the OpenAL device */
	Com_Printf("...opening OpenAL device: ");

	device = qalcOpenDevice(al_device->string[0] ? al_device->string : NULL);

	if(!device)
	{
		Com_DPrintf("failed\n");
		QAL_Shutdown();
		return false;
	}

	Com_Printf("ok\n");

	/* Create the OpenAL context */
	Com_Printf("...creating OpenAL context: ");

	context = qalcCreateContext(device, NULL);

	if(!context)
	{
		Com_DPrintf("failed\n");
		QAL_Shutdown();
		return false;
	}

	Com_Printf("ok\n");

	/* Set the created context as current context */
	Com_Printf("...making context current: ");

	if (!qalcMakeContextCurrent(context))
	{
		Com_DPrintf("failed\n");
		QAL_Shutdown();
		return false;
	}

	if (qalcIsExtensionPresent(device, "ALC_EXT_EFX") != AL_FALSE) {
		qalGenFilters = reinterpret_cast<LPALGENFILTERS>(qalGetProcAddress("alGenFilters"));
		qalFilteri = reinterpret_cast<LPALFILTERI>(qalGetProcAddress("alFilteri"));
		qalFilterf = reinterpret_cast<LPALFILTERF>(qalGetProcAddress("alFilterf"));
		qalDeleteFilters = reinterpret_cast<LPALDELETEFILTERS>(qalGetProcAddress("alDeleteFilters"));
	} else {
		qalGenFilters = NULL;
		qalFilteri = NULL;
		qalFilterf = NULL;
		qalDeleteFilters = NULL;
	}

	Com_Printf("ok\n");

	/* Print OpenAL information */
	Com_Printf("\n");
	QAL_SoundInfo();
	Com_Printf("\n");

	/*  Check extensions */
	if (qalcIsExtensionPresent(device, "ALC_EXT_disconnect") != AL_FALSE)
	{
		hasAlcExtDisconnect = true;
	}


    return true;
}

#endif /* USE_OPENAL */
