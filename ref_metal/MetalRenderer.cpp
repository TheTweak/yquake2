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
#include <cmath>

#include "../src/client/vid/header/ref.h"
#include "../src/client/refresh/ref_shared.h"
#include "../src/common/header/shared.h"
#include "../src/common/header/common.h"
#include "MetalRenderer.hpp"
#include "Image.hpp"
#include "model.h"
#include "BSP.hpp"

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
cvar_t *r_farsee;
cvar_t *r_fixsurfsky;
cvar_t *r_novis;
cvar_t *r_lockpvs;
cvar_t *r_drawworld;

#pragma endregion Utils }

MetalRenderer::MetalRenderer() : modelLoader(imageLoader) {
    _semaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);
}

void MetalRenderer::InitMetal(MTL::Device *pDevice, SDL_Window *pWindow, SDL_Renderer *pRenderer, MTL::Resource* pLayer) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    _pRenderer = pRenderer;
    _pMetalLayer = pLayer;
    _pDevice = pDevice->retain();
    _pCommandQueue = _pDevice->newCommandQueue();    
    SDL_Metal_GetDrawableSize(pWindow, &_width, &_height);
    draw = std::make_unique<MetalDraw>(_width, _height, imageLoader);
    
    auto *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(PIXEL_FORMAT, _width, _height, false);
    textureDesc->setUsage(MTL::TextureUsageRenderTarget);
    _pTexture = _pDevice->newTexture(textureDesc);
    
    auto *dTextureDesc = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float, _width, _height, false);
    dTextureDesc->setUsage(MTL::TextureUsageRenderTarget);
    _pDepthTexture = _pDevice->newTexture(dTextureDesc);
    _pDepthTexture->setLabel(NS::MakeConstantString("depth buffer"));
    
    _pSdlTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, _width, _height);
    
    buildShaders();
    buildDepthStencilState();
    drawInit();
    pPool->release();
    _textureVertexBufferAllocator = std::make_unique<TextureVertexBuffer>(_pDevice);
    _particleBufferAllocator = std::make_unique<ParticleBuffer>(_pDevice);
}

void MetalRenderer::drawInit() {
    /* load console characters */
    loadTexture("conchars");
}

void MetalRenderer::buildDepthStencilState() {
    MTL::DepthStencilDescriptor* pDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDesc->setDepthWriteEnabled(true);
    pDesc->setDepthCompareFunction(MTL::CompareFunctionLess);
    _pDepthStencilState = _pDevice->newDepthStencilState(pDesc);
    pDesc->release();
    
    pDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDesc->setDepthWriteEnabled(false);
    _pNoDepthTest = _pDevice->newDepthStencilState(pDesc);
    pDesc->release();
}

