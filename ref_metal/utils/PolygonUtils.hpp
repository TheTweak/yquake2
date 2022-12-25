//
//  PolygonUtils.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 25.12.2022.
//

#ifndef PolygonUtils_hpp
#define PolygonUtils_hpp

#include <array>

#include "SharedTypes.h"
#include "../model/model.h"

namespace PolygonUtils {
    std::array<Vertex, 3> cutTriangle(glpoly_t* poly, int vertexIndex);
};

#endif /* PolygonUtils_hpp */
