//
//  RayTracer.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 08.01.2023.
//

#include "RayTracer.hpp"
#include "../MetalRenderer.hpp"

RayTracer::RayTracer() {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    intersector = MTL::MPSRayIntersector::alloc()->init();
    accelStructure = MTL::MPSTriangleAccelerationStructure::alloc()->init();
    rayBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(32, MTL::ResourceStorageModePrivate);
    intersectionBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(32, MTL::ResourceStorageModePrivate);
    MTL::Library* pLibrary = MetalRenderer::getInstance().getDevice()->newDefaultLibrary();
    using NS::StringEncoding::UTF8StringEncoding;
    {
        MTL::Function* fn = pLibrary->newFunction(NS::String::string("rayKernel", UTF8StringEncoding));
        MTL::ComputePipelineDescriptor *d = MTL::ComputePipelineDescriptor::alloc()->init();
        d->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(true);
        d->setComputeFunction(fn);
        d->setLabel(NS::String::string("rayKernel", UTF8StringEncoding));
        
        NS::Error* pError = nullptr;
        genRaysPipeline = MetalRenderer::getInstance().getDevice()->newComputePipelineState(d, MTL::PipelineOptionNone, NULL, &pError);
        if (!genRaysPipeline) {
            __builtin_printf("%s", pError->localizedDescription()->utf8String());
            assert(false);
        }
        
        fn->release();
        d->release();
    }
    pLibrary->release();
    pPool->release();
}

void RayTracer::rebuildAccelerationStructure(std::vector<VertexBufferInfo> vertexBuffers) {
    accelStructure->setVertexBuffer(vertexBuffers[0].first);
    accelStructure->setTriangleCount(vertexBuffers[0].second/3);
    accelStructure->rebuild();
    accelStructureIsBuilt = true;
}

void RayTracer::generateRays() {
    
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
