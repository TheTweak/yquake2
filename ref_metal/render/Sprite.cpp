//
//  Pic.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include "Sprite.hpp"
#include "../texture/TextureCache.hpp"

std::optional<Quad> Sprite::createQuad(vector_uint2 viewportSize) {
    ImageSize imageSize = TextureCache::getInstance().getImageSize(pic);
    float halfWidth = w == 0 ? imageSize.width * scale / 2.0f : w / 2.0f;
    float halfHeight = h == 0 ? imageSize.height * scale / 2.0f : h / 2.0f;
    float offsetX = x + halfWidth - viewportSize[0] / 2.0;
    float offsetY = viewportSize[1] / 2.0 - (y + halfHeight);
    
    std::array<TexVertex, 6> vertices;
    //                          Pixel positions               Texture coordinates
    vertices[0] = {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }};
    vertices[1] = {{ -halfWidth + offsetX, -halfHeight + offsetY }, { sl, th }};
    vertices[2] = {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }};
    vertices[3] = {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }};
    vertices[4] = {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }};
    vertices[5] = {{  halfWidth + offsetX,  halfHeight + offsetY }, { sh, tl }};
    
    return vertices;
}
