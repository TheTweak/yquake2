#include <metal_stdlib>
using namespace metal;

struct Uniforms {
    float4x4 projectionMatrix;
};

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

vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],
                             constant vector_uint2 *viewportSizePointer [[buffer(1)]]) {
    VertexOut out;
    float2 viewportSize = float2(*viewportSizePointer);
    // To convert from positions in pixel space to positions in clip-space,
    //  divide the pixel coordinates by half the size of the viewport.
    // Z is set to 0.0 and w to 1.0 because this is 2D sample.
    out.position = vector_float4(0.0, 0.0, 0.0, 1.0);
    out.position.xy = in.position / viewportSize;
    out.texCoords = in.texCoords;
    out.color = float4(in.color) / float4(255.0);
    return out;
}

fragment half4 fragment_main(VertexOut in [[stage_in]],
                             texture2d<half, access::sample> texture [[texture(0)]]) {
    constexpr sampler linearSampler(coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear);
    half4 texColor = texture.sample(linearSampler, in.texCoords);
//    return half4(in.color) * texColor;
    return half4(in.color);
}
