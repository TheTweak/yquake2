//
//  TextureCache.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include <string>
#include <stdio.h>
#include <Metal/Metal.hpp>
#include <unordered_map>
#include <utility>

#include "../utils/Utils.hpp"

class TextureCache {
    std::unordered_map<std::string, std::pair<ImageSize, MTL::Texture*>> data;
    MTL::Device *device;
    
    TextureCache();
    std::pair<ImageSize, MTL::Texture*> getFromCache(std::string);
    MTL::Texture* createTexture(int width, int height, byte* data);
public:
    static TextureCache& getInstance();
    MTL::Texture* getTexture(std::string);    
    MTL::Texture* getFillColorTexture(vector_float4 bgra);
    ImageSize getImageSize(std::string);
    void init(MTL::Device*);
};
