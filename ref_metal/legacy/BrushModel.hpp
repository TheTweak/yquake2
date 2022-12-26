//
//  BrushModel.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 25.12.2022.
//

#ifndef BrushModel_hpp
#define BrushModel_hpp

#include <unordered_map>
#include <MetalKit/MetalKit.hpp>

#include "../../src/client/vid/header/ref.h"
#include "../model/model.h"
#include "../utils/Utils.hpp"
#include "../render/Polygon.hpp"

namespace BrushModel {
void createPolygons(entity_t* entity, model_s* model, cplane_t frustum[4], refdef_t mtl_newrefdef, vec3_t modelOrigin,
                    std::unordered_map<TexNameTransMatKey, Polygon, TexNameTransMatKeyHash> &worldPolygonsByTexture,
                    MTL::RenderPipelineState *renderPipelineState);
};

#endif /* BrushModel_hpp */
