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
    float3 staticLight;
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
                     constant float *alpha [[ buffer(VertexInputIndexAlpha) ]],
                     constant float3 *staticLight [[ buffer(VertexInputIndexStaticLight) ]])
{
    VertexRasteriserData out;
    Vertex v = vertexArray[vertexID];
    out.position = *mvpMatrix * *transMatrix * float4(v.position, 1.0);
    out.textureCoordinate = v.texCoordinate;
    out.alpha = *alpha;
    out.staticLight = *staticLight;
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
    if (in.staticLight[0] != 0) {
        out.color[0] *= in.staticLight[0];
    }
    if (in.staticLight[1] != 0) {
        out.color[1] *= in.staticLight[1];
    }
    if (in.staticLight[2] != 0) {
        out.color[2] *= in.staticLight[2];
    }
    if (*scaleDepth) {
        out.depth = in.position.z * 0.1;
    } else {
        out.depth = in.position.z;
    }
    return out;
}
