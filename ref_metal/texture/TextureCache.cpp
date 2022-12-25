//
//  TextureCache.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include <sstream>

#include "TextureCache.hpp"
#include "../image/Image.hpp"
#include "../utils/Constants.h"
#include "../MetalRenderer.hpp"

TextureCache::TextureCache() {}

TextureCache& TextureCache::getInstance() {
    static TextureCache instance;
    return instance;
}

void TextureCache::init(MTL::Device *device) {
    this->device = device;
}

MTL::Texture* TextureCache::getTexture(std::string pic) {
    return getFromCache(pic).second;
}

ImageSize TextureCache::getImageSize(std::string pic) {
    return getFromCache(pic).first;
}

byte *createFillColor(vector_float4 bgra, int screenWidth, int screenHeight) {
    size_t size = 4 * max(screenWidth, 1) * max(screenHeight, 1);
    auto data = static_cast<byte*>(malloc(size));
    assert(data);
    for (int x = 0; x < screenWidth; x++) {
        for (int y = 0; y < screenHeight; y++) {
            int idx = (y * (screenWidth - 1) + x) * 4;
            data[idx] = bgra[0];
            data[idx + 1] = bgra[1];
            data[idx + 2] = bgra[2];
            data[idx + 3] = bgra[3];
        }
    }
    return data;
}

MTL::Texture* TextureCache::getFillColorTexture(vector_float4 bgra) {
    assert(device != NULL);
    
    int screenWidth = MetalRenderer::getInstance().getScreenWidth();
    int screenHeight = MetalRenderer::getInstance().getScreenHeight();
    
    std::ostringstream os;
    os << FILL_TEXTURE << bgra[0] << bgra[1] << bgra[2] << bgra[3] << screenWidth << screenHeight;
    auto k = os.str();
    
    if (auto it = data.find(k); it != data.end()) {
        return it->second.second;
    }
    
    byte *image = createFillColor(bgra, screenWidth, screenHeight);
    
    data[k] = {ImageSize{screenWidth, screenHeight}, createTexture(screenWidth, screenHeight, image, k, 0)};
    return data[k].second;
}

std::pair<ImageSize, MTL::Texture*> TextureCache::getFromCache(std::string pic) {
    if (auto it = data.find(pic); it != data.end()) {
        return it->second;
    }
    
    image_s* image = Image::getInstance().DrawFindPic(pic.data());
    
    if (!image) {
        std::ostringstream ss;
        ss << "Texture not found: " << pic;
        throw std::logic_error(ss.str());
    }
    
    int mipmapLevelCount = std::log2(image->width);
    
    data[pic] = {ImageSize{image->width, image->height}, createTexture(image->width, image->height, image->data, pic, mipmapLevelCount)};
    
    return data[pic];
}

MTL::Texture* TextureCache::createTexture(int width, int height, byte* data, std::string name, int mipmapLevelCount) {
    assert(device != NULL);
    
    MTL::TextureDescriptor* pTextureDescriptor = MTL::TextureDescriptor::alloc()->init();
    pTextureDescriptor->setPixelFormat(PIXEL_FORMAT);
    pTextureDescriptor->setWidth(width);
    pTextureDescriptor->setHeight(height);
    pTextureDescriptor->setStorageMode( MTL::StorageModeShared );
    pTextureDescriptor->setUsage(MTL::ResourceUsageSample | MTL::ResourceUsageRead);
    pTextureDescriptor->setTextureType(MTL::TextureType2D);
    if (mipmapLevelCount > 1) {
        pTextureDescriptor->setMipmapLevelCount(mipmapLevelCount);
    }
    
    MTL::Texture* pFragmentTexture = device->newTexture(pTextureDescriptor);
    MTL::Region region = {
        0, 0, 0,
        ((uint) width), ((uint) height), 1
    };
    pFragmentTexture->replaceRegion(region, 0, data, width*4);
    pFragmentTexture->setLabel(NS::String::string(name.data(), NS::StringEncoding::UTF8StringEncoding));
    
    return pFragmentTexture;
}

void TextureCache::addTextureForImage(image_s *skin) {
    if (auto it = data.find(skin->path); it != data.end()) {
        return;
    }
    
    int mipmapLevelCount = std::log2(skin->width);
    
    data[skin->path] = {ImageSize{skin->width, skin->height}, createTexture(skin->width, skin->height, skin->data, skin->path, mipmapLevelCount)};
}

const std::unordered_map<std::string, std::pair<ImageSize, MTL::Texture*>>& TextureCache::getData() const {
    return data;
};
