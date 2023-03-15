//
//  imgui_impl_metal.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 08.02.2023.
//

#include <chrono>
#include <simd/simd.h>

#include "imgui_impl_metal.h"
#include "../utils/Constants.h"

ImGui_ImplMetal_Data::ImGui_ImplMetal_Data() {
    SharedMetalContext = new MetalContext();
}

ImGui_ImplMetal_Data* ImGui_ImplMetal_CreateBackendData() {
    return new ImGui_ImplMetal_Data();
}

ImGui_ImplMetal_Data* ImGui_ImplMetal_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplMetal_Data*) ImGui::GetIO().BackendRendererUserData : NULL;
}

bool ImGui_ImplMetal_Init(MTL::Device* device) {
    ImGui_ImplMetal_Data *bd = ImGui_ImplMetal_CreateBackendData();
    ImGuiIO &io = ImGui::GetIO();
    io.BackendRendererUserData = (void *) bd;
    io.BackendRendererName = "imgui_impl_metal";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    
    bd->SharedMetalContext = new MetalContext();
    bd->SharedMetalContext->device = device;
    
    return true;
}

void ImGui_ImplMetal_Shutdown() {}

void ImGui_ImplMetal_NewFrame(MTL::RenderPassDescriptor* renderPassDescriptor) {
    auto *bd = ImGui_ImplMetal_GetBackendData();
    IM_ASSERT(bd && bd->SharedMetalContext != nil && "No Metal context. Did you call ImGui_ImplMetal_Init() ?");
    
    bd->SharedMetalContext->frameBufferDescriptor = new FramebufferDescriptor(renderPassDescriptor);
    
    if (!bd->SharedMetalContext->depthStencilState) {
        ImGui_ImplMetal_CreateDeviceObjects(bd->SharedMetalContext->device);
    }
}

void ImGui_ImplMetal_SetupRenderState(ImDrawData *drawData,
                                      MTL::CommandBuffer *commandBuffer,
                                      MTL::RenderCommandEncoder *commandEncoder,
                                      MTL::RenderPipelineState *renderPipelineState,
                                      MTL::Buffer *vertexBuffer,
                                      size_t vertexBufferOffset) {
    auto *bd = ImGui_ImplMetal_GetBackendData();
    
    commandEncoder->setCullMode(MTL::CullModeNone);
    commandEncoder->setDepthStencilState(bd->SharedMetalContext->depthStencilState);
    
    /*
    MTL::Viewport viewport = MTL::Viewport();
    viewport.originX = 0.0;
    viewport.originY = 0.0;
    viewport.width = drawData->DisplaySize.x * drawData->FramebufferScale.x;
    viewport.height = drawData->DisplaySize.y * drawData->FramebufferScale.y;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    
    commandEncoder->setViewport(viewport);
    
    float L = drawData->DisplayPos.x;
    float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T = drawData->DisplayPos.y;
    float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
    float N = (float)viewport.znear;
    float F = (float)viewport.zfar;
    
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,   0.0f },
        { 0.0f,         0.0f,        1/(F-N),   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T), N/(F-N),   1.0f },
    };
    
    commandEncoder->setVertexBytes(&ortho_projection, sizeof(ortho_projection), 1);
    */
    vector_uint2 viewportSize = {static_cast<unsigned int>(drawData->DisplaySize.x), static_cast<unsigned int>(drawData->DisplaySize.y)};
    commandEncoder->setVertexBytes(&viewportSize, sizeof(viewportSize), 1);
    commandEncoder->setRenderPipelineState(renderPipelineState);
    commandEncoder->setVertexBuffer(vertexBuffer, 0, 0);
    commandEncoder->setVertexBufferOffset(vertexBufferOffset, 0);
}

