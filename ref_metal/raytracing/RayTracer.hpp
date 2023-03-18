//
//  RayTracer.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 08.01.2023.
//

#ifndef RayTracer_hpp
#define RayTracer_hpp

#include <vector>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include "../render/Renderable.hpp"
#include "../render/WorldGeometry.h"

class RayTracer {
    MTL::Texture *targetTexture;    
    MTL::ComputePipelineState* shadePipeline;
    
    int intersectionType;
    int threadsPerThreadGroupMultiplier = 1;
    size_t targetTextureWidth = 400;
    size_t targetTextureHeight = 300;
        
    MTL::AccelerationStructure* buildAccelerationStructure(MTL::Buffer* vertexBuffer, size_t vertexCount, MTL::CommandQueue* cmdQueue);    
    void shade(MTL::ComputeCommandEncoder *enc, Uniforms uniforms, const WorldGeometry& wg, MTL::CommandQueue* cmdQueue);
public:
    RayTracer();
    void encode(MTL::CommandBuffer *cmdBuffer, Uniforms uniforms, WorldGeometry wg, MTL::CommandQueue* cmdQueue);
    void updateImGui();
    MTL::Texture* getTargetTexture() const;
};

#endif /* RayTracer_hpp */
