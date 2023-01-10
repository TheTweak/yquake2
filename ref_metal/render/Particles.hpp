//
//  RParticle.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 23.12.2022.
//

#ifndef RParticle_hpp
#define RParticle_hpp

#include <vector>

#include "Renderable.hpp"

class Particles : public Renderable {
    std::vector<Particle> particles;
    MTL::RenderPipelineState *pipelineState;
public:
    Particles(MTL::RenderPipelineState *pipelineState);
    void addParticle(Particle p);
    VertexBufferInfo render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) override;
};

#endif /* RParticle_hpp */
