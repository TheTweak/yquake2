//
//  RayTracer.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 08.01.2023.
//

#include "RayTracer.hpp"
#include "../MetalRenderer.hpp"
#include "../utils/Constants.h"

MTL::ComputePipelineState *createPipeline(std::string name, MTL::Library *pLibrary) {
    using NS::StringEncoding::UTF8StringEncoding;
    
    MTL::Function* fn = pLibrary->newFunction(NS::String::string(name.data(), UTF8StringEncoding));
    MTL::ComputePipelineDescriptor *d = MTL::ComputePipelineDescriptor::alloc()->init();
    d->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(true);
    d->setComputeFunction(fn);
    d->setLabel(NS::String::string(name.data(), UTF8StringEncoding));
    
    NS::Error* pError = nullptr;
    auto result = MetalRenderer::getInstance().getDevice()->newComputePipelineState(d, MTL::PipelineOptionNone, NULL, &pError);
    if (!result) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }
    
    fn->release();
    d->release();
    
    return result;
}

RayTracer::RayTracer() {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    MTL::Library* pLibrary = MetalRenderer::getInstance().getDevice()->newDefaultLibrary();
    genRaysPipeline = createPipeline("rayKernel", pLibrary);
    shadePipeline = createPipeline("shadeKernel", pLibrary);
    
    pLibrary->release();
    pPool->release();
}

MTL::Texture* RayTracer::getTargetTexture() const {
    return targetTexture;
}

MTL::AccelerationStructure* newAccelerationStructure(MTL::PrimitiveAccelerationStructureDescriptor* desc,
                                                     MTL::CommandQueue* cmdQueue) {
    MTL::AccelerationStructureSizes sizes = MetalRenderer::getInstance().getDevice()->accelerationStructureSizes(desc);
    MTL::AccelerationStructure* accelStructure = MetalRenderer::getInstance().getDevice()->newAccelerationStructure(sizes.accelerationStructureSize);
    
    MTL::Buffer* scratchBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(sizes.buildScratchBufferSize, MTL::ResourceStorageModePrivate);
    MTL::CommandBuffer* cmdBuffer = cmdQueue->commandBuffer();
    MTL::AccelerationStructureCommandEncoder* enc = cmdBuffer->accelerationStructureCommandEncoder();
//    MTL::Buffer* compactedSizeBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(sizeof(uint32_t), MTL::ResourceStorageModeShared);
    
    enc->buildAccelerationStructure(accelStructure, desc, scratchBuffer, 0);
//    enc->writeCompactedAccelerationStructureSize(accelStructure, compactedSizeBuffer, 0);
    enc->endEncoding();
    cmdBuffer->commit();
    
//    scratchBuffer->autorelease();
//    enc->autorelease();
//    cmdBuffer->autorelease();
    
    return accelStructure;
}

void RayTracer::rebuildAccelerationStructure(MTL::Buffer *vertexBuffer, size_t vertexCount, std::vector<MTL::Texture*> shadeTextures,
                                             size_t shadeTexturesCount, std::vector<size_t> vertexTextureIndices, MTL::CommandQueue* cmdQueue) {
    for (int i = 0; i < vertexCount/3; i++) {
        masks.push_back(TRIANGLE_MASK_GEOMETRY);
    }
    triangleMasksBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(masks.size() * sizeof(uint32_t), MTL::ResourceStorageModeManaged);
    std::memcpy(triangleMasksBuffer->contents(), masks.data(), masks.size() * sizeof(uint32_t));
        
    MTL::AccelerationStructureTriangleGeometryDescriptor *gd = MTL::AccelerationStructureTriangleGeometryDescriptor::alloc()->init();
    gd->setVertexBuffer(vertexBuffer);
    gd->setTriangleCount(vertexCount / 3);
    gd->setVertexStride(sizeof(Vertex));
    CFTypeRef descriptors[] = { (CFTypeRef)(gd) };
    NS::Array* pGeoDescriptors = (NS::Array*)(CFArrayCreate(kCFAllocatorDefault, descriptors, sizeof(descriptors)/sizeof(gd), &kCFTypeArrayCallBacks));
    
    MTL::PrimitiveAccelerationStructureDescriptor *pd = MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
    pd->setGeometryDescriptors(pGeoDescriptors);
    primitiveAccelerationStructure = newAccelerationStructure(pd, cmdQueue);
    accelStructureIsBuilt = true;
    
    this->vertexBuffer = vertexBuffer;
    this->shadeTextures = std::move(shadeTextures);
    this->shadeTexturesCount = shadeTexturesCount;
    this->vertexTextureIndices = vertexTextureIndices;
    
    triangleMasksBuffer->autorelease();
    pd->autorelease();
    gd->autorelease();
}

