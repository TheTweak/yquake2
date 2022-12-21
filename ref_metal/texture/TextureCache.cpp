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

std::pair<ImageSize, MTL::Texture*> TextureCache::getFromCache(std::string pic) {
    assert(device != NULL);
    
    if (auto it = data.find(pic); it != data.end()) {
        return it->second;
    }
    
    image_s* image = Image::getInstance().DrawFindPic(pic.data());
    
    if (!image) {
        std::ostringstream ss;
        ss << "Texture not found: " << pic;
        throw std::logic_error(ss.str());
    }
    
    data[pic] = {ImageSize{image->width, image->height}, createTexture(image->width, image->height, image->data)};
    
    return data[pic];
}

MTL::Texture* TextureCache::createTexture(int width, int height, byte* data) {
    MTL::TextureDescriptor* pTextureDescriptor = MTL::TextureDescriptor::alloc()->init();
    pTextureDescriptor->setPixelFormat(PIXEL_FORMAT);
    pTextureDescriptor->setWidth(width);
    pTextureDescriptor->setHeight(height);
    pTextureDescriptor->setStorageMode( MTL::StorageModeShared );
    pTextureDescriptor->setUsage(MTL::ResourceUsageSample | MTL::ResourceUsageRead);
    pTextureDescriptor->setTextureType(MTL::TextureType2D);
    pTextureDescriptor->setMipmapLevelCount(4);
    
    MTL::Texture* pFragmentTexture = device->newTexture(pTextureDescriptor);
    MTL::Region region = {
        0, 0, 0,
        ((uint) width), ((uint) height), 1
    };
    pFragmentTexture->replaceRegion(region, 0, data, width*4);
    
    return pFragmentTexture;
}
