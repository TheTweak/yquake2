//
//  Created by SOROKIN EVGENY on 18.11.2022.
//

#include <metal_stdlib>
#include <simd/simd.h>
#include "../utils/SharedTypes.h"

using namespace metal;

typedef struct {
    float4 position [[ position ]];
} VertexRasteriserData;

struct FragmentOutput {
    float4 color [[color(0)]];
};

vertex VertexRasteriserData
debugVertexShader(uint vertexID [[ vertex_id ]],
             constant Vertex *vertexArray [[ buffer(VertexInputIndexVertices) ]],
             constant float4x4 *mvpMatrix [[ buffer(VertexInputIndexMVPMatrix) ]],
             constant float4x4 *transMatrix [[ buffer(VertexInputIndexTransModelMatrix) ]])
{
    VertexRasteriserData out;
    Vertex v = vertexArray[vertexID];
    out.position = *mvpMatrix * *transMatrix * float4(v.position, 1.0);
    return out;
}

fragment FragmentOutput
debugFragFunc(VertexRasteriserData in [[stage_in]])
{
    FragmentOutput out;
    out.color = float4(0.0f, 1.0f, 0.0f, 1.0f);
    return out;
}
