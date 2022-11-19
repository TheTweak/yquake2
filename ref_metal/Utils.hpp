//
//  Utils.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 14.11.2022.
//

#ifndef Utils_hpp
#define Utils_hpp

#include <stdio.h>
#include <string>

#include "SharedTypes.h"

#define PIXEL_FORMAT MTL::PixelFormatBGRA8Unorm
#define MAX_FRAMES_IN_FLIGHT 3

#pragma once

namespace Utils {

inline constexpr double pi = 3.14159265358979323846;
float toRadians(float);

};

struct ImageSize {
    int width, height;
};

struct DrawPicCommandData {
    std::string pic;
    TexVertex textureVertex[6];
};

struct DrawParticleCommandData {
    Particle particle;
};

#endif /* Utils_hpp */
