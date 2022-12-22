//
//  Pic.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include "Pic.hpp"
#include "../texture/TextureCache.hpp"

std::optional<Quad> Pic::createQuad(vector_uint2 viewportSize) {
    ImageSize imageSize = TextureCache::getInstance().getImageSize(pic);
    float halfWidth = imageSize.width * scale / 2.0f;
    float halfHeight = imageSize.height * scale / 2.0f;
    float offsetX = x + halfWidth - viewportSize[0] / 2.0;
    float offsetY = viewportSize[1] / 2.0 - (y + halfHeight);
    const float sl = 0.0f, tl = 0.0f, sh = 1.0f, th = 1.0f;
    
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
