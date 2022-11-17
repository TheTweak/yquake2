//
//  MetalRenderer.hpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//
#pragma once

#ifndef MetalRenderer_hpp
#define MetalRenderer_hpp

#include <unordered_map>
#include <vector>
#include <memory>

#include "Utils.hpp"
#include "MetalDraw.hpp"
#include "BufferAllocator.hpp"

class MetalRenderer {
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    MTL::RenderPipelineState* _pPSO;    
    MTL::Texture* _pTexture;
    SDL_Texture* _pSdlTexture;
    SDL_Renderer* _pRenderer;
    MTL::Resource* _pMetalLayer;
    int _width = 0;
    int _height = 0;
    size_t _frame = 0;
    std::vector<DrawPicCommandData> drawPicCmds;
    std::vector<DrawParticleCommandData> drawPartCmds;
    std::unordered_map<std::string, std::pair<ImageSize, MTL::Texture*>> _textureMap;
    std::unique_ptr<MetalDraw> draw;
    dispatch_semaphore_t _semaphore;
    std::unique_ptr<BufferAllocator<sizeof(DrawPicCommandData::textureVertex)>> _bufferAllocator;
    
    MetalRenderer();
    void buildShaders();
    void drawInit();
    std::pair<ImageSize, MTL::Texture*> loadTexture(std::string pic);
    void encodeMetalCommands();
    void flashScreen();
    void drawParticles();
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