void ImGui_ImplMetal_RenderDrawData(ImDrawData *drawData,
                                                   MTL::CommandBuffer* commandBuffer,
                                    MTL::RenderCommandEncoder* commandEncoder) {
    auto *bd = ImGui_ImplMetal_GetBackendData();
    auto *ctx = bd->SharedMetalContext;
    
    int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0 || drawData->CmdListsCount == 0) {
        return;
    }
    
    MTL::RenderPipelineState *renderPipelineState = ctx->renderPipelineStateCache[*ctx->frameBufferDescriptor];
    if (!renderPipelineState) {
        renderPipelineState = ctx->renderPipelineState(*ctx->frameBufferDescriptor, commandBuffer->device());
        
        ctx->renderPipelineStateCache[*ctx->frameBufferDescriptor] = renderPipelineState;
    }
    
    size_t vertexBufferLength = (size_t)drawData->TotalVtxCount * sizeof(ImDrawVert);
    size_t indexBufferLength = (size_t)drawData->TotalIdxCount * sizeof(ImDrawIdx);
    
    MetalBuffer *vertexBuffer = ctx->dequeueReusableBuffer(vertexBufferLength, commandBuffer->device());
    MetalBuffer *indexBuffer = ctx->dequeueReusableBuffer(indexBufferLength, commandBuffer->device());
    
    ImGui_ImplMetal_SetupRenderState(drawData, commandBuffer, commandEncoder, renderPipelineState, vertexBuffer->buffer, 0);
    
    ImVec2 clip_off = drawData->DisplayPos;
    ImVec2 clip_scale = drawData->FramebufferScale;
    
    size_t vertexBufferOffset = 0;
    size_t indexBufferOffset = 0;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[n];

        std::memcpy((char*)vertexBuffer->buffer->contents() + vertexBufferOffset, cmd_list->VtxBuffer.Data, (size_t)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        std::memcpy((char*)indexBuffer->buffer->contents() + indexBufferOffset, cmd_list->IdxBuffer.Data, (size_t)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplMetal_SetupRenderState(drawData, commandBuffer, commandEncoder, renderPipelineState, vertexBuffer->buffer, vertexBufferOffset);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as setScissorRect() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;
                if (pcmd->ElemCount == 0) // drawIndexedPrimitives() validation doesn't accept this
                    continue;

                /*
                // Apply scissor/clipping rectangle
                MTL::ScissorRect scissorRect =
                {
                    .x = NS::UInteger(clip_min.x),
                    .y = NS::UInteger(clip_min.y),
                    .width = NS::UInteger(clip_max.x - clip_min.x),
                    .height = NS::UInteger(clip_max.y - clip_min.y)
                };
                commandEncoder->setScissorRect(scissorRect);
                 */
                
                // Bind texture, Draw
                if (ImTextureID tex_id = pcmd->GetTexID()) {
                    commandEncoder->setFragmentTexture((MTL::Texture*) tex_id, 0);
                }

                commandEncoder->setVertexBufferOffset(vertexBufferOffset + pcmd->VtxOffset * sizeof(ImDrawVert), 0);
                commandEncoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32, indexBuffer->buffer, indexBufferOffset + pcmd->IdxOffset * sizeof(ImDrawIdx));
            }
        }

        vertexBufferOffset += (size_t)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
        indexBufferOffset += (size_t)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
        
        commandBuffer->addCompletedHandler([vertexBuffer, indexBuffer](MTL::CommandBuffer *cmdBuffer) {
            auto *bd = ImGui_ImplMetal_GetBackendData();
            bd->SharedMetalContext->bufferCache.push_back(vertexBuffer);
            bd->SharedMetalContext->bufferCache.push_back(indexBuffer);
        });
    }
}

bool ImGui_ImplMetal_CreateFontsTexture(MTL::Device *device) {
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    // We are retrieving and uploading the font atlas as a 4-channels RGBA texture here.
    // In theory we could call GetTexDataAsAlpha8() and upload a 1-channel texture to save on memory access bandwidth.
    // However, using a shader designed for 1-channel texture would make it less obvious to use the ImTextureID facility to render users own textures.
    // You can make that change in your implementation.
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    auto *textureDescriptor = MTL::TextureDescriptor::texture2DDescriptor(PIXEL_FORMAT, width, height, false);
    textureDescriptor->setUsage(MTL::TextureUsageShaderRead);
    textureDescriptor->setStorageMode(MTL::StorageModeManaged);
    auto *texture = device->newTexture(textureDescriptor);
    texture->replaceRegion(MTL::Region(0, 0, width, height), 0, pixels, width * 4);
    bd->SharedMetalContext->fontTexture = texture;
    io.Fonts->SetTexID((void*)bd->SharedMetalContext->fontTexture);
    return (bd->SharedMetalContext->fontTexture != NULL);
}

void ImGui_ImplMetal_DestroyFontsTexture() {
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    bd->SharedMetalContext->fontTexture = NULL;
    io.Fonts->SetTexID(NULL);
}

bool ImGui_ImplMetal_CreateDeviceObjects(MTL::Device *device) {
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    MTL::DepthStencilDescriptor *depthStencilDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depthStencilDescriptor->setDepthWriteEnabled(false);
    depthStencilDescriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
    bd->SharedMetalContext->depthStencilState = device->newDepthStencilState(depthStencilDescriptor);
    ImGui_ImplMetal_CreateFontsTexture(device);
    return true;
}

void ImGui_ImplMetal_DestroyDeviceObjects() {
    ImGui_ImplMetal_Data* bd = ImGui_ImplMetal_GetBackendData();
    ImGui_ImplMetal_DestroyFontsTexture();
    bd->SharedMetalContext->renderPipelineStateCache.clear();
}

MetalBuffer::MetalBuffer(MTL::Buffer *buffer) {
    this->buffer = buffer;
    this->lastReuseTime = std::chrono::system_clock::now().time_since_epoch().count();
}

FramebufferDescriptor::FramebufferDescriptor(MTL::RenderPassDescriptor *rpd) {
    sampleCount = rpd->colorAttachments()->object(0)->texture()->sampleCount();
    colorPixelFormat = rpd->colorAttachments()->object(0)->texture()->pixelFormat();
    depthPixelFormat = rpd->depthAttachment()->texture()->pixelFormat();
    stencilPixelFormat = rpd->stencilAttachment()->texture()->pixelFormat();
}

