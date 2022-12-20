//
//  System.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 19.12.2022.
//

#include <cstdarg>

#include "System.hpp"
#include "State.h"

void R_Printf(int level, const char* msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    GAME_API.Com_VPrintf(level, msg, argptr);
    va_end(argptr);
}

void
Sys_Error(char *error, ...)
{
    va_list argptr;
    char text[4096]; // MAXPRINTMSG == 4096
    
    va_start(argptr, error);
    vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);
    
    GAME_API.Sys_Error(ERR_FATAL, "%s", text);
}

void
Com_Printf(char *msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    GAME_API.Com_VPrintf(PRINT_ALL, msg, argptr);
    va_end(argptr);
}
