//
//  RParticle.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 23.12.2022.
//

#include "Particles.hpp"
#include "../MetalRenderer.hpp"

Particles::Particles(MTL::RenderPipelineState *pipelineState): pipelineState(pipelineState) {}

void Particles::render(MTL::RenderCommandEncoder *encoder, vector_uint2 viewportSize) {
    if (particles.empty()) {
        return;
    }
    
    encoder->setRenderPipelineState(pipelineState);
    auto pBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(particles.size()*sizeof(Particle), MTL::ResourceStorageModeManaged);
    assert(pBuffer);
    std::memcpy(pBuffer->contents(), particles.data(), particles.size()*sizeof(Particle));
    encoder->setVertexBuffer(pBuffer, 0, ParticleInputIndex::ParticleInputIndexVertices);
    simd_float4x4 mvpMatrix = MetalRenderer::getInstance().getMvpMatrix();
    encoder->setVertexBytes(&mvpMatrix, sizeof(mvpMatrix), ParticleInputIndex::ParticleInputIndexMVPMatrix);
    encoder->setVertexBytes(&matrix_identity_float4x4, sizeof(matrix_identity_float4x4), ParticleInputIndex::ParticleInputIndexTransModelMatrix);
    encoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypePoint, NS::UInteger(0), NS::UInteger(particles.size()));
    particles.clear();
    pBuffer->autorelease();
}

void Particles::addParticle(Particle p) {
    particles.push_back(p);
}
