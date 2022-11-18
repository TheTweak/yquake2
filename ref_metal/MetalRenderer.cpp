//
//  MetalRenderer.cpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define FADE_SCREEN_TEXTURE "_FADE_SCREEN"
#define FLASH_SCREEN_TEXTURE "_FLASH_SCREEN"
#define FILL_TEXTURE "_FILL_SCREEN_"

#include <iostream>
#include <sstream>
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
#include "MetalRenderer.hpp"
#include "Image.hpp"

#pragma mark - Utils
#pragma region Utils {

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
refdef_t mtl_newrefdef;

cvar_t *r_mode;
cvar_t *metal_particle_size;

#pragma endregion Utils }

MetalRenderer::MetalRenderer() {
    _semaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);
}

void MetalRenderer::InitMetal(MTL::Device *pDevice, SDL_Window *pWindow, SDL_Renderer *pRenderer, MTL::Resource* pLayer) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    _pRenderer = pRenderer;
    _pMetalLayer = pLayer;
    _pDevice = pDevice->retain();
    _pCommandQueue = _pDevice->newCommandQueue();    
    SDL_Metal_GetDrawableSize(pWindow, &_width, &_height);
    auto *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(PIXEL_FORMAT, _width, _height, false);
    textureDesc->setUsage(MTL::TextureUsageRenderTarget);
    _pTexture = _pDevice->newTexture(textureDesc);
    _pSdlTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, _width, _height);
    buildShaders();
    drawInit();
    pPool->release();
    draw = std::make_unique<MetalDraw>(_width, _height);
    _textureVertexBufferAllocator = std::make_unique<TextureVertexBuffer>(_pDevice);
    _particleBufferAllocator = std::make_unique<ParticleBuffer>(_pDevice);
}

void MetalRenderer::drawInit() {
    /* load console characters */
    loadTexture("conchars");
}

void MetalRenderer::buildShaders() {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    using NS::StringEncoding::UTF8StringEncoding;

    MTL::Library* pLibrary = _pDevice->newDefaultLibrary();

    {
        MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexShader", UTF8StringEncoding) );
        MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("samplingShader", UTF8StringEncoding) );
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn);
        
        NS::Error* pError = nullptr;
        _p2dPSO = _pDevice->newRenderPipelineState(pDesc, &pError);
        if (!_p2dPSO) {
            __builtin_printf("%s", pError->localizedDescription()->utf8String());
            assert(false);
        }
        
        pVertexFn->release();
        pFragFn->release();
        pDesc->release();
    }

    {
        MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("particleVertexShader", UTF8StringEncoding) );
        MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("particleSamplingShader", UTF8StringEncoding) );
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn);
        
        NS::Error* pError = nullptr;
        _pParticlePSO = _pDevice->newRenderPipelineState(pDesc, &pError);
        if (!_pParticlePSO) {
            __builtin_printf("%s", pError->localizedDescription()->utf8String());
            assert(false);
        }
        
        pVertexFn->release();
        pFragFn->release();
        pDesc->release();
    }
    
    pLibrary->release();
    pPool->release();
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
    mtl_newrefdef = *fd;
            
    if (mtl_newrefdef.blend[3] != 0.0f) {
        flashScreen();
    }
    
    //drawParticles();
    encodeMetalCommands();
    
    _textureVertexBufferAllocator->updateFrame(_frame);
    _frame = (_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

image_s* MetalRenderer::DrawFindPic(char* name) {
    return draw->drawFindPic(name);
}

void MetalRenderer::DrawGetPicSize(int *w, int *h, char *name) {
    draw->drawGetPicSize(w, h, name);
}

void MetalRenderer::DrawPicScaled(int x, int y, char* pic, float factor) {
    ImageSize imageSize = loadTexture(pic).first;
    drawPicCmds.push_back(draw->createDrawTextureCmdData(pic, x, y, imageSize.width * factor, imageSize.height * factor));
}

void MetalRenderer::DrawStretchPic(int x, int y, int w, int h, char* name) {
    drawPicCmds.push_back(draw->createDrawTextureCmdData(name, x, y, w, h));
}

void MetalRenderer::DrawCharScaled(int x, int y, int num, float scale) {
    if (auto cmd = draw->drawCharScaled(x, y, num, scale); cmd != std::nullopt) {
        drawPicCmds.push_back(cmd.value());
    }
}

void MetalRenderer::DrawTileClear(int x, int y, int w, int h, char* name) {
    drawPicCmds.push_back(draw->createDrawTextureCmdData(name, x, y, w, h, x/64.0f, y/64.0f, (x + w)/64.0f, (y + h)/64.0f));    
}

void MetalRenderer::DrawFill(int x, int y, int w, int h, int c) {
    std::ostringstream os;
    os << FILL_TEXTURE << c;
    auto k = os.str();
    if (auto cachedImageDataIt = _textureMap.find(k); cachedImageDataIt == _textureMap.end()) {
        _textureMap[k] = {{w, h}, draw->createdColoredTexture({(float)c, (float)c, (float)c, 0.5f}, _pDevice)};
    }
    drawPicCmds.push_back(draw->createDrawTextureCmdData(k, 0, 0, w, h));
}

void MetalRenderer::DrawFadeScreen() {
    if (auto cachedImageDataIt = _textureMap.find(FADE_SCREEN_TEXTURE); cachedImageDataIt == _textureMap.end()) {
        _textureMap[FADE_SCREEN_TEXTURE] = {{_width, _height}, draw->createdColoredTexture({0.0f, 0.0f, 0.0f, 128.0f}, _pDevice)};
    }
    drawPicCmds.push_back(draw->createDrawTextureCmdData(FADE_SCREEN_TEXTURE, 0, 0, _width, _height));
}

void MetalRenderer::DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data) {}

