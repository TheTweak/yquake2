//
//  Image.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.09.2022.
//

#pragma once

#ifndef Image_hpp
#define Image_hpp

#include <string>
#include <simd/simd.h>
#include "../src/client/refresh/ref_shared.h"

struct image_s {
    std::string path;
    int width, height;
    byte* data;
    ~image_s();
};

namespace Img {
    image_s* FindImage(char* name, imagetype_t type);
    vector_float4 GetPalleteColor(int, float);
};

#endif /* Image_hpp */
