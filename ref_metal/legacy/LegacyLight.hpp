//
//  LegacyLight.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 19.12.2022.
//

#pragma once

#ifndef LegacyLight_hpp
#define LegacyLight_hpp

#include "../model/model.h"

class LegacyLight {
    vec3_t lightspot;
    cplane_t *lightplane; /* used as shadow plane */
    vec3_t pointcolor;    
public:
    int recursiveLightPoint(mnode_t *node, vec3_t start, vec3_t end, refdef_t mtl_newrefdef, mtl_model_t *worldModel);
    void lightPoint(entity_t* currententity, vec3_t p, vec3_t color, refdef_t mtl_newrefdef, mtl_model_t *worldModel);
    void setLightLevel(entity_t* currententity, refdef_t mtl_newrefdef, mtl_model_t *worldModel);
};

#endif /* LegacyLight_hpp */
