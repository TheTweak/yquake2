//
//  SharedTypes.h
//  yq2
//
//  Created by SOROKIN EVGENY on 02.10.2022.
//

#ifndef SharedTypes_h
#define SharedTypes_h

#include <simd/simd.h>

// Buffer index values shared between shader and C code to ensure Metal shader buffer inputs match
//   Metal API buffer set calls
enum TexVertexInputIndex {
    TexVertexInputIndexVertices     = 0,
    TexVertexInputIndexViewportSize = 1,
};

enum ParticleInputIndex {
    ParticleInputIndexVertices     = 0,
    ParticleInputIndexMVPMatrix    = 1,
    ParticleInputIndexIdentityM    = 2,
};

enum VertexInputIndex {
    VertexInputIndexVertices  = 0,
    VertexInputIndexMVPMatrix = 1,
    VertexInputIndexTransModelMatrix = 2,
    VertexInputIndexAlpha     = 3
};

// Texture index values shared between shader and C code to ensure Metal shader buffer inputs match
//   Metal API texture set calls
typedef enum TextureIndex {
    TextureIndexBaseColor = 0,
} TextureIndex;

struct TexVertex {
    vector_float2 position;
    vector_float2 texCoordinate;
};

struct Particle {
    vector_float3 position;
    vector_float4 color;
    float size;
    float dist;
};

struct Vertex {
    vector_float3 position;
    vector_float2 texCoordinate;
};
#endif /* SharedTypes_h */