void MetalRenderer::buildShaders() {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    using NS::StringEncoding::UTF8StringEncoding;

    MTL::Library* pLibrary = _pDevice->newDefaultLibrary();

    {
        MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexShader2D", UTF8StringEncoding) );
        MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("samplingShader2D", UTF8StringEncoding) );
        
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
        MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("particleFragFunc", UTF8StringEncoding) );
        
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
    
    {
        MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexShader", UTF8StringEncoding) );
        MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("fragFunc", UTF8StringEncoding) );
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn);
        
        NS::Error* pError = nullptr;
        _pVertexPSO = _pDevice->newRenderPipelineState(pDesc, &pError);
        if (!_pVertexPSO) {
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

void MetalRenderer::BeginRegistration(char* map) {
    _oldViewCluster = -1;
    
    cvar_t *flushMap = ri.Cvar_Get("flushmap", "0", 0);
    std::stringstream ss;
    ss << "maps/" << map << ".bsp";
    std::string mapName = ss.str();
    
    if (auto m = modelLoader.getModel(mapName, std::nullopt, true); m != std::nullopt) {
        worldModel = m.value();
    }
    
    _viewCluster = -1;
}

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
            
    renderView();
    
    if (vBlend[3] != 0.0f) {
        flashScreen();
    }
    
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
    pDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
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
    pRpd->depthAttachment()->setClearDepth(1.0);
    pRpd->depthAttachment()->setTexture(_pDepthTexture);
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

void MetalRenderer::renderView() {
    setupFrame();
    setupFrustum();
    updateMVPMatrix();
    
    markLeaves();
    drawWorld();
    drawEntities();
    drawParticles();
//    drawAlphaSurfaces();
}

void MetalRenderer::drawWorld() {
    if (!r_drawworld->value) {
        return;
    }
    
    if (mtl_newrefdef.rdflags & RDF_NOWORLDMODEL) {
        return;
    }
    
    VectorCopy(mtl_newrefdef.vieworg, modelOrigin);
    
    entity_t ent;
    memset(&ent, 0, sizeof(ent));
    ent.frame = (int)(mtl_newrefdef.time * 2);
    
    recursiveWorldNode(&ent, worldModel->nodes);
    drawTextureChains(&ent);
}

std::array<Vertex, 3> MetalRenderer::getPolyVertices(std::string textureName, glpoly_t* poly, int vertexIndex, image_s* image) {
    if (auto tit = _textureMap.find(textureName); tit == _textureMap.end()) {
        _textureMap[textureName] = {ImageSize{image->width, image->height},
            draw->createTexture(image->width, image->height, _pDevice, image->data)};
    }
    std::array<Vertex, 3> vertices;
    {
        auto v = poly->vertices[0];
        vector_float3 vertex = {v.pos[0], v.pos[1], v.pos[2]};
        vector_float2 texCoord = {v.texCoord[0], v.texCoord[1]};
        vertices[0] = {vertex, texCoord};
    }
    {
        auto v = poly->vertices[vertexIndex-1];
        vector_float3 vertex = {v.pos[0], v.pos[1], v.pos[2]};
        vector_float2 texCoord = {v.texCoord[0], v.texCoord[1]};
        vertices[1] = {vertex, texCoord};
    }
    {
        auto v = poly->vertices[vertexIndex];
        vector_float3 vertex = {v.pos[0], v.pos[1], v.pos[2]};
        vector_float2 texCoord = {v.texCoord[0], v.texCoord[1]};
        vertices[2] = {vertex, texCoord};
    }
    return vertices;
}

void MetalRenderer::drawTextureChains(entity_t *currentEntity) {
    std::optional<std::string> prevTexture;
    for (auto it = imageLoader.GetLoadedImages().begin(); it != imageLoader.GetLoadedImages().end(); it++) {

        msurface_t* s = it->second->texturechain;
        
        if (!s) {
            continue;
        }
        
        for (; s; s = s->texturechain) {
            glpoly_t *p = s->polys;
            
            if (!p || !p->numverts) continue;
            
            for (int i = 2; i < p->numverts; i++) {
                DrawPolyCommandData dp;
                dp.alpha = 1.0f;
                dp.textureName = it->first;
                auto vertexArray = getPolyVertices(dp.textureName, p, i, it->second.get());
                for (int j = 0; j < vertexArray.size(); j++) {
                    dp.vertices.push_back(vertexArray[j]);
                }
                drawPolyCmds.push_back(dp);
            }
        }
        
        it->second->texturechain = nullptr;
    }
}

void MetalRenderer::recursiveWorldNode(entity_t* currentEntity, mnode_t* node) {
    if (node->contents == CONTENTS_SOLID ||
        node->visframe != _visFrameCount ||
        Utils::CullBox(node->minmaxs, node->minmaxs+3, frustum)) {
        return;
    }

    mleaf_t *pleaf;
    /* if a leaf node, draw stuff */
    if (node->contents != -1) {
        pleaf = (mleaf_t *)node;

        /* check for door connected areas */
        if (mtl_newrefdef.areabits) {
            if (!(mtl_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7)))) {
                return; /* not visible */
            }
        }

        msurface_t **mark = pleaf->firstmarksurface;
        int c = pleaf->nummarksurfaces;

        if (c) {
            do {
                (*mark)->visframe = _frameCount;
                mark++;
            }
            while (--c);
        }

        return;
    }
    
    /* node is just a decision point, so go down the apropriate
       sides find which side of the node we are on */
    cplane_t *plane = node->plane;
    float dot;

    switch (plane->type) {
        case PLANE_X:
            dot = modelOrigin[0] - plane->dist;
            break;
        case PLANE_Y:
            dot = modelOrigin[1] - plane->dist;
            break;
        case PLANE_Z:
            dot = modelOrigin[2] - plane->dist;
            break;
        default:
            dot = DotProduct(modelOrigin, plane->normal) - plane->dist;
            break;
    }
    
    int side, sidebit;
    
    if (dot >= 0) {
        side = 0;
        sidebit = 0;
    } else {
        side = 1;
        sidebit = SURF_PLANEBACK;
    }
    
    /* recurse down the children, front side first */
    recursiveWorldNode(currentEntity, node->children[side]);
    
    int c;
    msurface_t *surf;
    image_s *image;
    /* draw stuff */
    for (c = node->numsurfaces, surf = worldModel->surfaces + node->firstsurface;
         c; c--, surf++) {
        if (surf->visframe != _frameCount) {
            continue;
        }

        if ((surf->flags & SURF_PLANEBACK) != sidebit) {
            continue; /* wrong side */
        }

        if (surf->texinfo->flags & SURF_SKY) {
            /* just adds to visible sky bounds */
            //GL3_AddSkySurface(surf);
        } else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
            /* add to the translucent chain */
            surf->texturechain = alphaSurfaces;
            alphaSurfaces = surf;
            alphaSurfaces->texinfo->image = Utils::TextureAnimation(currentEntity, surf->texinfo);
        } else {
            /* the polygon is visible, so add it to the texture sorted chain */
            image = Utils::TextureAnimation(currentEntity, surf->texinfo);
            surf->texturechain = image->texturechain;
            image->texturechain = surf;
        }
    }

    /* recurse down the back side */
    recursiveWorldNode(currentEntity, node->children[!side]);
}

