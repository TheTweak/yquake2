//
//  BSP.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 22.11.2022.
//

#ifndef BSP_hpp
#define BSP_hpp

#include <stdio.h>

#include "model.h"

namespace BSPUtils {

mleaf_t* PointInLeaf(vec3_t p, mtl_model_t *model);

};

#endif /* BSP_hpp */
