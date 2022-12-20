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
#include <SDL2/SDL.h>
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

#pragma mark - Utils
#pragma region Utils {

#pragma endregion Utils }

MetalRenderer& MetalRenderer::getInstance() {
    static MetalRenderer instance;
    return instance;
}

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
    dTextureDesc->setStorageMode(MTL::StorageModeMemoryless);
    _pDepthTexture = _pDevice->newTexture(dTextureDesc);
    _pDepthTexture->setLabel(NS::MakeConstantString("depth buffer"));
    
    _pSdlTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, _width, _height);
    
    buildShaders();
    buildDepthStencilState();
    drawInit();
    pPool->release();
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
        
        MTL::RenderPipelineDescriptor* pDesc = createPipelineStateDescriptor(pVertexFn, pFragFn, true);
        
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
    
    cvar_t *flushMap = GAME_API.Cvar_Get("flushmap", "0", 0);
    std::stringstream ss;
    ss << "maps/" << map << ".bsp";
    std::string mapName = ss.str();
    
    if (auto m = modelLoader.getModel(mapName, std::nullopt, true); m != std::nullopt) {
        worldModel = m.value();
    }
    
    _viewCluster = -1;
}

model_s* MetalRenderer::RegisterModel(char* name) {
    auto modelOpt = modelLoader.getModel(name, worldModel, false);
    if (modelOpt) {
        auto model = *modelOpt;
        if (model->type == mod_sprite) {
            dsprite_t *sprout = (dsprite_t *) model->extradata;
            
            for (int i = 0; i < sprout->numframes; i++) {
                model->skins[i] = imageLoader.FindImage(sprout->frames[i].name, it_sprite);
            }
        } else if (model->type == mod_alias) {
            dmdl_t *pheader = (dmdl_t *) model->extradata;
            
            for (int i = 0; i < pheader->num_skins; i++) {
                model->skins[i] = imageLoader.FindImage((char *) pheader + pheader->ofs_skins + i * MAX_SKINNAME, it_skin);
            }
            
            model->numframes = pheader->num_frames;
        } else if (model->type == mod_brush) {
            for (int i = 0; i < model->numtexinfo; i++) {
//                not applicable to metal renderer
//                model->texinfo[i].image->registration_sequence = registration_sequence;
            }
        }
        
        return model.get();
    }
    return nullptr;
}

image_s* MetalRenderer::RegisterSkin(char* name) {
    return imageLoader.FindImage(name, it_skin);
}

void MetalRenderer::SetSky(char* name, float rotate, vec3_t axis) {}

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

