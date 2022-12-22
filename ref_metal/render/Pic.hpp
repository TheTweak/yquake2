//
//  Pic.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#include <stdio.h>
#include <string>
#include <array>

#include "Texture2D.hpp"
#include "../utils/SharedTypes.h"

class Pic : public Texture2D {
    using Texture2D::Texture2D;
    
    std::optional<Quad> createQuad(vector_uint2 viewportSize) override;
};
