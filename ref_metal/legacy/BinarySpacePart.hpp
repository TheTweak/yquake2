//
//  BinarySpacePart.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 26.12.2022.
//

#ifndef BinarySpacePart_hpp
#define BinarySpacePart_hpp

#include "../../src/client/vid/header/ref.h"
#include "../model/model.h"
#include "../render/SkyBox.hpp"

class BinarySpacePart {
    int _visFrameCount = 0; /* bumped when going to a new PVS */    
public:
    void recursiveWorldNode(entity_t* currentEntity, mnode_t* node, cplane_t frustum[4], refdef_t mtl_newrefdef,
                            int _frameCount, vec3_t modelOrigin, mtl_model_t *worldModel,
                            SkyBox &skyBox, vec3_t origin);
    void markLeaves(int *_oldViewCluster, int *_oldViewCluster2, int _viewCluster, int _viewCluster2, mtl_model_t *worldModel);
};

#endif /* BinarySpacePart_hpp */