void MetalRenderer::EndFrame() {
    encodeMetalCommands();
    
    _frame = (_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool MetalRenderer::EndWorldRenderpass() {
    return true;
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
    
    recursiveWorldNode(&ent, worldModel->nodes);
    drawTextureChains(&ent);
}

simd_float4x4 MetalRenderer::rotateForEntity(entity_t* entity) {
    // angles: pitch (around y), yaw (around z), roll (around x)
    // rot matrices to be multiplied in order Z, Y, X (yaw, pitch, roll)
    simd_float4x4 transMat = Utils::rotateAroundAxisZYX(entity->angles[1], -entity->angles[0], -entity->angles[2]);
    
    for (int i = 0; i < 3; i++) {
        transMat.columns[3][i] = entity->origin[i];
    }
    
    return simd_mul(matrix_identity_float4x4, transMat);
}

void MetalRenderer::drawAliasModel(entity_t* entity) {
    vec3_t bbox[8];
    if (!(entity->flags & RF_WEAPONMODEL) && Utils::CullAliasModel(bbox, entity, frustum)) {
        return;
    }
    
    if ((entity->flags & RF_WEAPONMODEL) && cvar::gl_lefthand->value == 2) {
        return;
    }
    
    mtl_model_t* model = entity->model;
    dmdl_t *paliashdr = (dmdl_t *) model->extradata;
    
    vec3_t shadelight;

    /* get lighting information */
    if (entity->flags &
        (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED |
         RF_SHELL_BLUE | RF_SHELL_DOUBLE))
    {
    }
    else if (entity->flags & RF_FULLBRIGHT)
    {
    }
    else
    {
        legacyLight.lightPoint(entity, entity->origin, shadelight, mtl_newrefdef, worldModel);

        /* player lighting hack for communication back to server */
        if (entity->flags & RF_WEAPONMODEL)
        {
            /* pick the greatest component, which should be
               the same as the mono value returned by software */
            if (shadelight[0] > shadelight[1])
            {
                if (shadelight[0] > shadelight[2])
                {
                    cvar::r_lightlevel->value = 150 * shadelight[0];
                }
                else
                {
                    cvar::r_lightlevel->value = 150 * shadelight[2];
                }
            }
            else
            {
                if (shadelight[1] > shadelight[2])
                {
                    cvar::r_lightlevel->value = 150 * shadelight[1];
                }
                else
                {
                    cvar::r_lightlevel->value = 150 * shadelight[2];
                }
            }
        }
    }
    
    if (entity->flags & RF_DEPTHHACK) {
        //gl set depth range todo: alternative in metal
    }
    
    std::optional<simd_float4x4> projMatOpt = std::nullopt;
    if (entity->flags & RF_WEAPONMODEL) {
        float screenaspect = (float)mtl_newrefdef.width / mtl_newrefdef.height;
        float dist = (cvar::r_farsee->value == 0) ? 4096.0f : 8192.0f;
        
        simd_float4x4 projMat;
        if (cvar::r_gunfov->value < 0) {
            projMat = Utils::gluPerspective(mtl_newrefdef.fov_y, screenaspect, 2, dist);
        } else {
            projMat = Utils::gluPerspective(cvar::r_gunfov->value, screenaspect, 2, dist);
        }
        
        if (cvar::gl_lefthand->value == 1.0f) {
            // to mirror gun so it's rendered left-handed, just invert X-axis column
            // of projection matrix
            for(int i=0; i<4; ++i) {
                projMat.columns[0][i] = -projMat.columns[0][i];
            }
        }
        projMatOpt = simd_mul(projMat, modelViewMatrix);
    }
    entity->angles[PITCH] = -entity->angles[PITCH];
    simd_float4x4 transModelMat = rotateForEntity(entity);
    entity->angles[PITCH] = -entity->angles[PITCH];
    
    image_s *skin;
    if (entity->skin) {
        skin = entity->skin;
    } else {
        if (entity->skinnum >= MAX_MD2SKINS) {
            skin = model->skins[0];
        } else {
            skin = model->skins[entity->skinnum];
            if (!skin) {
                skin = model->skins[0];
            }
        }
    }
    assert(skin);
    
    if ((entity->frame >= paliashdr->num_frames) ||
        (entity->frame < 0)) {
        R_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such frame %d\n",
                model->name, entity->frame);
        entity->frame = 0;
        entity->oldframe = 0;
    }
    
    if ((entity->oldframe >= paliashdr->num_frames) ||
        (entity->oldframe < 0)) {
        R_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such oldframe %d\n",
                model->name, entity->oldframe);
        entity->frame = 0;
        entity->oldframe = 0;
    }
    
    {
        bool colorOnly = 0 != (entity->flags &
                               (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE |
                                RF_SHELL_HALF_DAM));
        
        daliasframe_t *frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->frame * paliashdr->framesize);
        
        dtrivertx_t *v, *ov, *verts;
        verts = frame->verts;
        v = verts;
        
        daliasframe_t *oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->oldframe * paliashdr->framesize);
        ov = oldframe->verts;
        
        int *order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
        
        float alpha;
        if (entity->flags & RF_TRANSLUCENT) {
            alpha = entity->alpha * 0.666f;
        } else {
            alpha = 1.0;
        }
        
        vec3_t delta;
        _VectorSubtract(entity->oldorigin, entity->origin, delta);
        vec3_t vectors[3];
        AngleVectors(entity->angles, vectors[0], vectors[1], vectors[2]);
        
        vec3_t move;
        move[0] = DotProduct(delta, vectors[0]); /* forward */
        move[1] = -DotProduct(delta, vectors[1]); /* left */
        move[2] = DotProduct(delta, vectors[2]); /* up */
        
        VectorAdd(move, oldframe->translate, move);
        
        float backlerp = entity->backlerp;
        float frontlerp = 1.0 - backlerp;
        float *lerp;
        
        vec3_t frontv, backv;
        for (int i = 0; i < 3; i++) {
            move[i] = backlerp * move[i] + frontlerp * frame->translate[i];

            frontv[i] = frontlerp * frame->scale[i];
            backv[i] = backlerp * oldframe->scale[i];
        }
        
        lerp = s_lerped[0];
        Utils::LerpVerts(colorOnly, paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);
        
        int count;
        int index_xyz;
        if (auto tit = _textureMap.find(skin->path); tit == _textureMap.end()) {
            _textureMap[skin->path] = {ImageSize{skin->width, skin->height},
                draw->createTexture(skin->width, skin->height, _pDevice, skin->data)};            
            _textureMap[skin->path].second->setLabel(NS::String::string(skin->path.data(), NS::StringEncoding::UTF8StringEncoding));
        }
        DrawAliasPolyCommandData dp;
        dp.textureName = skin->path;
        dp.projMat = projMatOpt;
        dp.alpha = alpha;
        dp.transModelMat = transModelMat;
        dp.clamp = entity->flags & RF_WEAPONMODEL;

        while (1) {

            count = *order++;
            
            if (!count) {
                break;
            }
            
            if (count < 0) {
                count = -count;
                dp.triangle = true;
            } else {
                dp.triangle = false;
            }
            
            std::vector<Vertex> vertices;
            if (colorOnly) {
               // todo
            } else {
                for (int i = 0; i < count; i++) {
                    Vertex v;
                    v.texCoordinate[0] = ((float *) order)[0];
                    v.texCoordinate[1] = ((float *) order)[1];
                    
                    index_xyz = order[2];
                    
                    order += 3;
                    for (int j = 0; j < 3; j++) {
                        v.position[j] = s_lerped[index_xyz][j];
                    }
                    
                    vertices.push_back(v);
                }
            }
            if (dp.triangle) {
                for (int i = 1; i < count-1; i++) {
                    dp.vertices.push_back(vertices.at(0));
                    dp.vertices.push_back(vertices.at(i));
                    dp.vertices.push_back(vertices.at(i+1));
                }
            } else {
                int i;
                for (i = 1; i < count - 2; i += 2) {
                    dp.vertices.push_back(vertices.at(i-1));
                    dp.vertices.push_back(vertices.at(i));
                    dp.vertices.push_back(vertices.at(i+1));
                    
                    dp.vertices.push_back(vertices.at(i));
                    dp.vertices.push_back(vertices.at(i+2));
                    dp.vertices.push_back(vertices.at(i+1));
                }
                if (i < count - 1) {
                    dp.vertices.push_back(vertices.at(i-1));
                    dp.vertices.push_back(vertices.at(i));
                    dp.vertices.push_back(vertices.at(i+1));
                }
            }
        }
        drawAliasModPolyCmds.push_back(dp);
    }
}