void RayTracer::generateRays(MTL::ComputeCommandEncoder *enc, Uniforms uniforms) {
    MTL::Buffer *uniformsBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(sizeof(Uniforms), MTL::ResourceStorageModeManaged);
    std::memcpy(uniformsBuffer->contents(), &uniforms, sizeof(Uniforms));
    rayBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(RAY_STRIDE * uniforms.width * uniforms.height, MTL::ResourceStorageModePrivate);
    
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
    
    MTL::Size threadsPerThreadgroup = MTL::Size(this->threadsPerThreadGroupMultiplier * 32, 1, 1);
    MTL::Size threadgroups = MTL::Size(uniforms.width, uniforms.height, 1);

    enc->dispatchThreads(threadgroups, threadsPerThreadgroup);
    enc->endEncoding();
    
    rayBuffer->autorelease();
    uniformsBuffer->autorelease();
    randomTexture->autorelease();
}

void RayTracer::shade(MTL::ComputeCommandEncoder *enc, Uniforms uniforms) {
    enc->setAccelerationStructure(primitiveAccelerationStructure, 0);
    enc->setComputePipelineState(shadePipeline);
    enc->setTexture(targetTexture, 0);
    enc->setBytes(&uniforms, sizeof(uniforms), 2);    
    enc->setBuffer(rayBuffer, 0, 1);    
    enc->setBuffer(vertexBuffer, 0, 3);
    
    MTL::Buffer *vertexTextureIndicesBuffer = MetalRenderer::getInstance().getDevice()->newBuffer(sizeof(size_t) * vertexTextureIndices.size(), MTL::ResourceStorageModeManaged);
    std::memcpy(vertexTextureIndicesBuffer->contents(), vertexTextureIndices.data(), sizeof(size_t) * vertexTextureIndices.size());
    enc->setBuffer(vertexTextureIndicesBuffer, 0, 4);
    vertexTextureIndicesBuffer->autorelease();
    
    if (shadeTexturesCount > 0) {
        for (int i = 0; i < shadeTexturesCount; i++) {
            enc->setTexture(shadeTextures.at(i), i + 1);
        }
    }
    
    MTL::Size threadsPerThreadgroup = MTL::Size(this->threadsPerThreadGroupMultiplier * 32, 1, 1);
    MTL::Size threadgroups = MTL::Size(uniforms.width, uniforms.height, 1);
    
    enc->dispatchThreads(threadgroups, threadsPerThreadgroup);
    enc->endEncoding();
}

void RayTracer::updateImGui() {
    const char* intersectionTypes[] = { "any", "nearest" };
    ImGui::Combo("intersection type##comboIntersectionType", &intersectionType, intersectionTypes, IM_ARRAYSIZE(intersectionTypes));
    ImGui::SliderInt("threads per thread group multiplier", &threadsPerThreadGroupMultiplier, 1, 8);
}

void RayTracer::encode(MTL::CommandBuffer *cmdBuffer, Uniforms uniforms) {
    if (!accelStructureIsBuilt) {
        return;
    }

    MTL::ComputeCommandEncoder *cenc = cmdBuffer->computeCommandEncoder();
    generateRays(cenc, uniforms);
    
    cenc = cmdBuffer->computeCommandEncoder();
                    
    shade(cenc, uniforms);
    
//    primitiveAccelerationStructure->autorelease();
    targetTexture->autorelease();
}
