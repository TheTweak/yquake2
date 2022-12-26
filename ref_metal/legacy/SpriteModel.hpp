//
//  SpriteModel.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 26.12.2022.
//

#ifndef SpriteModel_hpp
#define SpriteModel_hpp

#include <MetalKit/MetalKit.hpp>

#include "../../src/client/vid/header/ref.h"
#include "../model/model.h"
#include "../render/Sprite.hpp"

namespace SpriteModel {
Sprite createSprite(entity_t* e, model_s* currentmodel, vec3_t vup, vec3_t vright, MTL::RenderPipelineState *renderPipelineState);
};

#endif /* SpriteModel_hpp */
