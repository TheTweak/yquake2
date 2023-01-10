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

#include <iostream>
#include <sstream>
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
#include <simd/simd.h>
#include <SDL2/SDL_video.h>
#include <cmath>

#include "../src/client/vid/header/ref.h"
#include "../src/client/refresh/ref_shared.h"
#include "../src/common/header/shared.h"
#include "../src/common/header/common.h"
#include "MetalRenderer.hpp"
#include "utils/BSPUtils.hpp"
#include "legacy/ConsoleVars.h"
#include "utils/Constants.h"
#include "legacy/State.h"
#include "render/Sprite.hpp"
#include "texture/TextureCache.hpp"
#include "model/Model.hpp"
#include "render/Polygon.hpp"
#include "utils/PolygonUtils.hpp"
#include "legacy/BrushModel.hpp"
#include "legacy/SpriteModel.hpp"

#pragma mark - Utils
#pragma region Utils {

#pragma endregion Utils }

MetalRenderer& MetalRenderer::getInstance() {
    static MetalRenderer instance;
    return instance;
}

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
    
    auto *dTextureDesc = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float, _width, _height, false);
    dTextureDesc->setUsage(MTL::TextureUsageRenderTarget);
    dTextureDesc->setStorageMode(MTL::StorageModeMemoryless);
    _pDepthTexture = _pDevice->newTexture(dTextureDesc);
    _pDepthTexture->setLabel(NS::MakeConstantString("depth buffer"));
    
    _pSdlTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, _width, _height);
    
    buildShaders();
    buildDepthStencilState();
    TextureCache::getInstance().init(_pDevice);
    conChars = std::make_unique<ConChars>(_p2dPSO);
    particles = std::make_unique<Particles>(_pParticlePSO);
    rayTracer = std::make_unique<RayTracer>();
    
    pPool->release();
}

MTL::Device* MetalRenderer::getDevice() {
    return _pDevice;
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
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn, true);
        pDesc->setLabel(NS::String::string("gui", UTF8StringEncoding));
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
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn, false);
        pDesc->setLabel(NS::String::string("particles", UTF8StringEncoding));
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
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn, false);
        pDesc->setLabel(NS::String::string("vertex", UTF8StringEncoding));
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

    {
        MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexShader", UTF8StringEncoding) );
        MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("fragFunc", UTF8StringEncoding) );
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn, true);
        pDesc->setLabel(NS::String::string("vertexAlpha", UTF8StringEncoding));
        NS::Error* pError = nullptr;
        _pVertexAlphaBlendingPSO = _pDevice->newRenderPipelineState(pDesc, &pError);
        if (!_pVertexAlphaBlendingPSO) {
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
    
    cvar_t *flushMap = GAME_API.Cvar_Get("flushmap", "0", 0);
    std::stringstream ss;
    ss << "maps/" << map << ".bsp";
    std::string mapName = ss.str();
    
    if (auto m = Model::getInstance().getModel(mapName, NULL, true); m != NULL) {
        worldModel = m;
    }
    
    _viewCluster = -1;
}

model_s* MetalRenderer::RegisterModel(char* name) {
    return Model::getInstance().registerModel(name, worldModel);
}

image_s* MetalRenderer::RegisterSkin(char* name) {
    return Image::getInstance().FindImage(name, it_skin);
}

void MetalRenderer::SetSky(char* name, float rotate, vec3_t axis) {
    if (!skyBox || skyBox->name != name) {
        skyBox.emplace(name, rotate, axis);
    }
}

void MetalRenderer::EndRegistration() {}

void MetalRenderer::RenderFrame(refdef_t* fd) {
    mtl_newrefdef = *fd;
            
    renderView();
    legacyLight.setLightLevel(NULL, mtl_newrefdef, worldModel);
    
    if (vBlend[3] != 0.0f) {
        flashScreen();
    }
}