void MetalRenderer::drawBrushModel(entity_t* entity, model_s* model) {
    if (model->nummodelsurfaces == 0) {
        return;
    }
    
    bool rotated;
    vec3_t mins, maxs;
    
    if (entity->angles[0] || entity->angles[1] || entity->angles[2]) {
        rotated = true;
        
        for (int i = 0; i < 3; i++) {
            mins[i] = entity->origin[i] - model->radius;
            maxs[i] = entity->origin[i] + model->radius;
        }
    } else {
        rotated = false;
        VectorAdd(entity->origin, model->mins, mins);
        VectorAdd(entity->origin, model->maxs, maxs);
    }
    
    if (Utils::CullBox(mins, maxs, frustum)) {
        return;
    }
    
    VectorSubtract(mtl_newrefdef.vieworg, entity->origin, modelOrigin);
    
    if (rotated) {
        vec3_t temp;
        vec3_t forward, right, up;

        VectorCopy(modelOrigin, temp);
        AngleVectors(entity->angles, forward, right, up);
        modelOrigin[0] = DotProduct(temp, forward);
        modelOrigin[1] = -DotProduct(temp, right);
        modelOrigin[2] = DotProduct(temp, up);
    }
    entity->angles[0] = -entity->angles[0];
    entity->angles[2] = -entity->angles[2];
    simd_float4x4 transModelMat = rotateForEntity(entity);
    entity->angles[0] = -entity->angles[0];
    entity->angles[2] = -entity->angles[2];
    
    msurface_t *psurf = &model->surfaces[model->firstmodelsurface];
    
    for (int i = 0; i < model->nummodelsurfaces; i++, psurf++) {
        cplane_t *pplane = psurf->plane;
        
        float dot = DotProduct(modelOrigin, pplane->normal) - pplane->dist;
        
        /* draw the polygon */
        if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
            (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
            if (psurf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
                /* add to the translucent chain */
                // todo
//            } else if(!(psurf->flags & SURF_DRAWTURB)) {
                // check in GL3_DrawGLFlowingPoly
            } else {
                image_s* image = Utils::TextureAnimation(entity, psurf->texinfo);
                glpoly_t *p = psurf->polys;
                
                if (!p || !p->numverts) continue;
                
                for (int i = 2; i < p->numverts; i++) {
                    DrawPolyCommandData dp;
                    dp.alpha = 1.0f;
                    dp.textureName = image->path;
                    dp.transModelMat = transModelMat;
                    auto vertexArray = getPolyVertices(dp.textureName, p, i, image);
                    for (int j = 0; j < vertexArray.size(); j++) {
                        dp.vertices.push_back(vertexArray[j]);
                    }
                    drawPolyCmds.push_back(dp);
                }
            }
        }
    }
}

