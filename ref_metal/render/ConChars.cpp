//
//  Hud.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 22.12.2022.
//

#include "ConChars.hpp"
#include "../MetalRenderer.hpp"
#include "../texture/TextureCache.hpp"

std::optional<Quad> ConChar::createQuad(vector_uint2 viewportSize) {
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
    
    float halfWidth = scaledSize / 2.0f;
    float halfHeight = scaledSize / 2.0f;
    float offsetX = x + halfWidth - viewportSize[0] / 2.0;
    float offsetY = viewportSize[1] / 2.0 - (y + halfHeight);
    
    Quad vertices;
    //              Pixel positions               Texture coordinates
    vertices[0] = {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }};
    vertices[1] = {{ -halfWidth + offsetX, -halfHeight + offsetY }, { sl, th }};
    vertices[2] = {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }};
    vertices[3] = {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }};
    vertices[4] = {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }};
    vertices[5] = {{  halfWidth + offsetX,  halfHeight + offsetY }, { sh, tl }};
    
    return vertices;
}

ConChars::ConChars(MTL::RenderPipelineState *pipelineState): pipelineState(pipelineState) {}

void ConChars::drawChar(ConChar c) {
    chars.push_back(c);
}

VertexBufferInfo ConChars::render(MTL::RenderCommandEncoder *encoder, vector_uint2 viewportSize) {
    std::vector<TexVertex> vertices;
    for (auto &c: chars) {
        if (auto qopt = c.createQuad(viewportSize); qopt) {
            Quad q = qopt.value();
            for (int i = 0; i < q.size(); i++) {
                vertices.push_back(q[i]);
            }
        }
    }
    if (vertices.empty()) {
        return emptyVertexBufferInfo;
    }
    
    encoder->setRenderPipelineState(pipelineState);
    
    MTL::Buffer *buffer = MetalRenderer::getInstance().getDevice()->newBuffer(vertices.size()*sizeof(TexVertex), MTL::ResourceStorageModeManaged);
    assert(buffer);
    std::memcpy(buffer->contents(), vertices.data(), vertices.size()*sizeof(TexVertex));
    encoder->setVertexBuffer(buffer, 0, TexVertexInputIndex::TexVertexInputIndexVertices);
    
    encoder->setVertexBytes(&viewportSize, sizeof(viewportSize), TexVertexInputIndex::TexVertexInputIndexViewportSize);
    encoder->setFragmentTexture(TextureCache::getInstance().getTexture("conchars"), TextureIndex::TextureIndexBaseColor);
    encoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(vertices.size()));
    
    buffer->autorelease();
    chars.clear();
    
    return emptyVertexBufferInfo;
}
