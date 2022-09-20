//
//  MetalDraw.hpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//
#pragma once

#ifndef MetalDraw_hpp
#define MetalDraw_hpp

class MetalRenderer {
private:
    MTL::Device* _pDevice;
    MetalRenderer() = default;
public:
    static MetalRenderer* INSTANCE;
    
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

#endif /* MetalDraw_hpp */
