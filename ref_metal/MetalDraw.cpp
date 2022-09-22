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

void MetalRenderer::InitMetal(MTL::Device *pDevice, SDL_Window *pWindow, SDL_Renderer *pRenderer) {
    _pRenderer = pRenderer;
    _pDevice = pDevice->retain();
    _pCommandQueue = _pDevice->newCommandQueue();
    SDL_Metal_GetDrawableSize(pWindow, &_width, &_height);
    auto *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm, _width, _height, false);
    textureDesc->setUsage(MTL::TextureUsageRenderTarget);
    _pTexture = _pDevice->newTexture(textureDesc);
    _pSdlTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, _width, _height);
    buildShaders();
    buildBuffers();
}

void MetalRenderer::buildShaders() {
    using NS::StringEncoding::UTF8StringEncoding;

    const char* shaderSrc = R"(
        #include <metal_stdlib>
        using namespace metal;

        struct v2f
        {
            float4 position [[position]];
            half3 color;
        };

        v2f vertex vertexMain( uint vertexId [[vertex_id]],
                               device const float3* positions [[buffer(0)]],
                               device const float3* colors [[buffer(1)]] )
        {
            v2f o;
            o.position = float4( positions[ vertexId ], 1.0 );
            o.color = half3 ( colors[ vertexId ] );
            return o;
        }

        half4 fragment fragmentMain( v2f in [[stage_in]] )
        {
            return half4( in.color, 1.0 );
        }
    )";

    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = _pDevice->newLibrary( NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError );
    if ( !pLibrary ) {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexMain", UTF8StringEncoding) );
    MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("fragmentMain", UTF8StringEncoding) );

    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction( pVertexFn );
    pDesc->setFragmentFunction( pFragFn );
    pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatRGBA8Unorm);

    _pPSO = _pDevice->newRenderPipelineState( pDesc, &pError );
    if ( !_pPSO ) {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    pLibrary->release();
}

void MetalRenderer::buildBuffers() {
    const size_t NumVertices = 8;
    const float s = 0.5;
    const float z = 0.0;

    simd::float3 positions[NumVertices] =
    {
        { z, z, z },
        { s, z, z },
    };

    simd::float3 colors[NumVertices] =
    {
        {  1.0, 0.3f, 0.2f },
        {  0.8f, 1.0, 0.0f },
        {  0.8f, 0.0f, 1.0 }
    };

    const size_t positionsDataSize = NumVertices * sizeof( simd::float3 );
    const size_t colorDataSize = NumVertices * sizeof( simd::float3 );

    MTL::Buffer* pVertexPositionsBuffer = _pDevice->newBuffer( positionsDataSize, MTL::ResourceStorageModeManaged );
    MTL::Buffer* pVertexColorsBuffer = _pDevice->newBuffer( colorDataSize, MTL::ResourceStorageModeManaged );

    _pVertexPositionsBuffer = pVertexPositionsBuffer;
    _pVertexColorsBuffer = pVertexColorsBuffer;

    memcpy( _pVertexPositionsBuffer->contents(), positions, positionsDataSize );
    memcpy( _pVertexColorsBuffer->contents(), colors, colorDataSize );

    _pVertexPositionsBuffer->didModifyRange( NS::Range::Make( 0, _pVertexPositionsBuffer->length() ) );
    _pVertexColorsBuffer->didModifyRange( NS::Range::Make( 0, _pVertexColorsBuffer->length() ) );
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

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    MTL::RenderPassDescriptor* pRpd = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachmentDesc = pRpd->colorAttachments()->object(0);
    colorAttachmentDesc->setTexture(_pTexture);
    colorAttachmentDesc->setLoadAction(MTL::LoadActionClear);
    colorAttachmentDesc->setClearColor(MTL::ClearColor(0.5f, 0.5f, 0.5f, 0));
    pRpd->setRenderTargetArrayLength(1);
    
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );

    pEnc->setRenderPipelineState( _pPSO );
    pEnc->setVertexBuffer( _pVertexPositionsBuffer, 0, 0 );
    pEnc->setVertexBuffer( _pVertexColorsBuffer, 0, 1 );
    pEnc->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3) );
    pEnc->endEncoding();
    
    auto blitCmdEnc = pCmd->blitCommandEncoder();
    blitCmdEnc->synchronizeTexture(_pTexture, 0, 0);
    blitCmdEnc->endEncoding();
    pCmd->commit();
    pCmd->waitUntilCompleted();
    
    uint32_t* pixels;
    int pitch;
    
    SDL_LockTexture(_pSdlTexture, NULL, (void**) &pixels, &pitch);
    _pTexture->getBytes(pixels, _width * 4, MTL::Region(0, 0, _width, _height), 0);
    SDL_UnlockTexture(_pSdlTexture);
    SDL_RenderCopy(_pRenderer, _pSdlTexture, NULL, NULL);
    SDL_RenderPresent(_pRenderer);

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
    MetalRenderer::INSTANCE->InitMetal(device, window, renderer);
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
