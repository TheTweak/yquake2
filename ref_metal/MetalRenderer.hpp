//
//  MetalRenderer.hpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//
#ifndef MetalRenderer_hpp
#define MetalRenderer_hpp

#include <unordered_map>
#include <vector>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <SDL2/SDL.h>
#include <optional>

#include "utils/Utils.hpp"
#include "legacy/LegacyLight.hpp"
#include "legacy/AliasModel.hpp"
#include "legacy/BinarySpacePart.hpp"
#include "render/Renderable.hpp"
#include "render/ConChars.hpp"
#include "render/Particles.hpp"
#include "render/Polygon.hpp"
#include "render/Sprite.hpp"
#include "render/SkyBox.hpp"
#include "raytracing/RayTracer.hpp"

typedef float vec4_t[4];

class MetalRenderer {
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    MTL::RenderPipelineState* _p2dPSO;
    MTL::RenderPipelineState* _pParticlePSO;
    MTL::RenderPipelineState* _pVertexPSO;
    MTL::RenderPipelineState* _pVertexAlphaBlendingPSO;
    MTL::RenderPipelineState* _pDebugPSO;
    MTL::RenderPipelineState* _pImGUIPSO;
    MTL::Texture* _pTexture;
    SDL_Texture* _pSdlTexture;
    SDL_Renderer* _pRenderer;
    MTL::Resource* _pMetalLayer;
    MTL::DepthStencilState* _pDepthStencilState;
    MTL::DepthStencilState* _pNoDepthTest;
    MTL::Texture* _pDepthTexture;
    MTL::Texture* _pImGUIFontTexture;
    
    SDL_Cursor* sdlCursor;
    
    int _width = 0;
    int _height = 0;
    int _oldViewCluster;
    int _oldViewCluster2;
    int _viewCluster;
    int _viewCluster2;
        
    mtl_model_t *worldModel;
    refdef_t mtl_newrefdef;
    float vBlend[4]; /* final blending color */
    msurface_t *alphaSurfaces;
    
    vec3_t vup;
    vec3_t vpn;
    vec3_t vright;
    vec3_t origin;
    vec3_t modelOrigin;
    vec4_t s_lerped[MAX_VERTS];
    
    cplane_t frustum[4];
    
    int _frame = 0;
    int _frameCount = 0;
    int _visFrameCount = 0; /* bumped when going to a new PVS */
    
    std::vector<std::shared_ptr<Renderable>> entities;
    std::vector<std::shared_ptr<Renderable>> gui;
    std::vector<Sprite> sprites;
    std::optional<SkyBox> skyBox;
                
    std::unordered_map<TexNameTransMatKey, Polygon, TexNameTransMatKeyHash> worldPolygonsByTexture;
    std::unordered_map<TexNameTransMatKey, Polygon, TexNameTransMatKeyHash> transparentWorldPolygonsByTexture;
    std::unordered_set<std::string> generatedMipMaps;    
    std::unique_ptr<ConChars> conChars;
    std::unique_ptr<Particles> particles;
    std::unique_ptr<RayTracer> rayTracer;
    dispatch_semaphore_t _semaphore;
    simd_float4x4 projectionMatrix;
    simd_float4x4 modelViewMatrix;
    simd_float4x4 mvpMatrix;
    
    LegacyLight legacyLight;
    AliasModel aliasModel;
    BinarySpacePart bsp;
    
    MetalRenderer();
    void buildShaders();
    void buildDepthStencilState();    
    void encodeMetalCommands();        
    void flashScreen();
    void renderView();
    void setupFrame();
    void setupFrustum();
            
    void drawWorld();
    void drawEntities();
    void drawEntity(entity_t*, bool transparent);
    void drawAlphaSurfaces();
    void drawTextureChains(entity_t*);
    void drawBeam(entity_t*);
    void drawNullModel(entity_t*);
    void drawParticles();
    
    void renderWorld(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize);
    void renderGUI(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize);
    void renderEntities(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize);
    void renderSprites(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize);
    void generateMipmaps(MTL::BlitCommandEncoder *enc);
    void renderImGUI(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize);
    void createImGUIFontsTexture();
            
    MTL::RenderPipelineDescriptor* createPipelineStateDescriptor(MTL::Function* pVertexFn, MTL::Function* pFragFn, bool blendingEnabled);
    MTL::RenderPassDescriptor* createRenderPassDescriptor();
    void updateMVPMatrix();
    
    void renderCameraDirection(MTL::RenderCommandEncoder *enc, const Uniforms &uniforms);
public:
    static MetalRenderer& getInstance();
    int getScreenWidth();
    int getScreenHeight();
    simd_float4x4 getMvpMatrix();
    MTL::Device* getDevice();    
    msurface_t* getAlphaSurfaces();
    void setAlphaSurfaces(msurface_t*);
    
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
