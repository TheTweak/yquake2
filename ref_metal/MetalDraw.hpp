//
//  MetalDraw.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 14.11.2022.
//

#ifndef MetalDraw_hpp
#define MetalDraw_hpp

#include <stdio.h>
#include <MetalKit/MetalKit.hpp>

#include "Utils.hpp"
#include "Image.hpp"
#include "../src/client/vid/header/ref.h"

class MetalDraw {
    const int screenWidth;
    const int screenHeight;
    image_s* DrawFindPic(char* name);
public:
    MetalDraw(int screenWidth, int screenHeight);
    std::pair<ImageSize, MTL::Texture*> loadTexture(std::string pic, MTL::Device* pDevice);
    DrawPicCommandData createDrawTextureCmdData(const std::string texture, float x, float y, float w, float h,
                                                float sl = 0.0f, float tl = 0.0f, float sh = 1.0f, float th = 1.0f);
};

#endif /* MetalDraw_hpp */