void MetalRenderer::markLeaves() {
    if (_oldViewCluster == _viewCluster &&
        _oldViewCluster2 == _viewCluster2 &&
        !r_novis->value &&
        _viewCluster != -1) {
        return;
    }
    
    if (r_lockpvs->value) {
        return;
    }
    
    _visFrameCount++;
    _oldViewCluster = _viewCluster;
    _oldViewCluster2 = _viewCluster2;
    
    if (r_novis->value || _viewCluster == -1 || !worldModel->vis) {
        for (int i = 0; i < worldModel->numleafs; i++) {
            worldModel->leafs[i].visframe = _visFrameCount;
        }
        
        for (int i = 0; i < worldModel->numnodes; i++) {
            worldModel->nodes[i].visframe = _visFrameCount;
        }
        
        return;
    }
    
    const byte* vis = modelLoader.clusterPVS(_viewCluster, worldModel.get());
        
    if (_viewCluster2 != _viewCluster) {
        YQ2_ALIGNAS_TYPE(int) byte fatvis[MAX_MAP_LEAFS / 8];
        
        memcpy(fatvis, vis, (worldModel->numleafs + 7) / 8);
        
        vis = modelLoader.clusterPVS(_viewCluster2, worldModel.get());
        
        int c = (worldModel->numleafs + 31) / 32;
        for (int i = 0; i < c; i++) {
            ((int *)fatvis)[i] |= ((int *)vis)[i];
        }

        vis = fatvis;
    }
    
    mleaf_t *leaf;
    mnode_t *node;
    int i;
    for (i = 0, leaf = worldModel->leafs; i < worldModel->numleafs; i++, leaf++) {
        int cluster = leaf->cluster;

        if (cluster == -1) {
            continue;
        }

        if (vis[cluster >> 3] & (1 << (cluster & 7))) {
            node = (mnode_t *)leaf;

            do {
                if (node->visframe == _visFrameCount) {
                    break;
                }

                node->visframe = _visFrameCount;
                node = node->parent;
            }
            while (node);
        }
    }
}

