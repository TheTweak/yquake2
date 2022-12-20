//
//  System.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 19.12.2022.
//

#ifndef System_hpp
#define System_hpp

#pragma once

#include <stdio.h>

void R_Printf(int level, const char* msg, ...);
void Sys_Error(char *error, ...);
void Com_Printf(char *msg, ...);

#endif /* System_hpp */
