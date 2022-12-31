//
//  Created by SOROKIN EVGENY on 18.11.2022.
//

#include <metal_stdlib>
#include <simd/simd.h>
#include "../utils/SharedTypes.h"

using namespace metal;

typedef struct {
    float4 position [[ position ]];
    float2 textureCoordinate;
    float alpha;
} VertexRasteriserData;

struct FragmentOutput {
    float4 color [[color(0)]];
    float depth [[depth(any)]];
};

vertex VertexRasteriserData
vertexShader(uint vertexID [[ vertex_id ]],
                     constant Vertex *vertexArray [[ buffer(VertexInputIndexVertices) ]],
                     constant float4x4 *mvpMatrix [[ buffer(VertexInputIndexMVPMatrix) ]],
                     constant float4x4 *transMatrix [[ buffer(VertexInputIndexTransModelMatrix) ]],
                     constant float *alpha [[ buffer(VertexInputIndexAlpha) ]])
{
    VertexRasteriserData out;
    Vertex v = vertexArray[vertexID];
    out.position = *mvpMatrix * *transMatrix * float4(v.position, 1.0);
    out.textureCoordinate = v.texCoordinate;
    out.alpha = *alpha;
    return out;
}

fragment FragmentOutput
fragFunc(VertexRasteriserData in [[stage_in]],
         constant bool *scaleDepth [[ buffer(TextureIndexScaleDepth) ]],
         texture2d<half, access::sample> colorTexture [[ texture(TextureIndexBaseColor) ]])
{
    constexpr sampler textureSampler (mag_filter::linear,
                                      min_filter::linear,
                                      address::repeat,
                                      mip_filter::linear);
    half4 colorSample = colorTexture.sample(textureSampler, in.textureCoordinate);
    colorSample[3] = in.alpha;
    FragmentOutput out;
    out.color = float4(colorSample);
    if (*scaleDepth) {
        out.depth = in.position.z * 0.1;
    } else {
        out.depth = in.position.z;
    }
    return out;
}
