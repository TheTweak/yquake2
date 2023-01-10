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
public:
    RayTracer();
    void rebuildAccelerationStructure(std::vector<VertexBufferInfo> vertexBuffers);
};

#endif /* RayTracer_hpp */
