//
//  MetalRenderer.hpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//
#pragma once

#ifndef MetalRenderer_hpp
#define MetalRenderer_hpp

#define MAX_PARTICLES_COUNT 1000

#include <unordered_map>
#include <vector>
#include <memory>
#include <string_view>

#include "Utils.hpp"
#include "MetalDraw.hpp"
#include "BufferAllocator.hpp"
#include "Model.hpp"
#include "Image.hpp"

using ParticleBuffer = BufferAllocator<sizeof(DrawParticleCommandData::particle) * MAX_PARTICLES_COUNT>;
using TextureVertexBuffer = BufferAllocator<sizeof(DrawPicCommandData::textureVertex)>;

class MetalRenderer {
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    MTL::RenderPipelineState* _p2dPSO;
    MTL::RenderPipelineState* _pParticlePSO;
    MTL::RenderPipelineState* _pVertexPSO;
    MTL::Texture* _pTexture;
    SDL_Texture* _pSdlTexture;
    SDL_Renderer* _pRenderer;
    MTL::Resource* _pMetalLayer;
    MTL::DepthStencilState* _pDepthStencilState;
    MTL::DepthStencilState* _pNoDepthTest;
    MTL::Texture* _pDepthTexture;
    
    int _width = 0;
    int _height = 0;
    int _oldViewCluster;
    int _oldViewCluster2;
    int _viewCluster;
    int _viewCluster2;
    
    Model modelLoader;
    Img imageLoader;
    std::shared_ptr<mtl_model_t> worldModel;
    float vBlend[4]; /* final blending color */
    msurface_t* alphaSurfaces;
    
    vec3_t vup;
    vec3_t vpn;
    vec3_t vright;
    vec3_t origin;
    vec3_t modelOrigin;
    
    cplane_t frustum[4];
    
    int _frame = 0;
    int _frameCount = 0;
    int _visFrameCount = 0; /* bumped when going to a new PVS */
    
    std::vector<DrawPicCommandData> drawPicCmds;
    std::vector<DrawParticleCommandData> drawPartCmds;
    std::vector<DrawPolyCommandData> drawPolyCmds;
    std::unordered_map<std::string, std::pair<ImageSize, MTL::Texture*>> _textureMap;
    std::unique_ptr<MetalDraw> draw;
    dispatch_semaphore_t _semaphore;
    std::unique_ptr<TextureVertexBuffer> _textureVertexBufferAllocator;
    std::unique_ptr<ParticleBuffer> _particleBufferAllocator;
    simd_float4x4 projectionMatrix;
    simd_float4x4 modelViewMatrix;
    simd_float4x4 mvpMatrix;
    
    MetalRenderer();
    void buildShaders();
    void buildDepthStencilState();
    void drawInit();
    std::pair<ImageSize, MTL::Texture*> loadTexture(std::string pic);
    void encodeMetalCommands();
    void encode2DCommands(MTL::RenderCommandEncoder*);
    void encodeParticlesCommands(MTL::RenderCommandEncoder*);
    void encodePolyCommands(MTL::RenderCommandEncoder*);
    void encodePolyCommandBatch(MTL::RenderCommandEncoder* pEnc, Vertex* vertexBatch, int batchSize, std::string_view textureName, float alpha);
    void flashScreen();
    void renderView();
    void setupFrame();
    void setupFrustum();
    void markLeaves();
    void recursiveWorldNode(entity_t*, mnode_t*);
    
    void drawWorld();
    void drawEntities();
    void drawEntity(entity_t*);
    void drawAlphaSurfaces();
    void drawTextureChains(entity_t*);
    void drawBeam(entity_t*);
    void drawNullModel(entity_t*);
    void drawParticles();
    void drawAliasModel(entity_t*);
    void drawBrushModel(entity_t*, model_s*);
    void drawSpriteModel(entity_t*, model_s*);
    
    MTL::RenderPipelineDescriptor* createPipelineStateDescriptor(MTL::Function* pVertexFn, MTL::Function* pFragFn);
    MTL::RenderPassDescriptor* createRenderPassDescriptor();
    void updateMVPMatrix();
    std::array<Vertex, 3> getPolyVertices(std::string textureName, glpoly_t* poly, int vertexIndex, image_s* image);
    
public:
    static MetalRenderer* INSTANCE;
    
    void InitMetal(MTL::Device* pDevice, SDL_Window* pWindow, SDL_Renderer* pRenderer, MTL::Resource* pLayer);
    bool Init();
    void Shutdown();
    int PrepareForWindow();
    int InitContext();
    void ShutdownContext();
    bool IsVSyncActive();
    void BeginRegistration(char* map);
    model_s* RegisterModel(char* name);
    image_s* RegisterSkin(char* name);
    void SetSky(char* name, float rotate, vec3_t axis);
    void EndRegistration();
    void RenderFrame(refdef_t* fd);
    image_s* DrawFindPic(char* name);
    void DrawGetPicSize(int *w, int *h, char *name);
    void DrawPicScaled(int x, int y, char* pic, float factor);
    void DrawStretchPic(int x, int y, int w, int h, char* name);
    void DrawCharScaled(int x, int y, int num, float scale);
    void DrawTileClear(int x, int y, int w, int h, char* name);
    void DrawFill(int x, int y, int w, int h, int c);
    void DrawFadeScreen();
    void DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data);
    void SetPalette(const unsigned char* palette);
    void BeginFrame(float camera_separation);
    void EndFrame();
    bool EndWorldRenderpass();
    
};

#endif /* MetalRenderer_hpp */
