//
//  Pic.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#ifndef Sprite_h
#define Sprite_h

#include <stdio.h>
#include <string>
#include <array>

#include "TexturedRectangle.hpp"

class Sprite : public TexturedRectangle {
    using TexturedRectangle::TexturedRectangle;
    std::optional<Quad> quad;
    std::optional<Quad> createQuad(vector_uint2 viewportSize) override;
public:
    Sprite(std::string pic, MTL::RenderPipelineState *pipelineState);
    void setQuad(Quad q);
};

#endif