void MetalRenderer::SetPalette(const unsigned char* palette) {}

void MetalRenderer::BeginFrame(float camera_separation) {}

void MetalRenderer::EndFrame() {}

bool MetalRenderer::EndWorldRenderpass() {
    return true;
}

#pragma mark - Private_Methods
#pragma region Private_Methods {

MTL::RenderPipelineDescriptor* MetalRenderer::createPipelineStateDescriptor(MTL::Function* pVertexFn, MTL::Function* pFragFn) {
    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(pVertexFn);
    pDesc->setFragmentFunction(pFragFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(PIXEL_FORMAT);
    pDesc->colorAttachments()->object(0)->setBlendingEnabled(true);
    pDesc->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    pDesc->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
    pDesc->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    pDesc->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
    pDesc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    pDesc->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    return pDesc;
}

MTL::RenderPassDescriptor* MetalRenderer::createRenderPassDescriptor() {
    MTL::RenderPassDescriptor* pRpd = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachmentDesc = pRpd->colorAttachments()->object(0);
    colorAttachmentDesc->setTexture(_pTexture);
    colorAttachmentDesc->setLoadAction(MTL::LoadActionClear);
    colorAttachmentDesc->setStoreAction(MTL::StoreActionStore);
    colorAttachmentDesc->setClearColor(MTL::ClearColor(0.0f, 0.0f, 0.0f, 0));
    pRpd->setRenderTargetArrayLength(1);
    return pRpd;
}

std::pair<ImageSize, MTL::Texture*> MetalRenderer::loadTexture(std::string pic) {
    if (auto cachedImageDataIt = _textureMap.find(pic); cachedImageDataIt != _textureMap.end()) {
        return cachedImageDataIt->second;
    }
    _textureMap[pic] = draw->loadTexture(pic, _pDevice);
    return _textureMap[pic];
}

void MetalRenderer::flashScreen() {
    if (auto cachedImageDataIt = _textureMap.find(FLASH_SCREEN_TEXTURE); cachedImageDataIt == _textureMap.end()) {
        _textureMap[FLASH_SCREEN_TEXTURE] = {{_width, _height}, draw->createdColoredTexture({0.0f, 0.0f, 0.0f, 64.0f}, _pDevice)};
    }
    drawPicCmds.push_back(draw->createDrawTextureCmdData(FLASH_SCREEN_TEXTURE, 0, 0, _width, _height));
}

void MetalRenderer::drawParticles() {
    const particle_t* p;
    int i = 0;
    float pointSize = metal_particle_size->value * (float) mtl_newrefdef.height/480.0f;
    for (i = 0, p = mtl_newrefdef.particles; i < mtl_newrefdef.num_particles; i++, p++) {
        Particle particle{
            {p->origin[0], p->origin[1], p->origin[2]},
            {0.0f, 255.0f, 0.0f, 255.0f},
            pointSize
        };
        drawPartCmds.push_back({particle});
    }
}

void MetalRenderer::encode2DCommands(MTL::RenderCommandEncoder* pEnc) {
    if (drawPicCmds.empty()) {
        return;
    }
    
    pEnc->setRenderPipelineState(_p2dPSO);
    vector_uint2 viewPortSize;
    viewPortSize.x = _width;
    viewPortSize.y = _height;
    for (auto& drawPicCmd: drawPicCmds) {
        MTL::Buffer* pVertexBuffer = _textureVertexBufferAllocator->getNextBuffer();
        assert(pVertexBuffer);
        std::memcpy(pVertexBuffer->contents(), drawPicCmd.textureVertex, sizeof(drawPicCmd.textureVertex));
        pEnc->setVertexBuffer(pVertexBuffer, 0, VertexInputIndex::VertexInputIndexVertices);
        pEnc->setVertexBytes(&viewPortSize, sizeof(viewPortSize), VertexInputIndex::VertexInputIndexViewportSize);
        pEnc->setFragmentTexture(_textureMap[drawPicCmd.pic].second, TextureIndex::TextureIndexBaseColor);
        pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
    }
    drawPicCmds.clear();
}

void MetalRenderer::encodeParticlesCommands(MTL::RenderCommandEncoder* pEnc) {
    if (drawPartCmds.empty()) {
        return;
    }
    
    pEnc->setRenderPipelineState(_pParticlePSO);
    Particle particles[MAX_PARTICLES_COUNT];
    int i;
    for (i = 0; i < MAX_PARTICLES_COUNT && i < drawPartCmds.size(); i++) {
        particles[i] = drawPartCmds.at(i).particle;
    }
    MTL::Buffer* pBuffer = _particleBufferAllocator->getNextBuffer();
    assert(pBuffer);
    std::memcpy(pBuffer->contents(), particles, i);
    pEnc->setVertexBuffer(pBuffer, 0, ParticleInputIndex::ParticleInputIndexVertices);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypePoint, 0, i, 1);
    drawPartCmds.clear();
}

