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

struct ConChar {
    int num, x, y;
    float scale;    
    std::optional<Quad> createQuad(vector_uint2 viewportSize);
};

class ConChars : public Renderable {
    std::vector<ConChar> chars;
    MTL::RenderPipelineState *pipelineState;
public:
    ConChars(MTL::RenderPipelineState *pipelineState);
    void drawChar(ConChar c);
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) override;
};

#endif /* Hud_h */