image_s* MetalRenderer::DrawFindPic(char* name) {
    return Image::getInstance().FindImage(name);
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

void MetalRenderer::DrawPicScaled(int x, int y, char* pic, float factor) {
    gui.push_back(std::make_shared<Sprite>(pic, x, y, factor, _p2dPSO));
}

void MetalRenderer::DrawStretchPic(int x, int y, int w, int h, char* pic) {
    gui.push_back(std::make_shared<Sprite>(pic, x, y, w, h, _p2dPSO));
}

void MetalRenderer::DrawCharScaled(int x, int y, int num, float scale) {
    conChars->drawChar({num, x, y, scale});
}

void MetalRenderer::DrawTileClear(int x, int y, int w, int h, char* pic) {
    gui.push_back(std::make_shared<Sprite>(pic, x, y, w, h, x/64.0f, y/64.0f, (x + w)/64.0f, (y + h)/64.0f, _p2dPSO));
}

void MetalRenderer::DrawFill(int x, int y, int w, int h, int c) {
    float fc = (float) c;
    gui.push_back(std::make_shared<Sprite>(vector_float4{fc, fc, fc, 0.5f}, x, y, w, h, _p2dPSO));
}

void MetalRenderer::DrawFadeScreen() {
    gui.push_back(std::make_shared<Sprite>(vector_float4{0.0f, 0.0f, 0.0f, 128.0f}, 0, 0, _width, _height, _p2dPSO));
}

void MetalRenderer::DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data) {}

void MetalRenderer::SetPalette(const unsigned char* palette) {}

void MetalRenderer::BeginFrame(float camera_separation) {}

