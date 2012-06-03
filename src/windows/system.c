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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This file is the starting point of the program and implements
 * several support functions and the main loop.
 *
 * =======================================================================
 */

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

#include "../common/header/common.h"
#include "header/conproc.h"
#include "header/resource.h"
#include "header/winquake.h"

#define MAX_NUM_ARGVS 128

int curtime;
int starttime;
qboolean ActiveApp;
qboolean Minimized;

static HANDLE hinput, houtput;
static HANDLE qwclsemaphore;

HINSTANCE global_hInstance;
static HINSTANCE game_library;

unsigned int sys_msg_time;
unsigned int sys_frame_time;

static char console_text[256];
static int console_textlen;  

int argc;
char *argv[MAX_NUM_ARGVS];

/* ================================================================ */

void
Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif
	
	Qcommon_Shutdown();

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);

	MessageBox(NULL, text, "Error", 0 /* MB_OK */);

	if (qwclsemaphore)
	{
		CloseHandle(qwclsemaphore);
	}

	/* shut down QHOST hooks if necessary */
	DeinitConProc();

	exit(1);
}

void
Sys_Quit(void)
{
	timeEndPeriod(1);

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif

	Qcommon_Shutdown();
	CloseHandle(qwclsemaphore);

	if (dedicated && dedicated->value)
	{
		FreeConsole();
	}

	/* shut down QHOST hooks if necessary */
	DeinitConProc();

	exit(0);
}

void
WinError(void)
{
	LPVOID lpMsgBuf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf, 0, NULL);

	/* Display the string. */
	MessageBox(NULL, lpMsgBuf, "GetLastError", MB_OK | MB_ICONINFORMATION);

	/* Free the buffer. */
	LocalFree(lpMsgBuf);
}

/* ================================================================ */

char *
Sys_ScanForCD(void)
{
	static char cddir[MAX_OSPATH];
	static qboolean done;

	char drive[4];
	FILE *f;
	char test[MAX_QPATH];

	if (done) /* don't re-check */
	{
		return cddir;
	}

	/* no abort/retry/fail errors */
	SetErrorMode(SEM_FAILCRITICALERRORS);

	drive[0] = 'c';
	drive[1] = ':';
	drive[2] = '\\';
	drive[3] = 0;

	done = true;

	/* scan the drives */
	for (drive[0] = 'c'; drive[0] <= 'z'; drive[0]++)
	{
		/* where activision put the stuff... */
		sprintf(cddir, "%sinstall\\data", drive);
		sprintf(test, "%sinstall\\data\\quake2.exe", drive);
		f = fopen(test, "r");

		if (f)
		{
			fclose(f);

			if (GetDriveType(drive) == DRIVE_CDROM)
			{
				return cddir;
			}
		}
	}

	cddir[0] = 0;

	return NULL;
}

/* ================================================================ */

void
Sys_Init(void)
{
	OSVERSIONINFO vinfo;

	timeBeginPeriod(1);
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx(&vinfo))
	{
		Sys_Error("Couldn't get OS info");
	}

	/* While Quake II should run on older versions,
	   limit Yamagi Quake II to Windows XP and
	   above. Testing older version would be a
	   PITA. */
	if (!(vinfo.dwMajorVersion > 5) || 
		  ((vinfo.dwMajorVersion == 5) &&
		   (vinfo.dwMinorVersion >= 1)))
	{
		Sys_Error("Yamagi Quake II needs Windows XP or higher!\n");
	}


	if (dedicated->value)
	{
		if (!AllocConsole())
		{
			Sys_Error("Couldn't create dedicated server console");
		}

		hinput = GetStdHandle(STD_INPUT_HANDLE);
		houtput = GetStdHandle(STD_OUTPUT_HANDLE);

		/* let QHOST hook in */
		InitConProc(argc, argv);
	}
}

char *
Sys_ConsoleInput(void)
{
	INPUT_RECORD recs[1024];
	int ch;
   	DWORD dummy, numread, numevents;

	if (!dedicated || !dedicated->value)
	{
		return NULL;
	}

	for ( ; ; )
	{
		if (!GetNumberOfConsoleInputEvents(hinput, &numevents))
		{
			Sys_Error("Error getting # of console events");
		}

		if (numevents <= 0)
		{
			break;
		}

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
		{
			Sys_Error("Error reading console input");
		}

		if (numread != 1)
		{
			Sys_Error("Couldn't read console input");
		}

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);

						if (console_textlen)
						{
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}

						break;

					case '\b':

						if (console_textlen)
						{
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						}

						break;

					default:

						if (ch >= ' ')
						{
							if (console_textlen < sizeof(console_text) - 2)
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}

						break;
				}
			}
		}
	}

	return NULL;
}