void MetalRenderer::drawSpriteModel(entity_t* e, model_s* currentmodel) {
    dsprite_t* psprite = (dsprite_t *) currentmodel->extradata;
    
    e->frame %= psprite->numframes;
    dsprframe_t* frame = &psprite->frames[e->frame];
    
    float* up = vup;
    float* right = vright;
    
    float alpha = 1.0f;
    if (e->flags & RF_TRANSLUCENT) {
        alpha = e->alpha;
    }
    
    mtl_3D_vtx_t verts[4];
    
    verts[0].texCoord[0] = 0;
    verts[0].texCoord[1] = 1;
    verts[1].texCoord[0] = 0;
    verts[1].texCoord[1] = 0;
    verts[2].texCoord[0] = 1;
    verts[2].texCoord[1] = 0;
    verts[3].texCoord[0] = 1;
    verts[3].texCoord[1] = 1;

    VectorMA( e->origin, -frame->origin_y, up, verts[0].pos );
    VectorMA( verts[0].pos, -frame->origin_x, right, verts[0].pos );

    VectorMA( e->origin, frame->height - frame->origin_y, up, verts[1].pos );
    VectorMA( verts[1].pos, -frame->origin_x, right, verts[1].pos );

    VectorMA( e->origin, frame->height - frame->origin_y, up, verts[2].pos );
    VectorMA( verts[2].pos, frame->width - frame->origin_x, right, verts[2].pos );

    VectorMA( e->origin, -frame->origin_y, up, verts[3].pos );
    VectorMA( verts[3].pos, frame->width - frame->origin_x, right, verts[3].pos );
    
    DrawPicCommandData dp;
    image_s *image = currentmodel->skins[e->frame];
    dp.pic = image->path;
    
    if (auto tit = _textureMap.find(dp.pic); tit == _textureMap.end()) {
        _textureMap[dp.pic] = {ImageSize{image->width, image->height},
            draw->createTexture(image->width, image->height, _pDevice, image->data)};
        _textureMap[dp.pic].second->setLabel(NS::String::string(dp.pic.data(), NS::StringEncoding::UTF8StringEncoding));
    }
    
    dp.textureVertex[0] = {
        {verts[0].pos[0], verts[0].pos[1]},
        {verts[0].texCoord[0], verts[0].texCoord[1]}
    };
    dp.textureVertex[1] = {
        {verts[1].pos[0], verts[1].pos[1]},
        {verts[1].texCoord[0], verts[1].texCoord[1]}
    };
    dp.textureVertex[2] = {
        {verts[2].pos[0], verts[2].pos[1]},
        {verts[2].texCoord[0], verts[2].texCoord[1]}
    };
    
    dp.textureVertex[3] = {
        {verts[0].pos[0], verts[0].pos[1]},
        {verts[0].texCoord[0], verts[0].texCoord[1]}
    };
    dp.textureVertex[4] = {
        {verts[3].pos[0], verts[3].pos[1]},
        {verts[3].texCoord[0], verts[3].texCoord[1]}
    };
    dp.textureVertex[5] = {
        {verts[2].pos[0], verts[2].pos[1]},
        {verts[2].texCoord[0], verts[2].texCoord[1]}
    };
    
    drawSpriteCmds.push_back(std::move(dp));
}

