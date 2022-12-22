//
//  Hud.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 22.12.2022.
//

#ifndef Hud_h
#define Hud_h

#include <vector>
#include <optional>

#include "Renderable.hpp"
#include "../utils/SharedTypes.h"

struct HudChar {
    int num, x, y;
    float scale;    
    std::optional<Quad> createQuad(vector_uint2 viewportSize);
};

class Hud : public Renderable {
    static MTL::DepthStencilState *noDepthTest;
    std::vector<HudChar> chars;
    MTL::RenderPipelineState *pipelineState;
public:
    Hud(MTL::RenderPipelineState *pipelineState);
    void drawChar(HudChar c);
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) override;
};

#endif /* Hud_h */
