//
//  MetalDraw.cpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include <iostream>
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
#include <simd/simd.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL.h>
#include "../src/client/vid/header/ref.h"

#include "MetalDraw.hpp"

MetalRenderer* MetalRenderer::INSTANCE = new MetalRenderer();
static refimport_t RI;

void MetalRenderer::SetDevice(MTL::Device *pDevice) {
    _pDevice = pDevice->retain();
    _pCommandQueue = _pDevice->newCommandQueue();
}

bool MetalRenderer::Init() {
    return true;
}
void MetalRenderer::Shutdown() {}
int MetalRenderer::PrepareForWindow() {
    return 1;
}
int MetalRenderer::InitContext() {
    return 1;
}
void MetalRenderer::ShutdownContext() {}
bool MetalRenderer::IsVSyncActive() {
    return false;
}
void MetalRenderer::BeginRegistration(char* map) {}
model_s* MetalRenderer::RegisterModel(char* name) {
    return NULL;
}
image_s* MetalRenderer::RegisterSkin(char* name) {
    return NULL;
}
void MetalRenderer::SetSky(char* name, float rotate, vec3_t axis) {}
void MetalRenderer::EndRegistration() {}
void MetalRenderer::RenderFrame(refdef_t* fd) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    auto* renderPipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDesc->colorAttachments()->object(0)->setPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );
    NS::Error* pError = nullptr;
    auto* renderPipelineState = _pDevice->newRenderPipelineState(renderPipelineDesc, &pError);
    if (!renderPipelineState) {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    MTL::RenderPassDescriptor* pRpd = MTL::RenderPassDescriptor::alloc()->init();
    pRpd->setRenderTargetArrayLength(1);
    auto colorAttachmentDesc = pRpd->colorAttachments()->object(0);
    colorAttachmentDesc->setClearColor(MTL::ClearColor(0.0f, 0.8f, 0.8f, 0.8f)); // BGRA?
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );
    pEnc->endEncoding();
    pCmd->commit();

    pPool->release();
    
}
image_s* MetalRenderer::DrawFindPic(char* name) {
    return NULL;
}
void MetalRenderer::DrawGetPicSize(int *w, int *h, char *name) {}
void MetalRenderer::DrawPicScaled(int x, int y, char* pic, float factor) {}
void MetalRenderer::DrawStretchPic(int x, int y, int w, int h, char* name) {}
void MetalRenderer::DrawCharScaled(int x, int y, int num, float scale) {}
void MetalRenderer::DrawTileClear(int x, int y, int w, int h, char* name) {}
void MetalRenderer::DrawFill(int x, int y, int w, int h, int c) {}
void MetalRenderer::DrawFadeScreen() {}
void MetalRenderer::DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data) {}
void MetalRenderer::SetPalette(const unsigned char* palette) {}
void MetalRenderer::BeginFrame(float camera_separation) {}
void MetalRenderer::EndFrame() {}
bool MetalRenderer::EndWorldRenderpass() {
    return true;
}

enum {
    rserr_ok,

    rserr_invalid_mode,

    rserr_unknown
};

bool Metal_Init() {
    int screenWidth = 960;
    int screenHeight = 600;
    if (!RI.GLimp_InitGraphics(0, &screenWidth, &screenHeight)) {
        return rserr_invalid_mode;
    }
    return true;
}
void Metal_Shutdown() {}
int Metal_PrepareForWindow() {
    return SDL_WINDOW_METAL;
}
int Metal_InitContext(void* p_sdlWindow) {
    if (p_sdlWindow == NULL) {
        RI.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");
        return false;
    }

    SDL_Window* window = static_cast<SDL_Window*>(p_sdlWindow);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    MTL::Resource* layer = static_cast<MTL::Resource*>(SDL_RenderGetMetalLayer(renderer));
    MTL::Device* device = layer->device();
    MetalRenderer::INSTANCE->SetDevice(device);
    return 1;
}
void Metal_ShutdownContext() {}
bool Metal_IsVSyncActive() {
    return false;
}
void Metal_BeginRegistration(char* map) {}
model_s* Metal_RegisterModel(char* name) {
    return NULL;
}
image_s* Metal_RegisterSkin(char* name) {
    return NULL;
}
void Metal_SetSky(char* name, float rotate, vec3_t axis) {}
void Metal_EndRegistration() {}
void Metal_RenderFrame(refdef_t* fd) {
    MetalRenderer::INSTANCE->RenderFrame(fd);
}
image_s* Metal_DrawFindPic(char* name) {
    return NULL;
}
void Metal_DrawGetPicSize(int *w, int *h, char *name) {}
void Metal_DrawPicScaled(int x, int y, char* pic, float factor) {}
void Metal_DrawStretchPic(int x, int y, int w, int h, char* name) {}
void Metal_DrawCharScaled(int x, int y, int num, float scale) {}
void Metal_DrawTileClear(int x, int y, int w, int h, char* name) {}
void Metal_DrawFill(int x, int y, int w, int h, int c) {}
void Metal_DrawFadeScreen() {}
void Metal_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data) {}
void Metal_SetPalette(const unsigned char* palette) {}
void Metal_BeginFrame(float camera_separation) {}
void Metal_EndFrame() {}
bool Metal_EndWorldRenderpass() {
    return true;
}

__attribute__((__visibility__("default")))
extern "C" refexport_t GetRefAPI(refimport_t ri) {
    RI = ri;
    refexport_t re;
    re.api_version = API_VERSION;
    
    re.Init = Metal_Init;
    re.Shutdown = Metal_Shutdown;
    re.PrepareForWindow = Metal_PrepareForWindow;
    re.InitContext = Metal_InitContext;
    re.ShutdownContext = Metal_ShutdownContext;
    re.IsVSyncActive = Metal_IsVSyncActive;

    re.BeginRegistration = Metal_BeginRegistration;
    re.RegisterModel = Metal_RegisterModel;
    re.RegisterSkin = Metal_RegisterSkin;

    re.SetSky = Metal_SetSky;
    re.EndRegistration = Metal_EndRegistration;

    re.RenderFrame = Metal_RenderFrame;

    re.DrawFindPic = Metal_DrawFindPic;
    re.DrawGetPicSize = Metal_DrawGetPicSize;

    re.DrawPicScaled = Metal_DrawPicScaled;
    re.DrawStretchPic = Metal_DrawStretchPic;

    re.DrawCharScaled = Metal_DrawCharScaled;
    re.DrawTileClear = Metal_DrawTileClear;
    re.DrawFill = Metal_DrawFill;
    re.DrawFadeScreen = Metal_DrawFadeScreen;

    re.DrawStretchRaw = Metal_DrawStretchRaw;
    re.SetPalette = Metal_SetPalette;

    re.BeginFrame = Metal_BeginFrame;
    re.EndWorldRenderpass = Metal_EndWorldRenderpass;
    re.EndFrame = Metal_EndFrame;
    
    ri.Vid_RequestRestart(RESTART_NO);

    return re;
}
