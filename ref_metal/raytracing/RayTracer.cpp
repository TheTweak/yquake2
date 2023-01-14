//
//  RayTracer.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 08.01.2023.
//

#include "RayTracer.hpp"
#include "../MetalRenderer.hpp"
#include "../utils/Constants.h"

static const size_t rayStride = 48;

RayTracer::RayTracer() {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    intersector = MTL::MPSRayIntersector::alloc()->init();
    accelStructure = MTL::MPSTriangleAccelerationStructure::alloc()->init();
    
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

void RayTracer::rebuildAccelerationStructure(MTL::Buffer *vertexBuffer, size_t vertexCount) {
    accelStructure->setVertexBuffer(vertexBuffer);
    accelStructure->setTriangleCount(vertexCount/3);
    accelStructure->rebuild();
    accelStructureIsBuilt = true;
}

void RayTracer::generateRays(MTL::ComputeCommandEncoder *enc, Uniforms uniforms) {
    MTL::Buffer *uniformsBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(sizeof(Uniforms), MTL::ResourceStorageModeManaged);
    std::memcpy(uniformsBuffer->contents(), &uniforms, sizeof(Uniforms));
    rayBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(rayStride * uniforms.width * uniforms.height,
                                                                    MTL::ResourceStorageModePrivate);
    
    MTL::Texture *randomTexture;
    {
        MTL::TextureDescriptor *td = MTL::TextureDescriptor::alloc()->init();
        td->setWidth(uniforms.width);
        td->setHeight(uniforms.height);
        td->setPixelFormat(MTL::PixelFormatR32Uint);
        td->setUsage(MTL::TextureUsageShaderRead);
        td->setTextureType(MTL::TextureType2D);
        td->setStorageMode(MTL::StorageModeShared);
        randomTexture = MetalRenderer::getInstance().getDevice()->newTexture(td);
        
        uint32_t *randomValues = (uint32_t *) malloc(sizeof(uint32_t) * uniforms.width * uniforms.height);
        for (int i = 0; i < uniforms.width * uniforms.height; i++) {
            randomValues[i] = rand() % (1024 * 1024);
        }
        randomTexture->replaceRegion(MTL::Region(0, 0, uniforms.width, uniforms.height), 0, randomValues, sizeof(uint32_t) * uniforms.width);
        free(randomValues);
        
        td->autorelease();
    }
    MTL::Texture *targetTexture;
    {
        MTL::TextureDescriptor *td = MTL::TextureDescriptor::alloc()->init();
        td->setWidth(uniforms.width);
        td->setHeight(uniforms.height);
        td->setPixelFormat(PIXEL_FORMAT);
        td->setTextureType(MTL::TextureType2D);
        td->setStorageMode(MTL::StorageModePrivate);
        td->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
        targetTexture = MetalRenderer::getInstance().getDevice()->newTexture(td);
        
        td->autorelease();
    }
    
    enc->setBuffer(uniformsBuffer, 0, 0);
    enc->setBuffer(rayBuffer, 0, 1);
    enc->setTexture(randomTexture, 0);
    enc->setTexture(targetTexture, 1);
    enc->setComputePipelineState(genRaysPipeline);
    
    MTL::Size threadsPerThreadgroup = MTL::Size(8, 8, 1);
    MTL::Size threadgroups = MTL::Size((uniforms.width  + threadsPerThreadgroup.width  - 1) / threadsPerThreadgroup.width,
                                       (uniforms.height + threadsPerThreadgroup.height - 1) / threadsPerThreadgroup.height,
                                       1);
    
    enc->dispatchThreads(threadgroups, threadsPerThreadgroup);
    enc->endEncoding();
    
    rayBuffer->autorelease();
    uniformsBuffer->autorelease();
    randomTexture->autorelease();
    targetTexture->autorelease();
}

void RayTracer::encode(MTL::CommandBuffer *cmdBuffer, Uniforms uniforms) {
    if (!accelStructureIsBuilt) {
        return;
    }
    MTL::ComputeCommandEncoder *cenc = cmdBuffer->computeCommandEncoder();
    generateRays(cenc, uniforms);
    intersector->encodeIntersection(cmdBuffer,
                                    MTL::MPSIntersectionType::Nearest,
                                    rayBuffer,
                                    0,
                                    intersectionBuffer,
                                    0,
                                    1,
                                    (MTL::MPSAccelerationStructure *) accelStructure);
}
