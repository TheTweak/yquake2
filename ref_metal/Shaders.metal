#include <metal_stdlib>
#include <simd/simd.h>
#include "SharedTypes.h"

using namespace metal;

// The structure that is fed into the vertex shader. We use packed data types to alleviate memory alignment
// issues caused by the float2

// The output of the vertex shader, which will be fed into the fragment shader
//typedef struct {
//    float4 position [[position]];
//    float4 colour;
//} RasteriserData;

struct RasterizerData {
    // The [[position]] attribute qualifier of this member indicates this value is
    // the clip space position of the vertex when this structure is returned from
    // the vertex shader
    float4 position [[position]];

    // Since this member does not have a special attribute qualifier, the rasterizer
    // will interpolate its value with values of other vertices making up the triangle
    // and pass that interpolated value to the fragment shader for each fragment in
    // that triangle.
    float2 textureCoordinate;
};

// Vertex Function
vertex RasterizerData
vertexShader(uint vertexID [[ vertex_id ]],
             constant TexVertex *vertexArray [[ buffer(VertexInputIndexVertices) ]],
             constant vector_uint2 *viewportSizePointer  [[ buffer(VertexInputIndexViewportSize) ]])
{

    RasterizerData out;

    // Index into the array of positions to get the current vertex.
    //   Positions are specified in pixel dimensions (i.e. a value of 100 is 100 pixels from
    //   the origin)
    float2 pixelSpacePosition = vertexArray[vertexID].position.xy;

    // Get the viewport size and cast to float.
    float2 viewportSize = float2(*viewportSizePointer);

    // To convert from positions in pixel space to positions in clip-space,
    //  divide the pixel coordinates by half the size of the viewport.
    // Z is set to 0.0 and w to 1.0 because this is 2D sample.
    out.position = vector_float4(0.0, 0.0, 0.0, 1.0);
    out.position.xy = pixelSpacePosition / (viewportSize / 2.0);

    // Pass the input textureCoordinate straight to the output RasterizerData. This value will be
    //   interpolated with the other textureCoordinate values in the vertices that make up the
    //   triangle.
    out.textureCoordinate = vertexArray[vertexID].texCoordinate;

    return out;
}

// Fragment function
fragment uint4
samplingShader(RasterizerData in [[stage_in]],
               texture2d<uint> colorTexture [[ texture(TextureIndexBaseColor) ]])
{
    constexpr sampler textureSampler (mag_filter::linear,
                                      min_filter::linear);

    // Sample the texture to obtain a color
    const uint4 colorSample = colorTexture.sample(textureSampler, in.textureCoordinate);

    // return the color of the texture
    return colorSample;
}


/*
vertex RasteriserData vertFunc(uint vertexID [[vertex_id]],
                                        constant Vertex *vertices [[buffer(0)]]) {
    
    RasteriserData out;
    //out.position = float4(0.0, 0.0, 0.0, 1.0);
    out.position = vertices[vertexID].position;
    out.colour = vertices[vertexID].colour;

    // Both the colour and the clip space position will be interpolated in this data structure
    return out;
}

fragment float4 fragFunc(RasteriserData in [[stage_in]]) {
    return in.colour;
}
*/
