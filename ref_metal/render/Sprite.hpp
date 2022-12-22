//
//  Pic.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include <stdio.h>
#include <string>
#include <array>

#include "TexturedRectangle.hpp"
#include "../utils/SharedTypes.h"

class Sprite : public TexturedRectangle {
    using TexturedRectangle::TexturedRectangle;
    
    std::optional<Quad> createQuad(vector_uint2 viewportSize) override;
};