void MetalRenderer::EndFrame() {
    encodeMetalCommands();
    _frame = (_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool MetalRenderer::EndWorldRenderpass() {
    return true;
}

int MetalRenderer::getScreenWidth() {
    return _width;
}

int MetalRenderer::getScreenHeight() {
    return _height;
}

simd_float4x4 MetalRenderer::getMvpMatrix() {
    return mvpMatrix;
}

msurface_t* MetalRenderer::getAlphaSurfaces() {
    return alphaSurfaces;
}

void MetalRenderer::setAlphaSurfaces(msurface_t *as) {
    alphaSurfaces = as;
}

#pragma mark - Private_Methods
#pragma region Private_Methods {

MTL::RenderPipelineDescriptor* MetalRenderer::createPipelineStateDescriptor(MTL::Function* pVertexFn, MTL::Function* pFragFn,
                                                                            bool blendingEnabled) {
    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(pVertexFn);
    pDesc->setFragmentFunction(pFragFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(PIXEL_FORMAT);
    pDesc->colorAttachments()->object(0)->setBlendingEnabled(blendingEnabled);
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

void MetalRenderer::flashScreen() {
    gui.push_back(std::make_shared<Sprite>(vector_float4{0.0f, 0.0f, 0.0f, 64.0f}, 0, 0, _width, _height, _p2dPSO));
}

void MetalRenderer::renderView() {
    setupFrame();
    setupFrustum();
    updateMVPMatrix();
    
    bsp.markLeaves(&_oldViewCluster, &_oldViewCluster2, _viewCluster, _viewCluster2, worldModel);
    drawWorld();
    drawEntities();
    drawParticles();
    drawAlphaSurfaces();
}

void MetalRenderer::drawWorld() {
    if (!cvar::r_drawworld->value) {
        return;
    }
    
    if (mtl_newrefdef.rdflags & RDF_NOWORLDMODEL) {
        return;
    }
    
    VectorCopy(mtl_newrefdef.vieworg, modelOrigin);
    
    entity_t ent;
    memset(&ent, 0, sizeof(ent));
    ent.frame = (int)(mtl_newrefdef.time * 2);
    if (skyBox) {
        skyBox->clearSkyBox();
    }
    bsp.recursiveWorldNode(&ent, worldModel->nodes, frustum, mtl_newrefdef, _frameCount, modelOrigin, worldModel, skyBox.value(), origin);
    drawTextureChains(&ent);
}

void MetalRenderer::drawEntity(entity_t* currentEntity, bool transparent) {
    if (currentEntity->flags & RF_BEAM) {
        drawBeam(currentEntity);
    } else {
        model_s *currentModel = currentEntity->model;
        Polygon *polygon;
        if (!currentModel) {
            drawNullModel(currentEntity);
        } else {
            MTL::RenderPipelineState *renderPipelineState = transparent ? _pVertexAlphaBlendingPSO : _pVertexPSO;
            switch (currentModel->type) {
               case mod_alias:
                    polygon = aliasModel.createPolygon(currentEntity, frustum, legacyLight, mtl_newrefdef, worldModel, modelViewMatrix, renderPipelineState);
                    if (polygon) {
                        entities.push_back(std::shared_ptr<Polygon>(polygon));
                    }
                    break;
               case mod_brush:
                    BrushModel::createPolygons(currentEntity, currentModel, frustum, mtl_newrefdef, modelOrigin, worldPolygonsByTexture, renderPipelineState);
                    break;
               case mod_sprite:
                    sprites.push_back(SpriteModel::createSprite(currentEntity, currentModel, vup, vright, _p2dPSO));
                    break;
               default:
                    GAME_API.Sys_Error(ERR_DROP, "Bad modeltype");
                    break;
            }
        }
    }
}

void MetalRenderer::drawEntities() {
    // draw solid models first
    for (int i = 0; i < mtl_newrefdef.num_entities; i++) {
        entity_t *currentEntity = &mtl_newrefdef.entities[i];
        
        if (currentEntity->flags & RF_TRANSLUCENT) {
            continue;
        }
        
        drawEntity(currentEntity, false);
    }
    
    // then draw translucent
    for (int i = 0; i < mtl_newrefdef.num_entities; i++) {
        entity_t *currentEntity = &mtl_newrefdef.entities[i];
        
        if (!(currentEntity->flags & RF_TRANSLUCENT)) {
            continue;
        }
        
        drawEntity(currentEntity, true);
    }
}

void MetalRenderer::drawBeam(entity_t *entity) {
    
}

void MetalRenderer::drawNullModel(entity_t *entity) {
    
}

void MetalRenderer::drawTextureChains(entity_t *currentEntity) {
    for (auto it = Image::getInstance().GetLoadedImages().begin(); it != Image::getInstance().GetLoadedImages().end(); it++) {

        TextureCache::getInstance().addTextureForImage(it->second.get());
        
        msurface_t* s = it->second->texturechain;
        
        if (!s) {
            continue;
        }
        
        TexNameTransMatKey key{it->first, matrix_identity_float4x4};
        worldPolygonsByTexture.insert({key, Polygon{it->first, matrix_identity_float4x4, 1.0f, _pVertexPSO}});
        
        for (; s; s = s->texturechain) {
            glpoly_t *p = s->polys;
            
            if (!p || !p->numverts) {
                continue;
            }

            for (glpoly_t *pp = p; pp; pp = pp->next) {
                for (int i = 2; i < pp->numverts; i++) {
                    auto vertexArray = PolygonUtils::cutTriangle(pp, i);
                    for (int j = 0; j < vertexArray.size(); j++) {
                        worldPolygonsByTexture[key].addVertex(vertexArray[j]);
                    }
                }
            }            
        }
        it->second->texturechain = nullptr;
    }
}

void MetalRenderer::drawAlphaSurfaces() {
    for (msurface_t* s = alphaSurfaces; s != NULL; s = s->texturechain) {
        float alpha = 1.0f;
        if (s->texinfo->flags & SURF_TRANS33) {
            alpha = 0.333f;
        } else if (s->texinfo->flags & SURF_TRANS66) {
            alpha = 0.666f;
        }

        TexNameTransMatKey key{s->texinfo->image->path, matrix_identity_float4x4};
        transparentWorldPolygonsByTexture.insert({key, Polygon{key.textureName, matrix_identity_float4x4, alpha, _pVertexAlphaBlendingPSO}});
        
        for (glpoly_s* p = s->polys; p != NULL; p = p->next) {
            
            if (!p || !p->numverts) continue;
            
            for (int i = 2; i < p->numverts; i++) {
                auto vertexArray = PolygonUtils::cutTriangle(p, i);
                for (int j = 0; j < vertexArray.size(); j++) {
                    transparentWorldPolygonsByTexture[key].addVertex(vertexArray[j]);
                }
            }
        }
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
        mleaf_t *leaf = BSPUtils::PointInLeaf(origin, worldModel);
        _viewCluster = leaf->cluster;
        _viewCluster2 = _viewCluster;
        
        /* check above and below so crossing solid water doesn't draw wrong */
        if (!leaf->contents) {
            /* look down a bit */
            vec3_t temp;
            VectorCopy(origin, temp);
            temp[2] -= 16;
            leaf = BSPUtils::PointInLeaf(temp, worldModel);
            
            if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != _viewCluster2)) {
                _viewCluster2 = leaf->cluster;
            }
        } else {
            /* look up a bit */
            vec3_t temp;

            VectorCopy(origin, temp);
            temp[2] += 16;
            leaf = BSPUtils::PointInLeaf(temp, worldModel);

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
        frustum[i].signbits = Utils::SignbitsForPlane(&frustum[i]);
    }
}

void MetalRenderer::drawParticles() {
    const particle_t* p;
    int i = 0;
    float pointSize = cvar::metal_particle_size->value * (float) mtl_newrefdef.height/480.0f;
    vector_float3 viewOrigin = {mtl_newrefdef.vieworg[0], mtl_newrefdef.vieworg[1], mtl_newrefdef.vieworg[2]};
    for (i = 0, p = mtl_newrefdef.particles; i < mtl_newrefdef.num_particles; i++, p++) {
        vector_float3 pOrigin = {p->origin[0], p->origin[1], p->origin[2]};
        vector_float3 offset = viewOrigin - pOrigin;
        auto c = Image::getInstance().GetPalleteColor(p->color, p->alpha);
        vector_float4 color = {c[0], c[1], c[2], c[3]};
        float distance = simd_length(offset);
        Particle particle{
            pOrigin,
            color,
            pointSize,
            distance
        };
        particles->addParticle(particle);
    }
}

void MetalRenderer::renderWorld(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize) {
    std::vector<VertexBufferInfo> vertexBuffers;
    for (auto it = worldPolygonsByTexture.begin(); it != worldPolygonsByTexture.end(); it++) {
        vertexBuffers.push_back(it->second.render(enc, viewportSize));
    }
    worldPolygonsByTexture.clear();
    
    for (auto it = transparentWorldPolygonsByTexture.begin(); it != transparentWorldPolygonsByTexture.end(); it++) {
        vertexBuffers.push_back(it->second.render(enc, viewportSize));
    }
    transparentWorldPolygonsByTexture.clear();
    
    for (auto &vb : vertexBuffers) {
    }
}

void MetalRenderer::renderGUI(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize) {
    for (auto g: gui) {
        g->render(enc, viewportSize);
    }
    gui.clear();
    conChars->render(enc, viewportSize);
}

void MetalRenderer::renderEntities(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize) {
    for (auto e: entities) {
        e->render(enc, viewportSize);
    }
    entities.clear();
}

void MetalRenderer::generateMipmaps(MTL::BlitCommandEncoder *enc) {
    for (auto it = TextureCache::getInstance().getData().begin(); it != TextureCache::getInstance().getData().end(); it++) {
        if (it->second.second != NULL && generatedMipMaps.find(it->first) == generatedMipMaps.end()) {
            if (it->second.second->mipmapLevelCount() > 1) {
                enc->generateMipmaps(it->second.second);
            }
            generatedMipMaps.insert(it->first);
        }
    }
}

void MetalRenderer::renderSprites(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize) {
    for (auto &s: sprites) {
        s.render(enc, viewportSize);
    }
    sprites.clear();
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
    pEnc->setCullMode(MTL::CullModeBack);
    vector_uint2 viewportSize = {static_cast<unsigned int>(_width), static_cast<unsigned int>(_height)};
    renderWorld(pEnc, viewportSize);
    renderEntities(pEnc, viewportSize);
    particles->render(pEnc, viewportSize);
    renderSprites(pEnc, viewportSize);
    skyBox->render(pEnc, viewportSize, origin, mtl_newrefdef, mvpMatrix, _pVertexPSO);
    // render GUI with disabled depth test
    pEnc->setDepthStencilState(_pNoDepthTest);
    renderGUI(pEnc, viewportSize);
    pEnc->endEncoding();

    auto blitCmdEnc = pCmd->blitCommandEncoder();
    generateMipmaps(blitCmdEnc);
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
    float dist = cvar::r_farsee->value == 0 ? 4096.0f : 8192.0f;
    
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
    GAME_API.Vid_MenuInit();
    
    int screenWidth = 640;
    int screenHeight = 480;
    
    cvar::r_mode = GAME_API.Cvar_Get("r_mode", "4", CVAR_ARCHIVE);
    cvar::metal_particle_size = GAME_API.Cvar_Get("gl3_particle_size", "40", CVAR_ARCHIVE);
    cvar::r_farsee = GAME_API.Cvar_Get("r_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
    cvar::r_fixsurfsky = GAME_API.Cvar_Get("r_fixsurfsky", "0", CVAR_ARCHIVE);
    cvar::r_novis = GAME_API.Cvar_Get("r_novis", "0", 0);
    cvar::r_lockpvs = GAME_API.Cvar_Get("r_lockpvs", "0", 0);
    cvar::r_drawworld = GAME_API.Cvar_Get("r_drawworld", "1", 0);
    cvar::gl_lefthand = GAME_API.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
    cvar::r_gunfov = GAME_API.Cvar_Get("r_gunfov", "80", CVAR_ARCHIVE);
    cvar::r_lightlevel = GAME_API.Cvar_Get("r_lightlevel", "0", 0);
    cvar::r_modulate = GAME_API.Cvar_Get("r_modulate", "1", CVAR_ARCHIVE);
    
    GAME_API.Vid_GetModeInfo(&screenWidth, &screenHeight, cvar::r_mode->value);
    
    if (!GAME_API.GLimp_InitGraphics(0, &screenWidth, &screenHeight)) {
        return rserr_invalid_mode;
    }
    return true;
}

void Metal_Shutdown() {
    MetalRenderer::getInstance().Shutdown();
}

int Metal_PrepareForWindow() {
    return SDL_WINDOW_METAL;
}

int Metal_InitContext(void* p_sdlWindow) {
    if (p_sdlWindow == NULL) {
        GAME_API.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");
        return false;
    }

    SDL_Window* window = static_cast<SDL_Window*>(p_sdlWindow);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    MTL::Resource* layer = static_cast<MTL::Resource*>(SDL_RenderGetMetalLayer(renderer));
    MTL::Device* device = layer->device();
    MetalRenderer::getInstance().InitMetal(device, window, renderer, layer);
    return 1;
}

void Metal_ShutdownContext() {
    MetalRenderer::getInstance().ShutdownContext();
}

bool Metal_IsVSyncActive() {
    return MetalRenderer::getInstance().IsVSyncActive();
}

void Metal_BeginRegistration(char* map) {
    MetalRenderer::getInstance().BeginRegistration(map);
}

model_s* Metal_RegisterModel(char* name) {
    return MetalRenderer::getInstance().RegisterModel(name);
}

image_s* Metal_RegisterSkin(char* name) {
    return MetalRenderer::getInstance().RegisterSkin(name);
}

void Metal_SetSky(char* name, float rotate, vec3_t axis) {
    MetalRenderer::getInstance().SetSky(name, rotate, axis);
}

void Metal_EndRegistration() {
    MetalRenderer::getInstance().EndRegistration();
}

void Metal_RenderFrame(refdef_t* fd) {
    MetalRenderer::getInstance().RenderFrame(fd);
}

image_s* Metal_DrawFindPic(char* name) {
    return MetalRenderer::getInstance().DrawFindPic(name);
}

void Metal_DrawGetPicSize(int *w, int *h, char *name) {
    MetalRenderer::getInstance().DrawGetPicSize(w, h, name);
}

void Metal_DrawPicScaled(int x, int y, char* pic, float factor) {
    MetalRenderer::getInstance().DrawPicScaled(x, y, pic, factor);
}

void Metal_DrawStretchPic(int x, int y, int w, int h, char* name) {
    MetalRenderer::getInstance().DrawStretchPic(x, y, w, h, name);
}

void Metal_DrawCharScaled(int x, int y, int num, float scale) {
    MetalRenderer::getInstance().DrawCharScaled(x, y, num, scale);
}

void Metal_DrawTileClear(int x, int y, int w, int h, char* name) {
    MetalRenderer::getInstance().DrawTileClear(x, y, w, h, name);
}

void Metal_DrawFill(int x, int y, int w, int h, int c) {
    MetalRenderer::getInstance().DrawFill(x, y, w, h, c);
}

void Metal_DrawFadeScreen() {
    MetalRenderer::getInstance().DrawFadeScreen();
}

void Metal_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data) {
    MetalRenderer::getInstance().DrawStretchRaw(x, y, w, h, cols, rows, data);
}

void Metal_SetPalette(const unsigned char* palette) {
    MetalRenderer::getInstance().SetPalette(palette);
}

void Metal_BeginFrame(float camera_separation) {
    MetalRenderer::getInstance().BeginFrame(camera_separation);
}

void Metal_EndFrame() {
    MetalRenderer::getInstance().EndFrame();
}

bool Metal_EndWorldRenderpass() {
    return MetalRenderer::getInstance().EndWorldRenderpass();
}

refimport_t ri;

__attribute__((__visibility__("default")))
extern "C" refexport_t GetRefAPI(refimport_t ri_) {
    GAME_API = ri_;
    ri = GAME_API; // this is required for game API, when called from this renderer library, to work
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
    
    GAME_API.Vid_RequestRestart(RESTART_NO);

    return re;
}

#pragma endregion External_API }
