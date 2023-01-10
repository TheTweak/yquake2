//
//  RayTracer.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 08.01.2023.
//

#include "RayTracer.hpp"
#include "../MetalRenderer.hpp"

RayTracer::RayTracer() {
    intersector = MTL::MPSRayIntersector::alloc()->init();
    primitiveASD = MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
    triangleGeometryASD = MTL::AccelerationStructureTriangleGeometryDescriptor::alloc()->init();
    auto array = NS::Array::alloc()->array(primitiveASD);
    primitiveASD->setGeometryDescriptors(array);
}

void RayTracer::rebuildAccelerationStructure(MTL::Buffer *vertexBuffer, int triangleCount) {
    triangleGeometryASD->setVertexBuffer(vertexBuffer);
    triangleGeometryASD->setTriangleCount(triangleCount);
    accelStructure = MetalRenderer::getInstance().getDevice()->newAccelerationStructure(primitiveASD);
    int i = 0;
    i += 1;
}
