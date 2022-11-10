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

#define PIXEL_FORMAT MTL::PixelFormatBGRA8Unorm

#include <iostream>
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
#include <simd/simd.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL.h>

#include "../src/client/vid/header/ref.h"
#include "../src/client/refresh/ref_shared.h"
#include "../src/common/header/shared.h"

#include "../src/common/header/common.h"
#include "MetalDraw.hpp"
#include "Image.hpp"
#include "SharedTypes.h"

void R_Printf(int level, const char* msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    ri.Com_VPrintf(level, msg, argptr);
    va_end(argptr);
}

/*
 * this is only here so the functions in shared source files
 * (shared.c, rand.c, flash.c, mem.c/hunk.c) can link
 */
void
Sys_Error(char *error, ...)
{
    va_list argptr;
    char text[4096]; // MAXPRINTMSG == 4096

    va_start(argptr, error);
    vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    ri.Sys_Error(ERR_FATAL, "%s", text);
}

void
Com_Printf(char *msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    ri.Com_VPrintf(PRINT_ALL, msg, argptr);
    va_end(argptr);
}

MetalRenderer* MetalRenderer::INSTANCE = new MetalRenderer();
refimport_t ri;

void MetalRenderer::InitMetal(MTL::Device *pDevice, SDL_Window *pWindow, SDL_Renderer *pRenderer) {
    _pRenderer = pRenderer;
    _pDevice = pDevice->retain();
    _pCommandQueue = _pDevice->newCommandQueue();    
    SDL_Metal_GetDrawableSize(pWindow, &_width, &_height);
    auto *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(PIXEL_FORMAT, _width, _height, false);
    textureDesc->setUsage(MTL::TextureUsageRenderTarget);
    _pTexture = _pDevice->newTexture(textureDesc);
    _pSdlTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, _width, _height);
    buildShaders();
    drawInit();
}

void MetalRenderer::drawInit() {
    /* load console characters */
    loadTexture("conchars");
}

void MetalRenderer::buildShaders() {
    using NS::StringEncoding::UTF8StringEncoding;

    MTL::Library* pLibrary = _pDevice->newDefaultLibrary();

    MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexShader", UTF8StringEncoding) );
    MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("samplingShader", UTF8StringEncoding) );

    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction( pVertexFn );
    pDesc->setFragmentFunction( pFragFn );
    pDesc->colorAttachments()->object(0)->setPixelFormat(PIXEL_FORMAT);

    NS::Error* pError = nullptr;
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
    colorAttachmentDesc->setStoreAction(MTL::StoreActionStore);
    colorAttachmentDesc->setClearColor(MTL::ClearColor(0.0f, 0.0f, 0.0f, 0));
    pRpd->setRenderTargetArrayLength(1);
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );
    pEnc->setRenderPipelineState( _pPSO );
    vector_uint2 viewPortSize;
    viewPortSize.x = _width;
    viewPortSize.y = _height;
    for (const auto& drawPicCmd : drawPicCmds) {
        // create a buffer for the vertex data
        MTL::Buffer* pVertexBuffer = _pDevice->newBuffer(drawPicCmd.textureVertex, sizeof(drawPicCmd.textureVertex), MTL::ResourceStorageModeShared);
        pEnc->setVertexBuffer(pVertexBuffer, 0, VertexInputIndex::VertexInputIndexVertices);
        pEnc->setVertexBytes(&viewPortSize, sizeof(viewPortSize), VertexInputIndex::VertexInputIndexViewportSize);
        pEnc->setFragmentTexture(_textureMap[drawPicCmd.pic].second, TextureIndex::TextureIndexBaseColor);
        pEnc->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6) );
    }
    pEnc->endEncoding();
    drawPicCmds.clear();
    
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
    image_s* image;
    char fullname[MAX_QPATH];

    if ((name[0] != '/') && (name[0] != '\\'))
    {
        Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
        image = Img::FindImage(fullname, it_pic);
    }
    else
    {
        image = Img::FindImage(name + 1, it_pic);
    }

    return image;
}
void MetalRenderer::DrawGetPicSize(int *w, int *h, char *name) {
    image_s* image = DrawFindPic(name);
    
    if (!image) {
        *w = *h = -1;
        return;
    }
    
    *w = image->width;
    *h = image->height;
}

