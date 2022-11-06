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
#include "../src/client/refresh/ref_shared.h"

struct image_s {
    std::string path;
    int width, height;
    byte* data;
    ~image_s();
};

namespace Img {
    image_s* FindImage(char* name, imagetype_t type);
};

#endif /* Image_hpp */
