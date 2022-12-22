//
//  Texture2D.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 21.12.2022.
//

#include "Texture2D.hpp"
#include "../texture/TextureCache.hpp"

Texture2D::Texture2D(std::string pic, int x, int y, float scale, MTL::RenderPipelineState *pipelineState): pic(pic), x(x), y(y), scale(scale), pipelineState(pipelineState) {}

std::optional<MTL::DepthStencilState*> Texture2D::getDepthStencilState() {
    return std::nullopt;
}

void Texture2D::render(MTL::RenderCommandEncoder* encoder, vector_uint2 viewportSize) {
    encoder->setRenderPipelineState(pipelineState);
    auto vertices = createQuad(viewportSize);
    if (!vertices) {
        return;
    }
    if (auto depthStencilState = getDepthStencilState(); depthStencilState) {
        encoder->setDepthStencilState(depthStencilState.value());
    }
    encoder->setVertexBytes(&vertices, sizeof(vertices), TexVertexInputIndex::TexVertexInputIndexVertices);
    encoder->setVertexBytes(&viewportSize, sizeof(viewportSize), TexVertexInputIndex::TexVertexInputIndexViewportSize);
    encoder->setFragmentTexture(TextureCache::getInstance().getTexture(pic), TextureIndex::TextureIndexBaseColor);
    encoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
}
