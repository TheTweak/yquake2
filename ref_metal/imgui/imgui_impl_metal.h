// dear imgui: Renderer Backend for Metal
// This needs to be used along with a Platform Backend (e.g. OSX)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'MTLTexture' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include <unordered_map>
#include <vector>

#include "imgui.h"      // IMGUI_IMPL_API

//-----------------------------------------------------------------------------
// C++ API
//-----------------------------------------------------------------------------

// Enable Metal C++ binding support with '#define IMGUI_IMPL_METAL_CPP' in your imconfig.h file
// More info about using Metal from C++: https://developer.apple.com/metal/cpp/

#include <Metal/Metal.hpp>

IMGUI_IMPL_API bool ImGui_ImplMetal_Init(MTL::Device* device);
IMGUI_IMPL_API void ImGui_ImplMetal_Shutdown();
IMGUI_IMPL_API void ImGui_ImplMetal_NewFrame(MTL::RenderPassDescriptor* renderPassDescriptor);
IMGUI_IMPL_API void ImGui_ImplMetal_RenderDrawData(ImDrawData* draw_data,
                                                   MTL::CommandBuffer* commandBuffer,
                                                   MTL::RenderCommandEncoder* commandEncoder);

// Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool ImGui_ImplMetal_CreateFontsTexture(MTL::Device* device);
IMGUI_IMPL_API void ImGui_ImplMetal_DestroyFontsTexture();
IMGUI_IMPL_API bool ImGui_ImplMetal_CreateDeviceObjects(MTL::Device* device);
IMGUI_IMPL_API void ImGui_ImplMetal_DestroyDeviceObjects();

struct MetalBuffer {
    MTL::Buffer *buffer;
    double lastReuseTime;
    
    MetalBuffer(MTL::Buffer *);
};

struct FramebufferDescriptor {
    size_t sampleCount;
    MTL::PixelFormat colorPixelFormat;
    MTL::PixelFormat depthPixelFormat;
    MTL::PixelFormat stencilPixelFormat;
    
    FramebufferDescriptor(MTL::RenderPassDescriptor *rpd);
    
    bool operator==(const FramebufferDescriptor&) const;
};

struct MetalContext {
    MTL::Device *device;
    MTL::DepthStencilState *depthStencilState;
    FramebufferDescriptor frameBufferDescriptor;
    std::unordered_map<FramebufferDescriptor, MTL::RenderPipelineState*> renderPipelineStateCache;
    MTL::Texture *fontTexture;
    std::vector<MTL::Buffer*> bufferCache;
    double lastBufferCachePurge;
    
    MTL::Buffer* dequeueReusableBuffer(size_t length, MTL::Device* device);
    MTL::RenderPipelineState* renderPipelineState(FramebufferDescriptor fbDescriptor, MTL::Device *device);
    MetalContext();
};

struct ImGui_ImplMetal_Data {
    MetalContext* SharedMetalContext;
    
    ImGui_ImplMetal_Data();
};

ImGui_ImplMetal_Data* ImGui_ImplMetal_CreateBackendData();
ImGui_ImplMetal_Data* ImGui_ImplMetal_GetBackendData();

