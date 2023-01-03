//
//  AliasModel.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.12.2022.
//

#include "Polygon.hpp"
#include "../MetalRenderer.hpp"
#include "../texture/TextureCache.hpp"

Polygon::Polygon(std::string textureName, simd_float4x4 translation, float alpha, MTL::RenderPipelineState *pipelineState): textureName(textureName), translation(translation), alpha(alpha), pipelineState(pipelineState) {}

void Polygon::addVertex(Vertex v) {
    vertices.push_back(v);
}

void Polygon::setIsTriangle(bool isTriangle) {
    this->triangle = isTriangle;
}

bool Polygon::isTriangle() {
    return triangle;
}

void Polygon::setClamp(bool clamp) {
    this->clamp = clamp;
}

void Polygon::setMVP(simd_float4x4 mvp) {
    this->mvp = mvp;
}

void Polygon::render(MTL::RenderCommandEncoder* encoder, vector_uint2 viewportSize) {
    if (vertices.empty()) {
        return;
    }
    
    simd_float4x4 defaultMvp = MetalRenderer::getInstance().getMvpMatrix();
    simd_float4x4 *inputMvp = &defaultMvp;
    if (mvp) {
        inputMvp = &mvp.value();
    }
    
    encoder->setRenderPipelineState(pipelineState);
    auto vertexBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(vertices.size()*sizeof(Vertex), MTL::ResourceStorageModeShared);
    assert(vertexBuffer);
    std::memcpy(vertexBuffer->contents(), vertices.data(), vertices.size()*sizeof(Vertex));
    encoder->setVertexBuffer(vertexBuffer, 0, VertexInputIndex::VertexInputIndexVertices);
    encoder->setVertexBytes(inputMvp, sizeof(*inputMvp), VertexInputIndex::VertexInputIndexMVPMatrix);
    encoder->setVertexBytes(&translation, sizeof(translation), VertexInputIndex::VertexInputIndexTransModelMatrix);
    encoder->setVertexBytes(&alpha, sizeof(alpha), VertexInputIndex::VertexInputIndexAlpha);
    encoder->setFragmentTexture(TextureCache::getInstance().getTexture(textureName), TextureIndex::TextureIndexBaseColor);    
    encoder->setFragmentBytes(&clamp, sizeof(clamp), TextureIndex::TextureIndexScaleDepth);
    encoder->setDepthClipMode(clamp ? MTL::DepthClipModeClamp : MTL::DepthClipModeClip);
    encoder->drawPrimitives(triangle ? MTL::PrimitiveTypeTriangle : MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(vertices.size()));
    encoder->setDepthClipMode(MTL::DepthClipModeClip);
    
    vertexBuffer->release();
}
