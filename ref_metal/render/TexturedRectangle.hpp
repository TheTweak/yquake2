//
//  Texture2D.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 21.12.2022.
//

#pragma once

#include <string>
#include <array>
#include <optional>

#include "Renderable.hpp"
#include "../utils/SharedTypes.h"

class TexturedRectangle : public Renderable {
protected:
    std::string pic;
    int x, y;
    float scale;
    MTL::RenderPipelineState *pipelineState;
    virtual std::optional<Quad> createQuad(vector_uint2 viewportSize) = 0;
    virtual std::optional<MTL::DepthStencilState*> getDepthStencilState();
public:
    TexturedRectangle(std::string pic, int x, int y, float scale, MTL::RenderPipelineState *pipelineState);
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize);
};
