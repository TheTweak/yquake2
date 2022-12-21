//
//  Pic.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include <stdio.h>
#include <string>

#include "Renderable.hpp"
#include "../utils/SharedTypes.h"

class Pic: public Renderable {
    std::string pic;
    int x, y;
    float scale;
    MTL::RenderPipelineState *pipelineState;
public:
    Pic(std::string pic, int x, int y, float scale, MTL::RenderPipelineState *pipelineState);
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize);
};
