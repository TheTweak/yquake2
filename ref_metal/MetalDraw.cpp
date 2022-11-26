//
//  MetalDraw.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 14.11.2022.
//

#include <exception>
#include <sstream>

#include "MetalDraw.hpp"

MetalDraw::MetalDraw(int sWidth, int sHeight, Img& il) : screenWidth(sWidth), screenHeight(sHeight), imageLoader(il) {}

image_s* MetalDraw::DrawFindPic(char* name) {
    image_s* image;
    char fullname[MAX_QPATH];

    if ((name[0] != '/') && (name[0] != '\\'))
    {
        Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
        image = imageLoader.FindImage(fullname, it_pic);
    }
    else
    {
        image = imageLoader.FindImage(name + 1, it_pic);
    }

    return image;
}

MTL::Texture* _createTexture(int width, int height, MTL::Device* pDevice, byte* data) {
    MTL::TextureDescriptor* pTextureDescriptor = MTL::TextureDescriptor::alloc()->init();
    pTextureDescriptor->setPixelFormat(PIXEL_FORMAT);
    pTextureDescriptor->setWidth(width);
    pTextureDescriptor->setHeight(height);
    pTextureDescriptor->setStorageMode( MTL::StorageModeManaged );
    pTextureDescriptor->setUsage( MTL::ResourceUsageSample | MTL::ResourceUsageRead );
    pTextureDescriptor->setTextureType( MTL::TextureType2D );
    
    MTL::Texture* pFragmentTexture = pDevice->newTexture(pTextureDescriptor);
    MTL::Region region = {
        0, 0, 0,
        ((uint)width), ((uint)height), 1
    };
    pFragmentTexture->replaceRegion(region, 0, data, width*4);
    
    return pFragmentTexture;
}

std::pair<ImageSize, MTL::Texture*> MetalDraw::loadTexture(std::string pic, MTL::Device* pDevice) {
    image_s* image = DrawFindPic(pic.data());
    
    if (!image) {
        std::ostringstream ss;
        ss << "Texture not found: " << pic;
        throw std::logic_error(ss.str());
    }
    
    return {ImageSize{image->width, image->height}, _createTexture(image->width, image->height, pDevice, image->data)};
}

MTL::Texture* MetalDraw::createdColoredTexture(vector_float4 colorBGRA, MTL::Device* pDevice) {
    size_t size = 4 * max(screenWidth, 1) * max(screenHeight, 1);
    auto data = static_cast<byte*>(malloc(size));
    for (int x = 0; x < screenWidth; x++) {
        for (int y = 0; y < screenHeight; y++) {
            int idx = (y * (screenWidth - 1) + x) * 4;
            data[idx] = colorBGRA[0];
            data[idx + 1] = colorBGRA[1];
            data[idx + 2] = colorBGRA[2];
            data[idx + 3] = colorBGRA[3];
        }
    }
    
    return _createTexture(screenWidth, screenHeight, pDevice, data);
}

/*
 Create data for Metal draw commands required to render a rectangle with a texture on it.
 sl, tl, sh, th - parameters naming is borrowed from original ref_gl implementation.
 See "drawTexturedRectangle(...)" method in gl3_draw.c for details.
 */
DrawPicCommandData MetalDraw::createDrawTextureCmdData(const std::string texture, float x, float y, float w, float h,
                                                       float sl, float tl, float sh, float th) {
    float halfWidth = w / 2.0f;
    float halfHeight = h / 2.0f;
    float offsetX = x + halfWidth - screenWidth / 2.0;
    float offsetY = screenHeight / 2.0 - (y + halfHeight);
    
    return {texture, {
        //              Pixel positions               Texture coordinates
        {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }},
        {{ -halfWidth + offsetX, -halfHeight + offsetY }, { sl, th }},
        {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }},

        {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }},
        {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }},
        {{  halfWidth + offsetX,  halfHeight + offsetY }, { sh, tl }},
    }};
}

image_s* MetalDraw::drawFindPic(char* name) {
    image_s* image;
    char fullname[MAX_QPATH];

    if ((name[0] != '/') && (name[0] != '\\'))
    {
        Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
        image = imageLoader.FindImage(fullname, it_pic);
    }
    else
    {
        image = imageLoader.FindImage(name + 1, it_pic);
    }

    return image;
}

void MetalDraw::drawGetPicSize(int *w, int *h, char *name) {
    image_s* image = DrawFindPic(name);
    
    if (!image) {
        *w = *h = -1;
        return;
    }
    
    *w = image->width;
    *h = image->height;
}

std::optional<DrawPicCommandData> MetalDraw::drawCharScaled(int x, int y, int num, float scale) {
    int row, col;
    float frow, fcol, size, scaledSize;
    num &= 255;

    if ((num & 127) == 32)
    {
        return std::nullopt; /* space */
    }

    if (y <= -8)
    {
        return std::nullopt; /* totally off screen */
    }

    row = num >> 4;
    col = num & 15;

    frow = row * 0.0625;
    fcol = col * 0.0625;
    size = 0.0625;

    scaledSize = 8*scale;
        
    float sl = fcol;
    float tl = frow;
    float sh = fcol + size;
    float th = frow + size;
    
    return std::optional<DrawPicCommandData>(createDrawTextureCmdData("conchars", x, y, scaledSize, scaledSize, sl, tl, sh, th));
}
