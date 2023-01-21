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

class RayTracer {
    MTL::MPSRayIntersector *intersector;
    MTL::MPSTriangleAccelerationStructure *accelStructure;
    
    MTL::Buffer *rayBuffer;
    MTL::Buffer *intersectionBuffer;
    MTL::Buffer *triangleMasksBuffer;
    
    MTL::ComputePipelineState* genRaysPipeline;
    MTL::ComputePipelineState* shadePipeline;
    
    std::vector<uint32_t> masks;
    
    bool accelStructureIsBuilt;
    
    void generateRays(MTL::ComputeCommandEncoder *enc, Uniforms uniforms);
    void shade(MTL::ComputeCommandEncoder *enc, Uniforms uniforms);
public:
    RayTracer();
    void rebuildAccelerationStructure(MTL::Buffer *vertexBuffer, size_t vertexCount);
    void encode(MTL::CommandBuffer *cmdBuffer, Uniforms uniforms);
};

#endif /* RayTracer_hpp */
