//
//  WorldGeometry.h
//  yq2
//
//  Created by SOROKIN EVGENY on 18.03.2023.
//

#ifndef WorldGeometry_h
#define WorldGeometry_h

#include <Metal/Metal.hpp>
#include <vector>

struct WorldGeometry {
    MTL::Buffer* vertexBuffer;
    size_t vertexCount;
    size_t textureCount;
    std::vector<MTL::Texture*> vertexTextures;
    std::vector<size_t> vertexTextureIndices;
};

#endif /* WorldGeometry_h */
