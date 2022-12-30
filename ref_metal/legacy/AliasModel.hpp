//
//  AliasModel.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 25.12.2022.
//

#ifndef AliasModel_hpp
#define AliasModel_hpp

#include "../render/Polygon.hpp"
#include "../../src/client/vid/header/ref.h"
#include "LegacyLight.hpp"

typedef float vec4_t[4];

class AliasModel {
    vec4_t s_lerped[MAX_VERTS];
public:
    Polygon* createPolygon(entity_t* entity, cplane_t frustum[4], LegacyLight legacyLight, refdef_t mtl_newrefdef, mtl_model_t *worldModel, simd_float4x4 &modelViewMatrix, MTL::RenderPipelineState *renderPipelineState);
};

#endif /* AliasModel_hpp */
