//
//  Pic.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include "Sprite.hpp"
#include "../texture/TextureCache.hpp"

Sprite::Sprite(std::string pic, MTL::RenderPipelineState *pipelineState): TexturedRectangle(pic, 0, 0, 0, pipelineState) {}

void Sprite::setQuad(Quad q) {
    this->quad = q;
}

std::optional<Quad> Sprite::createQuad(vector_uint2 viewportSize) {
    if (quad) {
        return quad;
    }
    
    float halfWidth, halfHeight;
    if (w == 0 && h == 0) {
        ImageSize imageSize = TextureCache::getInstance().getImageSize(pic);
        halfWidth = imageSize.width * scale / 2.0f;
        halfHeight = imageSize.height * scale / 2.0f;
    } else {
        halfWidth = w / 2.0f;
        halfHeight = h / 2.0f;
    }
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
