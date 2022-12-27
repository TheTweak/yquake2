//
//  SkyBox.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 27.12.2022.
//

#include <sstream>

#include "SkyBox.hpp"
#include "../texture/TextureCache.hpp"
#include "../image/Image.hpp"

static const char* suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static const float skyMin = 1.0 / 512;
static const float skyMax = 511.0 / 512;
static const int skytexorder[6] = {0, 2, 1, 3, 4, 5};

SkyBox::SkyBox(std::string name, float rotate, vec3_t skyAxis): rotate(rotate), name(name) {
    VectorCopy(skyAxis, axis);
        
    for (int i = 0; i < 6; i++) {
        std::ostringstream ss;
        ss << "env/" << name << suf[i] << ".tga";
        
        image_s *image = Image::getInstance().FindImage(ss.str().data(), it_sky);
        
        if (!image) {
            ss.str("");
            ss.clear();
            ss << "pics/Skies/" << name << suf[i] << ".m8";
            image = Image::getInstance().FindImage(ss.str().data(), it_sky);
        }
        
        if (image) {
            textureNames[i] = image->path;
            TextureCache::getInstance().addTextureForImage(image);
        }
    }
}

void SkyBox::render(MTL::RenderCommandEncoder *encoder, vector_uint2 viewportSize) {
    
}
