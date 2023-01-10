//
//  RayTracer.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 08.01.2023.
//

#ifndef RayTracer_hpp
#define RayTracer_hpp

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

class RayTracer {
    MTL::MPSRayIntersector *intersector;
    MTL::AccelerationStructure *accelStructure;
    MTL::PrimitiveAccelerationStructureDescriptor *primitiveASD;
    MTL::AccelerationStructureTriangleGeometryDescriptor *triangleGeometryASD;
public:
    RayTracer();
    void rebuildAccelerationStructure(MTL::Buffer *vertexBuffer, int triangleCount);
};

#endif /* RayTracer_hpp */
