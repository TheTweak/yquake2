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
    MTL::Buffer *rayBuffer;
    MTL::Buffer *triangleMasksBuffer;
    MTL::Buffer *vertexBuffer;
    MTL::Texture *targetTexture;    
    MTL::ComputePipelineState* genRaysPipeline;
    MTL::ComputePipelineState* shadePipeline;
    
    MTL::AccelerationStructure* primitiveAccelerationStructure;
    
    std::vector<MTL::Texture*> shadeTextures;
    std::vector<size_t> vertexTextureIndices;
    size_t shadeTexturesCount = 0;
    
    std::vector<uint32_t> masks;
    
    bool accelStructureIsBuilt;
    int intersectionType;
    int threadsPerThreadGroupMultiplier = 1;
    
    void generateRays(MTL::ComputeCommandEncoder *enc, Uniforms uniforms);
    void shade(MTL::ComputeCommandEncoder *enc, Uniforms uniforms);
public:
    RayTracer();
    void rebuildAccelerationStructure(MTL::Buffer *vertexBuffer, size_t vertexCount, std::vector<MTL::Texture*> shadeTextures,
                                      size_t shadeTexturesCount, std::vector<size_t> vertexTextureIndices, MTL::CommandQueue* cmdQueue);
    void encode(MTL::CommandBuffer *cmdBuffer, Uniforms uniforms);
    void updateImGui();
    MTL::Texture* getTargetTexture() const;
};

#endif /* RayTracer_hpp */
