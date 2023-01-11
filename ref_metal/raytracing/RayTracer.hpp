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
    bool accelStructureIsBuilt;
public:
    RayTracer();
    void rebuildAccelerationStructure(std::vector<VertexBufferInfo> vertexBuffers);
    void encode(MTL::CommandBuffer *cmdBuffer);
};

#endif /* RayTracer_hpp */
