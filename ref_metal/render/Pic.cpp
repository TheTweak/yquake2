//
//  Pic.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include "Pic.hpp"
#include "../texture/TextureCache.hpp"

Pic::Pic(std::string pic, int x, int y, float scale, MTL::RenderPipelineState *pipelineState): pic(pic), x(x), y(y), scale(scale), pipelineState(pipelineState) {}

void Pic::render(MTL::RenderCommandEncoder* encoder, vector_uint2 viewportSize) {
    ImageSize imageSize = TextureCache::getInstance().getImageSize(pic);
    float halfWidth = imageSize.width * scale / 2.0f;
    float halfHeight = imageSize.height * scale / 2.0f;
    float offsetX = x + halfWidth - viewportSize[0] / 2.0;
    float offsetY = viewportSize[1] / 2.0 - (y + halfHeight);
    const float sl = 0.0f, tl = 0.0f, sh = 1.0f, th = 1.0f;
    
    TexVertex vertices[] = {
        //              Pixel positions               Texture coordinates
        {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }},
        {{ -halfWidth + offsetX, -halfHeight + offsetY }, { sl, th }},
        {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }},
        
        {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }},
        {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }},
        {{  halfWidth + offsetX,  halfHeight + offsetY }, { sh, tl }},
    };
    
    encoder->setRenderPipelineState(pipelineState);
    encoder->setVertexBytes(&vertices, sizeof(vertices), TexVertexInputIndex::TexVertexInputIndexVertices);
    encoder->setVertexBytes(&viewportSize, sizeof(viewportSize), TexVertexInputIndex::TexVertexInputIndexViewportSize);
    encoder->setFragmentTexture(TextureCache::getInstance().getTexture(pic), TextureIndex::TextureIndexBaseColor);
    encoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
}
