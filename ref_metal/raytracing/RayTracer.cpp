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
    accelStructure = MTL::MPSTriangleAccelerationStructure::alloc()->init();
    rayBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(32, MTL::ResourceStorageModePrivate);
    intersectionBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(32, MTL::ResourceStorageModePrivate);
}

void RayTracer::rebuildAccelerationStructure(std::vector<VertexBufferInfo> vertexBuffers) {
    accelStructure->setVertexBuffer(vertexBuffers[0].first);
    accelStructure->setTriangleCount(vertexBuffers[0].second/3);
    accelStructure->rebuild();
    accelStructureIsBuilt = true;
}

void RayTracer::encode(MTL::CommandBuffer *cmdBuffer) {
    if (!accelStructureIsBuilt) {
        return;
    }
    intersector->encodeIntersection(cmdBuffer,
                                    MTL::MPSIntersectionType::Nearest,
                                    rayBuffer,
                                    0,
                                    intersectionBuffer,
                                    0,
                                    1,
                                    (MTL::MPSAccelerationStructure *) accelStructure);
}
