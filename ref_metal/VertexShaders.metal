//
//  Created by SOROKIN EVGENY on 18.11.2022.
//

#include <metal_stdlib>
#include <simd/simd.h>
#include "SharedTypes.h"

using namespace metal;

typedef struct {
    float4 position [[ position ]];
    float2 textureCoordinate;
    float alpha;
} VertexRasteriserData;

vertex VertexRasteriserData
vertexShader(uint vertexID [[ vertex_id ]],
                     constant Vertex *vertexArray [[ buffer(VertexInputIndexVertices) ]],
                     constant float4x4 *mvpMatrix [[ buffer(VertexInputIndexMVPMatrix) ]],
                     constant float4x4 *identityMatrix [[ buffer(VertexInputIndexIdentityM) ]],
                     constant float *alpha [[ buffer(VertexInputIndexAlpha) ]])
{
    VertexRasteriserData out;
    Vertex v = vertexArray[vertexID];
    out.position = *mvpMatrix * *identityMatrix * float4(v.position, 1.0);
    out.textureCoordinate = v.texCoordinate;
    out.alpha = *alpha;
    return out;
}

fragment float4
fragFunc(VertexRasteriserData in [[stage_in]],
         texture2d<half, access::sample> colorTexture [[ texture(TextureIndexBaseColor) ]])
{
    constexpr sampler textureSampler (mag_filter::linear,
                                      min_filter::linear,
                                      address::repeat);
    half4 colorSample = colorTexture.sample(textureSampler, in.textureCoordinate);
    colorSample[3] = in.alpha;
    return float4(colorSample);
}
