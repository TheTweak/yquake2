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
#include "../src/client/refresh/ref_shared.h"
#include "../src/common/header/shared.h"

#include "../src/common/header/common.h"
#include "MetalDraw.hpp"

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
        #include <simd/simd.h>

        using namespace metal;

        // The structure that is fed into the vertex shader. We use packed data types to alleviate memory alignment
        // issues caused by the float2
        typedef struct {
            packed_float4 position;
            packed_float4 colour;
        } Vertex;

        // The output of the vertex shader, which will be fed into the fragment shader
        typedef struct {
            float4 position [[position]];
            float4 colour;
        } RasteriserData;

        vertex RasteriserData vertFunc(uint vertexID [[vertex_id]],
                                                constant Vertex *vertices [[buffer(0)]]) {
            
            RasteriserData out;
            //out.position = float4(0.0, 0.0, 0.0, 1.0);
            out.position = vertices[vertexID].position;
            out.colour = vertices[vertexID].colour;

            // Both the colour and the clip space position will be interpolated in this data structure
            return out;
        }

        fragment float4 fragFunc(RasteriserData in [[stage_in]]) {
            return in.colour;
        }
    )";

    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = _pDevice->newLibrary( NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError );
    if ( !pLibrary ) {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertFunc", UTF8StringEncoding) );
    MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("fragFunc", UTF8StringEncoding) );

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
    // Vertex data, [Vertex x,y,z,w] [Colour B,G,R,A]
    const float vertexData[] =
    {
        0.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0,
      -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
    };
    // create a buffer for the vertex data
    _pVertexBuffer = _pDevice->newBuffer(vertexData, sizeof(vertexData), MTL::CPUCacheModeDefaultCache);
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
    pEnc->setVertexBuffer(_pVertexBuffer, 0, 0);
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

image_s* MetalRenderer::findImage(char *name, imagetype_t type) {
    image_s* image;
    int i, len;
    byte *pic;
    int width, height;
    char *ptr;
    char namewe[256];
    int realwidth = 0, realheight = 0;
    const char* ext;

    if (!name)
    {
        return NULL;
    }

    ext = COM_FileExtension(name);
    if(!ext[0])
    {
        /* file has no extension */
        return NULL;
    }
    
    len = strlen(name);

    /* Remove the extension */
    memset(namewe, 0, 256);
    memcpy(namewe, name, len - (strlen(ext) + 1));

    if (len < 5)
    {
        return NULL;
    }

    /* fix backslashes */
    while ((ptr = strchr(name, '\\')))
    {
        *ptr = '/';
    }
    
    pic = NULL;
    
    if (strcmp(ext, "pcx") == 0)
    {
        LoadPCX(name, &pic, NULL, &width, &height);

        if (!pic)
        {
            return NULL;
        }

//        image = GL3_LoadPic(name, pic, width, 0, height, 0, type, 8);
    }
    else if (strcmp(ext, "wal") == 0 || strcmp(ext, "m8") == 0)
    {
        if (strcmp(ext, "m8") == 0)
        {
//            image = LoadM8(name, type);

            if (!image)
            {
                /* No texture found */
                return NULL;
            }
        }
        else /* gl_retexture is not set */
        {
//            image = LoadWal(name, type);

            if (!image)
            {
                /* No texture found */
                return NULL;
            }
        }
    }
    else if (strcmp(ext, "tga") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0)
    {
        char tmp_name[256];

        realwidth = 0;
        realheight = 0;

        strcpy(tmp_name, namewe);
        strcat(tmp_name, ".wal");
        GetWalInfo(tmp_name, &realwidth, &realheight);

        if (realwidth == 0 || realheight == 0) {
            strcpy(tmp_name, namewe);
            strcat(tmp_name, ".m8");
            GetM8Info(tmp_name, &realwidth, &realheight);
        }

        if (realwidth == 0 || realheight == 0) {
            /* It's a sky or model skin. */
            strcpy(tmp_name, namewe);
            strcat(tmp_name, ".pcx");
            GetPCXInfo(tmp_name, &realwidth, &realheight);
        }

        /* TODO: not sure if not having realwidth/heigth is bad - a tga/png/jpg
         * was requested, after all, so there might be no corresponding wal/pcx?
         * if (realwidth == 0 || realheight == 0) return NULL;
         */

        if(LoadSTB(name, ext, &pic, &width, &height))
        {
//            image = GL3_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
        } else {
            return NULL;
        }
    }
    else
    {
        return NULL;
    }

    if (pic)
    {
        free(pic);
    }

    return image;
}

image_s* MetalRenderer::DrawFindPic(char* name) {
    image_s* image;
    char fullname[MAX_QPATH];

    if ((name[0] != '/') && (name[0] != '\\'))
    {
        Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
        image = findImage(fullname, it_pic);
    }
    else
    {
        image = findImage(name + 1, it_pic);
    }

    return image;
}
void MetalRenderer::DrawGetPicSize(int *w, int *h, char *name) {}
void MetalRenderer::DrawPicScaled(int x, int y, char* pic, float factor) {
    image_s* image = DrawFindPic(pic);
    
}
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
    Swap_Init();
    ri.Vid_MenuInit();
    
    int screenWidth = 960;
    int screenHeight = 600;
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
void Metal_DrawGetPicSize(int *w, int *h, char *name) {}
void Metal_DrawPicScaled(int x, int y, char* pic, float factor) {
    MetalRenderer::INSTANCE->DrawPicScaled(x, y, pic, factor);
}
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
