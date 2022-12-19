//
//  MetalDraw.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 14.11.2022.
//

#ifndef MetalDraw_hpp
#define MetalDraw_hpp

#include <stdio.h>
#include <optional>
#include <MetalKit/MetalKit.hpp>

#include "Utils.hpp"
#include "Image.hpp"
#include "../src/client/vid/header/ref.h"

class MetalDraw {
    const int screenWidth;
    const int screenHeight;
    Image& imageLoader;
    image_s* DrawFindPic(char* name);
public:
    MetalDraw(int screenWidth, int screenHeight, Image& imageLoader);
    std::pair<ImageSize, MTL::Texture*> loadTexture(std::string pic, MTL::Device* pDevice);
    MTL::Texture* createdColoredTexture(vector_float4 colorBGRA, MTL::Device* pDevice);
    DrawPicCommandData createDrawTextureCmdData(const std::string texture, float x, float y, float w, float h,
                                                float sl = 0.0f, float tl = 0.0f, float sh = 1.0f, float th = 1.0f);
    image_s* drawFindPic(char* name);
    void drawGetPicSize(int *w, int *h, char *name);
    std::optional<DrawPicCommandData> drawCharScaled(int x, int y, int num, float scale);
    MTL::Texture* createTexture(int width, int height, MTL::Device* pDevice, byte* data);
};

#endif /* MetalDraw_hpp */
