//
//  SkyBox.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 27.12.2022.
//

#ifndef SkyBox_hpp
#define SkyBox_hpp

#include <string>
#include <array>
#include <Metal/Metal.hpp>
#include <simd/simd.h>

#include "../model/model.h"

class SkyBox {
    float rotate;
    vec3_t axis;
    std::array<std::string, 6> textureNames;
public:
    const std::string name;
    SkyBox(std::string name, float rotate, vec3_t axis);
    void addSkySurface(msurface_t *fa, vec3_t origin);
    void clearSkyBox();
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize, vec3_t origin, refdef_t mtl_newrefdef, simd_float4x4 mvpMatrix, MTL::RenderPipelineState *pipelineState);
};

#endif /* SkyBox_hpp */
