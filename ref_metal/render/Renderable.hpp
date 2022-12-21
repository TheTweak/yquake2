//
//  Renderable.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#ifndef Renderable_hpp
#define Renderable_hpp

#include <stdio.h>
#include <simd/simd.h>
#include <Metal/Metal.hpp>

class Renderable {
public:
    virtual void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) = 0;
};

#endif /* Renderable_hpp */
