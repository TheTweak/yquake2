//
//  Created by SOROKIN EVGENY on 18.11.2022.
//

#include <metal_stdlib>
#include <simd/simd.h>
#include "SharedTypes.h"

using namespace metal;

typedef struct {
    float4 position [[ position ]];
    float4 color;
} VertexRasteriserData;

vertex VertexRasteriserData
vertexShader(uint vertexID [[ vertex_id ]],
                     constant Vertex *vertexArray [[ buffer(VertexInputIndexVertices) ]],
                     constant float4x4 *mvpMatrix [[ buffer(VertexInputIndexMVPMatrix) ]],
                     constant float4x4 *identityMatrix [[ buffer(VertexInputIndexIdentityM) ]])
{
    VertexRasteriserData out;
    Vertex v = vertexArray[vertexID];
    out.position = *mvpMatrix * *identityMatrix * float4(v.position, 1.0);
    out.color = float4(0.0, 255.0, 0.0, 100.0);
    return out;
}

fragment float4
fragFunc(VertexRasteriserData in [[stage_in]])
{
    return in.color;
}
