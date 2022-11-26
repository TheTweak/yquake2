//
//  Image.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.09.2022.
//

#pragma once

#ifndef Image_hpp
#define Image_hpp

#include <array>
#include <optional>

#include "model.h"

namespace Img {
    image_s* FindImage(char* name, imagetype_t type);
    std::array<float, 4> GetPalleteColor(int, float);
    std::optional<image_s*> LoadM8(char *origname, imagetype_t type);
    std::optional<image_s*> LoadWal(char *origname, imagetype_t type);
};

#endif /* Image_hpp */
