//
//  ImGuiHud.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 15.03.2023.
//

#ifndef ImGuiHud_hpp
#define ImGuiHud_hpp

#include <SDL2/SDL.h>

#include "Renderable.hpp"
#include "../imgui/imgui.h"

class ImGuiHud : public Renderable {
    MTL::Texture* fontTexture;
    SDL_Cursor* sdlCursor;
public:
    ImGuiHud();
    void createFontsTexture();
    void render(MTL::RenderCommandEncoder *enc, vector_uint2 viewportSize) override;
};

#endif /* ImGuiHud_hpp */