void
Sys_ConsoleOutput(char *string)
{
	char text[256];
	DWORD dummy;

	if (!dedicated || !dedicated->value)
	{
		return;
	}

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen + 1] = '\r';
		text[console_textlen + 2] = 0;
		WriteFile(houtput, text, console_textlen + 2, &dummy, NULL);
	}

	WriteFile(houtput, string, strlen(string), &dummy, NULL);

	if (console_textlen)
	{
		WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
	}
}

void
Sys_SendKeyEvents(void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage(&msg, NULL, 0, 0))
		{
			Sys_Quit();
		}

		sys_msg_time = msg.time;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	/* grab frame time */
	sys_frame_time = timeGetTime();
}

char *
Sys_GetClipboardData(void)
{
	char *data = NULL;
	char *cliptext;

	if (OpenClipboard(NULL) != 0)
	{
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData(CF_TEXT)) != 0)
		{
			if ((cliptext = GlobalLock(hClipboardData)) != 0)
			{
				data = malloc(GlobalSize(hClipboardData) + 1);
				strcpy(data, cliptext);
				GlobalUnlock(hClipboardData);
			}
		}

		CloseClipboard();
	}

	return data;
}

/* ================================================================ */

void
Sys_AppActivate(void)
{
	ShowWindow(cl_hwnd, SW_RESTORE);
	SetForegroundWindow(cl_hwnd);
}

/* ================================================================ */

void
Sys_UnloadGame(void)
{
	if (!FreeLibrary(game_library))
	{
		Com_Error(ERR_FATAL, "FreeLibrary failed for game library");
	}

	game_library = NULL;
}

void *
Sys_GetGameAPI(void *parms)
{
	void *(*GetGameAPI)(void *);
	const char *gamename = "game.dll";
	char name[MAX_OSPATH];
	char *path = NULL;

	if (game_library)
	{
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");
	}

	/* now run through the search paths */
	path = NULL;

	while (1)
	{
		path = FS_NextPath(path);

		if (!path)
		{
			return NULL; /* couldn't find one anywhere */
		}

		Com_sprintf(name, sizeof(name), "%s/%s", path, gamename);
		game_library = LoadLibrary(name);

		if (game_library)
		{
			Com_DPrintf("LoadLibrary (%s)\n", name);
			break;
		}
	}

	GetGameAPI = (void *)GetProcAddress(game_library, "GetGameAPI");

	if (!GetGameAPI)
	{
		Sys_UnloadGame();
		return NULL;
	}

	return GetGameAPI(parms);
}

/* ======================================================================= */

void
ParseCommandLine(LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
		{
			lpCmdLine++;
		}

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
			{
				lpCmdLine++;
			}

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}

/* ======================================================================= */

int
Sys_Milliseconds(void)
{
	static int base;
	static qboolean initialized = false;

	if (!initialized)
	{   /* let base retain 16 bits of effectively random data */
		base = timeGetTime() & 0xffff0000;
		initialized = true;
	}

	curtime = timeGetTime() - base;

	return curtime;
} 

/* ======================================================================= */

/*
 * Windows main function. Containts the 
 * initialization code and the main loop
 */
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	int time, oldtime, newtime;
	char *cddir;

	/* Previous instances do not exist in Win32 */
	if (hPrevInstance)
	{
		return 0;
	}

	/* Make the current instance global */
	global_hInstance = hInstance;

	/* Parse the command line arguments */
	ParseCommandLine(lpCmdLine);

	/* Search the CD (for partial installations) */
	cddir = Sys_ScanForCD();

	if (cddir && (argc < MAX_NUM_ARGVS - 3))
	{
		int i;

		/* don't override a cddir on the command line */
		for (i = 0; i < argc; i++)
		{
			if (!strcmp(argv[i], "cddir"))
			{
				break;
			}
		}

		if (i == argc)
		{
			argv[argc++] = "+set";
			argv[argc++] = "cddir";
			argv[argc++] = cddir;
		}
	}

	/* Call the initialization code */
	Qcommon_Init(argc, argv);

	/* Save our time */
	oldtime = Sys_Milliseconds();

	/* The legendary main loop */
	while (1)
	{
		/* If at a full screen console, don't update unless needed */
		if (Minimized || (dedicated && dedicated->value))
		{
			Sleep(1);
		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&msg, NULL, 0, 0))
			{
				Com_Quit();
			}

			sys_msg_time = msg.time;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		}
		while (time < 1);

		/*_controlfp(_PC_24, _MCW_PC); */
		Qcommon_Frame(time);

		oldtime = newtime;
	}

	/* never gets here */
	return TRUE;
}

