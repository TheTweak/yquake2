#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position  [[attribute(0)]];
    float2 texCoords [[attribute(1)]];
    uchar4 color     [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texCoords;
    float4 color;
};

vertex VertexOut vertex_main(VertexIn in [[stage_in]],
                             constant float4x4 *projectionMatrix [[buffer(1)]]) {
    VertexOut out;

    out.position = *projectionMatrix * float4(in.position, 0, 1);
    out.texCoords = in.texCoords;
    out.color = float4(0, 0, 0, 0);
    out.color[0] = in.color[2];
    out.color[2] = in.color[0];
    out.color[1] = in.color[1];
    out.color[3] = 255.0;
    out.color = float4(in.color) / float4(255.0);
    return out;
}

fragment half4 fragment_main(VertexOut in [[stage_in]],
                             texture2d<half, access::sample> texture [[texture(0)]]) {
    constexpr sampler linearSampler(coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear);
    half4 texColor = texture.sample(linearSampler, in.texCoords);
    return half4(in.color) * texColor;
}