void MetalRenderer::drawEntities() {
    
}

void MetalRenderer::drawAlphaSurfaces() {
    /* go back to the world matrix */
//    gl3state.uni3DData.transModelMat4 = gl3_identityMat4;

    for (msurface_t* s = alphaSurfaces; s != NULL; s = s->texturechain)
    {
        float alpha = 1.0f;
        if (s->texinfo->flags & SURF_TRANS33)
        {
            alpha = 0.333f;
        }
        else if (s->texinfo->flags & SURF_TRANS66)
        {
            alpha = 0.666f;
        }

        /*
        if (s->flags & SURF_DRAWTURB)
        {
            for (glpoly_s* bp = s->polys; bp != NULL; bp = bp->next) {
                for (int i = 2; i < bp->numverts; i++) {
                    auto textureName = s->texinfo->image->path;
                    drawPolyCmds.push_back(createDrawPolyCommand(textureName, bp, i, s->texinfo->image, alpha));
                }
            }
        }
        else if (s->texinfo->flags & SURF_FLOWING)
        {
            for (glpoly_s* bp = s->polys; bp != NULL; bp = bp->next) {
                for (int i = 2; i < bp->numverts; i++) {
                    auto textureName = s->texinfo->image->path;
                    drawPolyCmds.push_back(createDrawPolyCommand(textureName, bp, i, s->texinfo->image, alpha));
                }
            }
        }
        else
        {
            for (glpoly_s* bp = s->polys; bp != NULL; bp = bp->next) {
                for (int i = 2; i < bp->numverts; i++) {
                    auto textureName = s->texinfo->image->path;
                    drawPolyCmds.push_back(createDrawPolyCommand(textureName, bp, i, s->texinfo->image, alpha));
                }
            }
        }
         */
    }

    alphaSurfaces = NULL;
}

void MetalRenderer::setupFrame() {
    _frameCount++;
    VectorCopy(mtl_newrefdef.vieworg, origin);
    AngleVectors(mtl_newrefdef.viewangles, vpn, vright, vup);
    
    if (!(mtl_newrefdef.rdflags & RDF_NOWORLDMODEL)) {
        _oldViewCluster = _viewCluster;
        _oldViewCluster2 = _viewCluster2;
        
        // find where we are in the world map
        mleaf_t *leaf = BSPUtils::PointInLeaf(origin, worldModel.get());
        _viewCluster = leaf->cluster;
        _viewCluster2 = _viewCluster;
        
        /* check above and below so crossing solid water doesn't draw wrong */
        if (!leaf->contents) {
            /* look down a bit */
            vec3_t temp;
            VectorCopy(origin, temp);
            temp[2] -= 16;
            leaf = BSPUtils::PointInLeaf(temp, worldModel.get());
            
            if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != _viewCluster2)) {
                _viewCluster2 = leaf->cluster;
            }
        } else {
            /* look up a bit */
            vec3_t temp;

            VectorCopy(origin, temp);
            temp[2] += 16;
            leaf = BSPUtils::PointInLeaf(temp, worldModel.get());

            if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != _viewCluster2)) {
                _viewCluster2 = leaf->cluster;
            }
        }
        
        for (int i = 0; i < 4; i++) {
            vBlend[i] = mtl_newrefdef.blend[i];
        }
        
        /* clear out the portion of the screen that the NOWORLDMODEL defines */
        if (mtl_newrefdef.rdflags & RDF_NOWORLDMODEL) {
            // TODO: implement this
        }
    }
}

static int
SignbitsForPlane(cplane_t *out)
{
    int bits, j;

    /* for fast box on planeside test */
    bits = 0;

    for (j = 0; j < 3; j++)
    {
        if (out->normal[j] < 0)
        {
            bits |= 1 << j;
        }
    }

    return bits;
}