void MetalRenderer::drawEntity(entity_t* currentEntity) {
    if (currentEntity->flags & RF_BEAM) {
        drawBeam(currentEntity);
    } else {
        model_s *currentModel = currentEntity->model;
        
        if (!currentModel) {
            drawNullModel(currentEntity);
        } else {
            switch (currentModel->type) {
               case mod_alias:
                   drawAliasModel(currentEntity);
                   break;
               case mod_brush:
                   drawBrushModel(currentEntity, currentModel);
                   break;
               case mod_sprite:
                   drawSpriteModel(currentEntity, currentModel);
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
        
        drawEntity(currentEntity);
    }
    
    // then draw translucent
    for (int i = 0; i < mtl_newrefdef.num_entities; i++) {
        entity_t *currentEntity = &mtl_newrefdef.entities[i];
        
        if (!(currentEntity->flags & RF_TRANSLUCENT)) {
            continue;
        }
        
        drawEntity(currentEntity);
    }
}

void MetalRenderer::drawBeam(entity_t *entity) {
    
}

void MetalRenderer::drawNullModel(entity_t *entity) {
    
}

std::array<Vertex, 3> MetalRenderer::getPolyVertices(std::string textureName, glpoly_t* poly, int vertexIndex, image_s* image) {
    if (auto tit = _textureMap.find(textureName); tit == _textureMap.end()) {
        _textureMap[textureName] = {ImageSize{image->width, image->height},
            draw->createTexture(image->width, image->height, _pDevice, image->data)};
        _textureMap[textureName].second->setLabel(NS::String::string(textureName.data(), NS::StringEncoding::UTF8StringEncoding));
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
        !cvar::r_novis->value &&
        _viewCluster != -1) {
        return;
    }
    
    if (cvar::r_lockpvs->value) {
        return;
    }
    
    _visFrameCount++;
    _oldViewCluster = _viewCluster;
    _oldViewCluster2 = _viewCluster2;
    
    if (cvar::r_novis->value || _viewCluster == -1 || !worldModel->vis) {
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
    float pointSize = cvar::metal_particle_size->value * (float) mtl_newrefdef.height/480.0f;
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

void MetalRenderer::encodePolyCommands(MTL::RenderCommandEncoder* pEnc) {
    if (drawPolyCmds.empty()) {
        return;
    }
    pEnc->setRenderPipelineState(_pVertexPSO);
    
    std::unordered_map<DrawPolyCommandKey, DrawPolyCommandData, DrawPolyCommandKeyHash> texturePolys;
    // group polygons by texture and translation matrix
    for (auto& cmd: drawPolyCmds) {
        DrawPolyCommandKey key;
        key.textureName = cmd.textureName;
        if (cmd.transModelMat) {
            key.transModelMat = cmd.transModelMat.value();
        } else {
            key.transModelMat = matrix_identity_float4x4;
        }
        for (auto& v: cmd.vertices) {
            texturePolys[key].vertices.push_back(v);
        }
        texturePolys[key].alpha = cmd.alpha;
        texturePolys[key].transModelMat = cmd.transModelMat;
    }
    std::vector<MTL::Buffer*> buffers;
    for (auto it = texturePolys.begin(); it != texturePolys.end(); it++) {
        auto pBuffer = _pDevice->newBuffer(it->second.vertices.size()*sizeof(Vertex), MTL::ResourceStorageModeManaged);
        assert(pBuffer);
        buffers.push_back(pBuffer);
        std::memcpy(pBuffer->contents(), it->second.vertices.data(), it->second.vertices.size()*sizeof(Vertex));
        pEnc->setVertexBuffer(pBuffer, 0, VertexInputIndex::VertexInputIndexVertices);
        
        if (it->second.transModelMat) {
            pEnc->setVertexBytes(&it->second.transModelMat.value(), sizeof(it->second.transModelMat.value()), VertexInputIndex::VertexInputIndexTransModelMatrix);
        } else {
            pEnc->setVertexBytes(&matrix_identity_float4x4, sizeof(matrix_identity_float4x4), VertexInputIndex::VertexInputIndexTransModelMatrix);
        }
        pEnc->setVertexBytes(&mvpMatrix, sizeof(mvpMatrix), VertexInputIndex::VertexInputIndexMVPMatrix);
        pEnc->setVertexBytes(&it->second.alpha, sizeof(it->second.alpha), VertexInputIndex::VertexInputIndexAlpha);
        
        auto texture = _textureMap.at(it->first.textureName).second;
        pEnc->setFragmentTexture(texture, TextureIndex::TextureIndexBaseColor);
        
        pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(it->second.vertices.size()));
    }
    
    for (auto pBuffer: buffers) {
        pBuffer->release();
    }
    drawPolyCmds.clear();
}

void MetalRenderer::encode2DCommands(MTL::RenderCommandEncoder* pEnc, MTL::RenderPipelineState* pPs, std::vector<DrawPicCommandData>& cmds) {
    if (cmds.empty()) {
        return;
    }
    
    pEnc->setRenderPipelineState(pPs);
    vector_uint2 viewPortSize;
    viewPortSize.x = _width;
    viewPortSize.y = _height;
    for (auto& drawPicCmd: cmds) {
        pEnc->setVertexBytes(&drawPicCmd.textureVertex, sizeof(drawPicCmd.textureVertex), TexVertexInputIndex::TexVertexInputIndexVertices);
        pEnc->setVertexBytes(&viewPortSize, sizeof(viewPortSize), TexVertexInputIndex::TexVertexInputIndexViewportSize);
        pEnc->setFragmentTexture(_textureMap[drawPicCmd.pic].second, TextureIndex::TextureIndexBaseColor);
        pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
    }
    cmds.clear();
}

void MetalRenderer::encodeParticlesCommands(MTL::RenderCommandEncoder* pEnc) {
    if (drawPartCmds.empty()) {
        return;
    }
    
    pEnc->setRenderPipelineState(_pParticlePSO);
    std::vector<Particle> particles;
    for (int i = 0; i < drawPartCmds.size(); i++) {
        particles.push_back(drawPartCmds.at(i).particle);
    }
    auto pBuffer = _pDevice->newBuffer(particles.size()*sizeof(Particle), MTL::ResourceStorageModeManaged);
    assert(pBuffer);
    std::memcpy(pBuffer->contents(), particles.data(), particles.size()*sizeof(Particle));
    pEnc->setVertexBuffer(pBuffer, 0, ParticleInputIndex::ParticleInputIndexVertices);
    pEnc->setVertexBytes(&mvpMatrix, sizeof(mvpMatrix), ParticleInputIndex::ParticleInputIndexMVPMatrix);
    pEnc->setVertexBytes(&matrix_identity_float4x4, sizeof(matrix_identity_float4x4), ParticleInputIndex::ParticleInputIndexIdentityM);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypePoint, NS::UInteger(0), NS::UInteger(particles.size()));
    drawPartCmds.clear();
    pBuffer->release();
}

void MetalRenderer::encodeAliasModPolyCommands(MTL::RenderCommandEncoder* pEnc) {
    if (drawAliasModPolyCmds.empty()) {
        return;
    }
    
    std::vector<MTL::Buffer*> buffers;
    for (auto& cmd: drawAliasModPolyCmds) {
        if (cmd.vertices.empty()) {
            continue;
        }
        
        simd_float4x4* mvp;
        if (cmd.projMat) {
            mvp = &cmd.projMat.value();
        } else {
            mvp = &mvpMatrix;
        }
        
        auto pBuffer = _pDevice->newBuffer(cmd.vertices.size()*sizeof(Vertex), MTL::ResourceStorageModeShared);
        assert(pBuffer);
        buffers.push_back(pBuffer);
        std::memcpy(pBuffer->contents(), cmd.vertices.data(), cmd.vertices.size()*sizeof(Vertex));
        pEnc->setVertexBuffer(pBuffer, 0, VertexInputIndex::VertexInputIndexVertices);
        pEnc->setVertexBytes(mvp, sizeof(*mvp), VertexInputIndex::VertexInputIndexMVPMatrix);
        pEnc->setVertexBytes(&cmd.transModelMat, sizeof(cmd.transModelMat), VertexInputIndex::VertexInputIndexTransModelMatrix);
        pEnc->setVertexBytes(&cmd.alpha, sizeof(cmd.alpha), VertexInputIndex::VertexInputIndexAlpha);
        
        auto texture = _textureMap.at(std::string(cmd.textureName.data(), cmd.textureName.size())).second;
        pEnc->setFragmentTexture(texture, TextureIndex::TextureIndexBaseColor);
        if (cmd.clamp) {
            pEnc->setDepthClipMode(MTL::DepthClipModeClamp);
        } else {
            pEnc->setDepthClipMode(MTL::DepthClipModeClip);
        }
        pEnc->drawPrimitives(cmd.triangle ? MTL::PrimitiveTypeTriangle : MTL::PrimitiveTypeLineStrip, NS::UInteger(0), NS::UInteger(cmd.vertices.size()));
    }
    pEnc->setDepthClipMode(MTL::DepthClipModeClip);
    drawAliasModPolyCmds.clear();
    
    for (auto pBuffer: buffers) {
        pBuffer->release();
    }
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

    encodePolyCommands(pEnc);
    encodeAliasModPolyCommands(pEnc);
    encode2DCommands(pEnc, _pVertexPSO, drawSpriteCmds);
    encodeParticlesCommands(pEnc);
    
    pEnc->setDepthStencilState(_pNoDepthTest);
    encode2DCommands(pEnc, _p2dPSO, drawPicCmds);
    pEnc->endEncoding();

    auto blitCmdEnc = pCmd->blitCommandEncoder();
    
    for (auto it = _textureMap.begin(); it != _textureMap.end(); it++) {
        if (it->second.second != NULL && generatedMipMaps.find(it->first) == generatedMipMaps.end()) {
            blitCmdEnc->generateMipmaps(it->second.second);
            generatedMipMaps.insert(it->first);            
        }
    }

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
