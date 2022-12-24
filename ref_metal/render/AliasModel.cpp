//
//  AliasModel.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.12.2022.
//

#include "AliasModel.hpp"
#include "../MetalRenderer.hpp"
#include "../texture/TextureCache.hpp"

AliasModel::AliasModel(std::string textureName, std::vector<Vertex> vertices, simd_float4x4 translation, float alpha, MTL::RenderPipelineState *pipelineState): textureName(textureName),
vertices(vertices), translation(translation), alpha(alpha), pipelineState(pipelineState) {}

void AliasModel::setIsTriangle(bool isTriangle) {
    this->isTriangle = isTriangle;
}

void AliasModel::setClamp(bool clamp) {
    this->clamp = clamp;
}

void AliasModel::setMVP(simd_float4x4 mvp) {
    this->mvp = mvp;
}

void AliasModel::render(MTL::RenderCommandEncoder* encoder, vector_uint2 viewportSize) {
    if (vertices.empty()) {
        return;
    }
    
    simd_float4x4 defaultMvp = MetalRenderer::getInstance().getMvpMatrix();
    simd_float4x4 *inputMvp = &defaultMvp;
    if (mvp) {
        inputMvp = &mvp.value();
    }
    
    auto vertexBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(vertices.size()*sizeof(Vertex), MTL::ResourceStorageModeShared);
    assert(vertexBuffer);
    std::memcpy(vertexBuffer->contents(), vertices.data(), vertices.size()*sizeof(Vertex));
    encoder->setVertexBuffer(vertexBuffer, 0, VertexInputIndex::VertexInputIndexVertices);
    encoder->setVertexBytes(inputMvp, sizeof(*inputMvp), VertexInputIndex::VertexInputIndexMVPMatrix);
    encoder->setVertexBytes(&translation, sizeof(translation), VertexInputIndex::VertexInputIndexTransModelMatrix);
    encoder->setVertexBytes(&alpha, sizeof(alpha), VertexInputIndex::VertexInputIndexAlpha);
    encoder->setFragmentTexture(TextureCache::getInstance().getTexture(textureName), TextureIndex::TextureIndexBaseColor);
    encoder->setDepthClipMode(clamp ? MTL::DepthClipModeClamp : MTL::DepthClipModeClip);
    encoder->drawPrimitives(isTriangle ? MTL::PrimitiveTypeTriangle : MTL::PrimitiveTypeLineStrip, NS::UInteger(0), NS::UInteger(vertices.size()));
    encoder->setDepthClipMode(MTL::DepthClipModeClip);
    
    vertexBuffer->release();
}