void MetalRenderer::setupFrustum() {
    /* rotate VPN right by FOV_X/2 degrees */
    RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - mtl_newrefdef.fov_x / 2));
    /* rotate VPN left by FOV_X/2 degrees */
    RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - mtl_newrefdef.fov_x / 2);
    /* rotate VPN up by FOV_X/2 degrees */
    RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - mtl_newrefdef.fov_y / 2);
    /* rotate VPN down by FOV_X/2 degrees */
    RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - mtl_newrefdef.fov_y / 2));

    for (int i = 0; i < 4; i++) {
        frustum[i].type = PLANE_ANYZ;
        frustum[i].dist = DotProduct(origin, frustum[i].normal);
        frustum[i].signbits = SignbitsForPlane(&frustum[i]);
    }
}

void MetalRenderer::drawParticles() {
    const particle_t* p;
    int i = 0;
    float pointSize = metal_particle_size->value * (float) mtl_newrefdef.height/480.0f;
    vector_float3 viewOrigin = {mtl_newrefdef.vieworg[0], mtl_newrefdef.vieworg[1], mtl_newrefdef.vieworg[2]};
    for (i = 0, p = mtl_newrefdef.particles; i < mtl_newrefdef.num_particles; i++, p++) {
        vector_float3 pOrigin = {p->origin[0], p->origin[1], p->origin[2]};
        vector_float3 offset = viewOrigin - pOrigin;
        auto c = imageLoader.GetPalleteColor(p->color, p->alpha);
        vector_float4 color = {c[0], c[1], c[2], c[3]};
        float distance = simd_length(offset);
        Particle particle{
            pOrigin,
            color,
            pointSize,
            distance
        };
        drawPartCmds.push_back({particle});
    }
}

void MetalRenderer::encodePolyCommandBatch(MTL::RenderCommandEncoder* pEnc, Vertex* vertexBatch, int batchSize, std::string_view textureName, float alpha) {
    pEnc->setVertexBytes(vertexBatch, batchSize*sizeof(Vertex), VertexInputIndex::VertexInputIndexVertices);
    pEnc->setVertexBytes(&mvpMatrix, sizeof(mvpMatrix), VertexInputIndex::VertexInputIndexMVPMatrix);
    pEnc->setVertexBytes(&matrix_identity_float4x4, sizeof(matrix_identity_float4x4), VertexInputIndex::VertexInputIndexIdentityM);
    pEnc->setVertexBytes(&alpha, sizeof(alpha), VertexInputIndex::VertexInputIndexAlpha);
    auto texture = _textureMap.at(std::string(textureName.data(), textureName.size())).second;
    pEnc->setFragmentTexture(texture, TextureIndex::TextureIndexBaseColor);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(batchSize));
}

void MetalRenderer::encodePolyCommands(MTL::RenderCommandEncoder* pEnc) {
    if (drawPolyCmds.empty()) {
        return;
    }
    pEnc->setRenderPipelineState(_pVertexPSO);
    for (auto& cmd: drawPolyCmds) {
        encodePolyCommandBatch(pEnc, cmd.vertices.data(), (int) cmd.vertices.size(), cmd.textureName, cmd.alpha);
    }
    drawPolyCmds.clear();
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
        pEnc->setVertexBuffer(pVertexBuffer, 0, TexVertexInputIndex::TexVertexInputIndexVertices);
        pEnc->setVertexBytes(&viewPortSize, sizeof(viewPortSize), TexVertexInputIndex::TexVertexInputIndexViewportSize);
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
    pEnc->setVertexBytes(&mvpMatrix, sizeof(mvpMatrix), ParticleInputIndex::ParticleInputIndexMVPMatrix);
    pEnc->setVertexBytes(&matrix_identity_float4x4, sizeof(matrix_identity_float4x4), ParticleInputIndex::ParticleInputIndexIdentityM);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypePoint, NS::UInteger(0), NS::UInteger(i));
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
    
    pEnc->setDepthStencilState(_pDepthStencilState);
    encodePolyCommands(pEnc);
    
    pEnc->setDepthStencilState(_pNoDepthTest);
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