std::pair<ImageSize, MTL::Texture*> MetalRenderer::loadTexture(std::string pic) {
    if (auto cachedImageDataIt = _textureMap.find(pic); cachedImageDataIt != _textureMap.end()) {
        return cachedImageDataIt->second;
    }
    
    image_s* image = DrawFindPic(pic.data());
    assert(image);
        
    MTL::TextureDescriptor* pTextureDescriptor = MTL::TextureDescriptor::alloc()->init();
    pTextureDescriptor->setPixelFormat(PIXEL_FORMAT);
    pTextureDescriptor->setWidth(image->width);
    pTextureDescriptor->setHeight(image->height);
    pTextureDescriptor->setStorageMode( MTL::StorageModeManaged );
    pTextureDescriptor->setUsage( MTL::ResourceUsageSample | MTL::ResourceUsageRead );
    pTextureDescriptor->setTextureType( MTL::TextureType2D );
    
    MTL::Texture* pFragmentTexture = _pDevice->newTexture(pTextureDescriptor);
    MTL::Region region = {
        0, 0, 0,
        ((uint)image->width), ((uint)image->height), 1
    };
    pFragmentTexture->replaceRegion(region, 0, image->data, image->width*4);
    
    _textureMap[pic] = {ImageSize{image->width, image->height}, pFragmentTexture};
    return _textureMap[pic];
}

void MetalRenderer::DrawPicScaled(int x, int y, char* pic, float factor) {
    ImageSize imageSize = loadTexture(pic).first;

    float halfWidth = (float)(imageSize.width)/2.0f;
    float halfHeight = (float)(imageSize.height)/2.0f;
    float offsetX = x + halfWidth - _width / 2.0;
    float offsetY = _height / 2.0 - (y + halfHeight);
    
    DrawPicCommandData d{pic, {
        // Pixel positions, Texture coordinates
        { { halfWidth + offsetX, -halfHeight + offsetY },  { 1.f, 1.f } },
        { { -halfWidth + offsetX, -halfHeight + offsetY },  { 0.f, 1.f } },
        { { -halfWidth + offsetX, halfHeight + offsetY},  { 0.f, 0.f } },

        { {  halfWidth + offsetX, -halfHeight + offsetY},  { 1.f, 1.f } },
        { { -halfWidth + offsetX, halfHeight + offsetY },  { 0.f, 0.f } },
        { {  halfWidth + offsetX, halfHeight + offsetY},  { 1.f, 0.f } },
    }};

    drawPicCmds.push_back(d);
}
void MetalRenderer::DrawStretchPic(int x, int y, int w, int h, char* name) {}

void MetalRenderer::DrawCharScaled(int x, int y, int num, float scale) {
    int row, col;
    float frow, fcol, size, scaledSize;
    num &= 255;

    if ((num & 127) == 32)
    {
        return; /* space */
    }

    if (y <= -8)
    {
        return; /* totally off screen */
    }

    row = num >> 4;
    col = num & 15;

    frow = row * 0.0625;
    fcol = col * 0.0625;
    size = 0.0625;

    scaledSize = 8*scale;
    
    float halfWidth = scaledSize/2.0f;
    float halfHeight = halfWidth;
    
    float offsetX = x + halfWidth - _width / 2.0;
    float offsetY = _height / 2.0 - (y + halfHeight);
    
    float sl = fcol;
    float tl = frow;
    float sh = fcol + size;
    float th = frow + size;
    
    DrawPicCommandData d{"conchars", {
        // Pixel positions, Texture coordinates
        { { halfWidth + offsetX, -halfHeight + offsetY },  { sh, th } },
        { { -halfWidth + offsetX, -halfHeight + offsetY },  { sl, th } },
        { { -halfWidth + offsetX, halfHeight + offsetY},  { sl, tl } },

        { {  halfWidth + offsetX, -halfHeight + offsetY},  { sh, th } },
        { { -halfWidth + offsetX, halfHeight + offsetY },  { sl, tl } },
        { {  halfWidth + offsetX, halfHeight + offsetY},  { sh, tl } },
    }};

    drawPicCmds.push_back(d);
}

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
    Swap_Init();
    ri.Vid_MenuInit();
    
    int screenWidth = 640;
    int screenHeight = 480;
    if (!ri.GLimp_InitGraphics(0, &screenWidth, &screenHeight)) {
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
        ri.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");
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
    return MetalRenderer::INSTANCE->DrawFindPic(name);
}
void Metal_DrawGetPicSize(int *w, int *h, char *name) {
    MetalRenderer::INSTANCE->DrawGetPicSize(w, h, name);
}
void Metal_DrawPicScaled(int x, int y, char* pic, float factor) {
    MetalRenderer::INSTANCE->DrawPicScaled(x, y, pic, factor);
}
void Metal_DrawStretchPic(int x, int y, int w, int h, char* name) {}
void Metal_DrawCharScaled(int x, int y, int num, float scale) {
    MetalRenderer::INSTANCE->DrawCharScaled(x, y, num, scale);
}
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
extern "C" refexport_t GetRefAPI(refimport_t ri_) {
    ri = ri_;
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
