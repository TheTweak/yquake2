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
    int w, h;
    float sl = 0.0f;
    float tl = 0.0f;
    float sh = 1.0f;
    float th = 1.0f;
    float scale;
    MTL::RenderPipelineState *pipelineState;
    virtual std::optional<Quad> createQuad(vector_uint2 viewportSize) = 0;
    virtual std::optional<MTL::DepthStencilState*> getDepthStencilState();
public:
    TexturedRectangle(std::string pic, int x, int y, float scale, MTL::RenderPipelineState *pipelineState);
    TexturedRectangle(std::string pic, int x, int y, int w, int h, MTL::RenderPipelineState *pipelineState);
    TexturedRectangle(std::string pic, int x, int y, int w, int h, float sl, float tl, float sh, float th, MTL::RenderPipelineState *pipelineState);
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize);
};
