//
//  Renderable.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#pragma once

#include <array>
#include <utility>
#include <simd/simd.h>
#include <Metal/Metal.hpp>

#include "../utils/SharedTypes.h"

using Quad = std::array<TexVertex, 6>;
using VertexBufferInfo = std::pair<MTL::Buffer*, int>;
static VertexBufferInfo emptyVertexBufferInfo = { NULL, 0 };

class Renderable {
public:
    virtual VertexBufferInfo render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) = 0;
};
