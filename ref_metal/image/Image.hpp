//
//  Image.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.09.2022.
//

#pragma once

#include <array>
#include <optional>
#include <map>

#include "../model/model.h"

class Image {
    std::map<std::string, std::shared_ptr<image_s>> _imageCache;
    Image();
public:
    static Image& getInstance();
    image_s* FindImage(char* name, imagetype_t type);
    image_s* FindImage(char* name);
    std::array<float, 4> GetPalleteColor(int, float);
    std::optional<image_s*> LoadM8(char *origname, imagetype_t type);
    std::optional<image_s*> LoadWal(char *origname, imagetype_t type);
    const std::map<std::string, std::shared_ptr<image_s>>& GetLoadedImages() const;
    image_s* DrawFindPic(char* name);
};