size_t FramebufferDescriptorHash::operator()(const FramebufferDescriptor &key) const {
    int sc = key.sampleCount & 0x3;
    int cf = key.colorPixelFormat & 0x3FF;
    int df = key.depthPixelFormat & 0x3FF;
    int sf = key.stencilPixelFormat & 0x3FF;
    size_t hash = (sf << 22) | (df << 12) | (cf << 2) | sc;
    return hash;
}

bool FramebufferDescriptor::operator==(const FramebufferDescriptor &other) const {
    return std::make_tuple(sampleCount, colorPixelFormat, depthPixelFormat, stencilPixelFormat) == std::make_tuple(other.sampleCount, other.colorPixelFormat, other.depthPixelFormat, other.stencilPixelFormat);
}

MetalContext::MetalContext() {
    lastBufferCachePurge = std::chrono::system_clock::now().time_since_epoch().count();
}

MetalBuffer* MetalContext::dequeueReusableBuffer(size_t length, MTL::Device* device) {
    size_t now = std::chrono::system_clock::now().time_since_epoch().count();
    
    if (now - lastBufferCachePurge > 1) {
        std::vector<MetalBuffer*> survivors;
        for (auto *candidate : bufferCache) {
            if (candidate->lastReuseTime > lastBufferCachePurge) {
                survivors.push_back(candidate);
            }
        }
        bufferCache = survivors;
        lastBufferCachePurge = now;
    }
    
    MetalBuffer *bestCandidate = NULL;
    for (auto *candidate : bufferCache) {
        if (candidate->buffer->length() >= length &&
            (bestCandidate == NULL || bestCandidate->lastReuseTime > candidate->lastReuseTime)) {
            bestCandidate = candidate;
        }
    }
    
    if (bestCandidate) {
        auto it = std::remove(bufferCache.begin(), bufferCache.end(), bestCandidate);
        bufferCache.erase(it, bufferCache.end());
        return bestCandidate;
    }
    
    auto *b = device->newBuffer(length, MTL::ResourceStorageModeShared);
    return new MetalBuffer(b);
}

MTL::RenderPipelineState* MetalContext::renderPipelineState(FramebufferDescriptor fbDescriptor, MTL::Device *device) {
    
    using NS::StringEncoding::UTF8StringEncoding;

    MTL::Library* pLibrary = device->newDefaultLibrary();
    
    MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexShader2D", UTF8StringEncoding) );
    MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("samplingShader2D", UTF8StringEncoding) );
            
    MTL::VertexDescriptor *vertexDescriptor = MTL::VertexDescriptor::alloc()->init();

    vertexDescriptor->attributes()->object(0)->setOffset(IM_OFFSETOF(ImDrawVert, pos));
    vertexDescriptor->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2); // position
    vertexDescriptor->attributes()->object(0)->setBufferIndex(0);
    vertexDescriptor->attributes()->object(1)->setOffset(IM_OFFSETOF(ImDrawVert, uv));
    vertexDescriptor->attributes()->object(1)->setFormat(MTL::VertexFormatFloat2); // texCoords
    vertexDescriptor->attributes()->object(1)->setBufferIndex(0);
    vertexDescriptor->attributes()->object(2)->setOffset(IM_OFFSETOF(ImDrawVert, col));
    vertexDescriptor->attributes()->object(2)->setFormat(MTL::VertexFormatUChar4); // color
    vertexDescriptor->attributes()->object(2)->setBufferIndex(0);
    vertexDescriptor->layouts()->object(0)->setStepRate(1);
    vertexDescriptor->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDescriptor->layouts()->object(0)->setStride(sizeof(ImDrawVert));

    MTL::RenderPipelineDescriptor* pipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDescriptor->setVertexFunction(pVertexFn);
    pipelineDescriptor->setFragmentFunction(pFragFn);
    pipelineDescriptor->setVertexDescriptor(vertexDescriptor);
    pipelineDescriptor->setRasterSampleCount(frameBufferDescriptor->sampleCount);
    pipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(frameBufferDescriptor->colorPixelFormat);
    pipelineDescriptor->colorAttachments()->object(0)->setBlendingEnabled(true);
    pipelineDescriptor->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    pipelineDescriptor->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    pipelineDescriptor->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    pipelineDescriptor->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
    pipelineDescriptor->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    pipelineDescriptor->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    pipelineDescriptor->setDepthAttachmentPixelFormat(frameBufferDescriptor->depthPixelFormat);
    pipelineDescriptor->setStencilAttachmentPixelFormat(frameBufferDescriptor->stencilPixelFormat);

    pipelineDescriptor->setLabel(NS::String::string("im_gui", UTF8StringEncoding));
    NS::Error* pError = nullptr;
    auto result = device->newRenderPipelineState(pipelineDescriptor, &pError);
    if (!result) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }
    return result;
}
