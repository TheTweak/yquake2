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
#include <map>

#include "model.h"

class Image {
    std::map<std::string, std::shared_ptr<image_s>> _imageCache;
public:
    image_s* FindImage(char* name, imagetype_t type);
    std::array<float, 4> GetPalleteColor(int, float);
    std::optional<image_s*> LoadM8(char *origname, imagetype_t type);
    std::optional<image_s*> LoadWal(char *origname, imagetype_t type);
    const std::map<std::string, std::shared_ptr<image_s>>& GetLoadedImages() const;
};

#endif /* Image_hpp */