void MetalRenderer::updateMVPMatrix() {
    float screenAspect = (float) mtl_newrefdef.width / mtl_newrefdef.height;
    float dist = r_farsee->value == 0 ? 4096.0f : 8192.0f;
    
    {
        // calculate projection matrix
        float fovY = mtl_newrefdef.fov_y;
        float zNear = 4;
        float zFar = dist;
        
        float left, right, bottom, top;
        float A, B, C, D;
        
        top = zNear * std::tan(fovY * Utils::pi / 360.0);
        bottom = -top;
        left = bottom * screenAspect;
        right = top * screenAspect;
        
        A = (right + left) / (right - left);
        B = (top + bottom) / (top - bottom);
        C = -(zFar + zNear) / (zFar - zNear);
        D = -(2.0*zFar*zNear) / (zFar - zNear);
        
        projectionMatrix = simd_matrix_from_rows(simd_make_float4((2.0f*zNear)/(right-left), 0, A, 0),
                                                 simd_make_float4(0, (2.0f*zNear)/(top-bottom), B, 0),
                                                 simd_make_float4(0, 0, C, D),
                                                 simd_make_float4(0, 0, -1, 0));
    }
    
    {
        // calculate model - view matrix
        simd_float4x4 mvMatrix = simd_matrix_from_rows(simd_make_float4(0, -1, 0, 0),
                                                       simd_make_float4(0, 0, 1, 0),
                                                       simd_make_float4(-1, 0, 0, 0),
                                                       simd_make_float4(0, 0, 0, 1));
        
        float rotateX = Utils::toRadians(-mtl_newrefdef.viewangles[2]);
        float rotateY = Utils::toRadians(-mtl_newrefdef.viewangles[0]);
        float rotateZ = Utils::toRadians(-mtl_newrefdef.viewangles[1]);
        
        float sinA = std::sinf(rotateX);
        float cosA = std::cosf(rotateX);
        float sinB = std::sinf(rotateY);
        float cosB = std::cosf(rotateY);
        float sinG = std::sinf(rotateZ);
        float cosG = std::cosf(rotateZ);
        
        simd_float4x4 rotMatrix = simd_matrix_from_rows(
                simd_make_float4(cosB*cosG, -cosB*sinG, sinB, 0),
                simd_make_float4(sinA*sinB*cosG + cosA*sinG, -sinA*sinB*sinG + cosA*cosG, -sinA*cosB, 0),
                simd_make_float4(-cosA*sinB*cosG + sinA*sinG, cosA*sinB*sinG + sinA*cosG, cosA*cosB, 0),
                simd_make_float4(0, 0, 0, 1));
        
        mvMatrix = simd_mul(mvMatrix, rotMatrix);
        
        simd_float4x4 transMatrix = matrix_identity_float4x4;
        transMatrix.columns[3][0] = -mtl_newrefdef.vieworg[0];
        transMatrix.columns[3][1] = -mtl_newrefdef.vieworg[1];
        transMatrix.columns[3][2] = -mtl_newrefdef.vieworg[2];
        
        mvMatrix = simd_mul(mvMatrix, transMatrix);
        
        modelViewMatrix = mvMatrix;
    }
    
    mvpMatrix = simd_mul(projectionMatrix, modelViewMatrix);
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
    r_farsee = ri.Cvar_Get("r_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
    r_fixsurfsky = ri.Cvar_Get("r_fixsurfsky", "0", CVAR_ARCHIVE);
    r_novis = ri.Cvar_Get("r_novis", "0", 0);
    r_lockpvs = ri.Cvar_Get("r_lockpvs", "0", 0);
    r_drawworld = ri.Cvar_Get("r_drawworld", "1", 0);
    
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