void MetalRenderer::encodeMetalCommands() {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    MetalRenderer* pRenderer = this;
    pCmd->addCompletedHandler([pRenderer](MTL::CommandBuffer* pCommand) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });
    
    MTL::RenderPassDescriptor* pRpd = createRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    
    encodeParticlesCommands(pEnc);
    encode2DCommands(pEnc);
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

#pragma endregion Private_Methods }

#pragma mark - External_API
#pragma region External_API {
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
    
    r_mode = ri.Cvar_Get("r_mode", "4", CVAR_ARCHIVE);
    metal_particle_size = ri.Cvar_Get("gl3_particle_size", "40", CVAR_ARCHIVE);
    ri.Vid_GetModeInfo(&screenWidth, &screenHeight, r_mode->value);
    
    if (!ri.GLimp_InitGraphics(0, &screenWidth, &screenHeight)) {
        return rserr_invalid_mode;
    }
    return true;
}

void Metal_Shutdown() {
    MetalRenderer::INSTANCE->Shutdown();
}

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
    MetalRenderer::INSTANCE->InitMetal(device, window, renderer, layer);
    return 1;
}

void Metal_ShutdownContext() {
    MetalRenderer::INSTANCE->ShutdownContext();
}

bool Metal_IsVSyncActive() {
    return MetalRenderer::INSTANCE->IsVSyncActive();
}

void Metal_BeginRegistration(char* map) {
    MetalRenderer::INSTANCE->BeginRegistration(map);
}

model_s* Metal_RegisterModel(char* name) {
    return MetalRenderer::INSTANCE->RegisterModel(name);
}

image_s* Metal_RegisterSkin(char* name) {
    return MetalRenderer::INSTANCE->RegisterSkin(name);
}

void Metal_SetSky(char* name, float rotate, vec3_t axis) {
    MetalRenderer::INSTANCE->SetSky(name, rotate, axis);
}

void Metal_EndRegistration() {
    MetalRenderer::INSTANCE->EndRegistration();
}

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

void Metal_DrawStretchPic(int x, int y, int w, int h, char* name) {
    MetalRenderer::INSTANCE->DrawStretchPic(x, y, w, h, name);
}

void Metal_DrawCharScaled(int x, int y, int num, float scale) {
    MetalRenderer::INSTANCE->DrawCharScaled(x, y, num, scale);
}

void Metal_DrawTileClear(int x, int y, int w, int h, char* name) {
    MetalRenderer::INSTANCE->DrawTileClear(x, y, w, h, name);
}

void Metal_DrawFill(int x, int y, int w, int h, int c) {
    MetalRenderer::INSTANCE->DrawFill(x, y, w, h, c);
}

void Metal_DrawFadeScreen() {
    MetalRenderer::INSTANCE->DrawFadeScreen();
}

void Metal_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data) {
    MetalRenderer::INSTANCE->DrawStretchRaw(x, y, w, h, cols, rows, data);
}

void Metal_SetPalette(const unsigned char* palette) {
    MetalRenderer::INSTANCE->SetPalette(palette);
}

void Metal_BeginFrame(float camera_separation) {
    MetalRenderer::INSTANCE->BeginFrame(camera_separation);
}

void Metal_EndFrame() {
    MetalRenderer::INSTANCE->EndFrame();
}

bool Metal_EndWorldRenderpass() {
    return MetalRenderer::INSTANCE->EndWorldRenderpass();
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

#pragma endregion External_API }
