//
//  RayTracer.metal
//  ref_metal
//
//  Created by SOROKIN EVGENY on 11.01.2023.
//

#include <metal_stdlib>

#include "../utils/SharedTypes.h"
#include "../utils/Constants.h"

using namespace metal;

kernel void shadeKernel(uint2 tid [[thread_position_in_grid]],                        
                        metal::raytracing::primitive_acceleration_structure accelStructure [[buffer(0)]],
                        constant Uniforms &uniforms [[buffer(2)]],
                        constant Vertex *vertexArray [[buffer(3)]],
                        constant size_t *vertexTextureIndices [[buffer(4)]],
                        texture2d<half, access::write> dstTex [[ texture(0) ]],
                        const array<texture2d<half, access::sample>, RT_TEXTURE_ARRAY_SIZE> vertexTextures [[ texture(1) ]])
{
    if (tid.x < uniforms.width && tid.y < uniforms.height) {
        
        metal::raytracing::intersector<metal::raytracing::triangle_data> i;
        typename metal::raytracing::intersector<metal::raytracing::triangle_data>::result_type intersection;
        
        i.accept_any_intersection(false);

        constant Camera & camera = uniforms.camera;
        
        metal::raytracing::ray r;
        // Map pixel coordinates to -1..1
        // Pixel coordinates for this thread
        float2 pixel = (float2) tid;
        float2 pixelUv = (float2) pixel / float2(uniforms.width, uniforms.height);
        pixelUv = pixelUv * 2.0f - 1.0f;
        r.direction = normalize(pixelUv.x * camera.right +
                                pixelUv.y * camera.up +
                                camera.forward);
        r.origin = camera.position;;
        r.max_distance = INFINITY;
        
        intersection = i.intersect(r, accelStructure);
        
        // Stop if the ray didn't hit anything and has bounced out of the scene.
        if (intersection.type == metal::raytracing::intersection_type::none) {
            return;
        }

        // 1. get primitive index from intersection
        // 2. convert privimitive index to vertices indices
        int v0 = intersection.primitive_id * 3 + 0;
        int v1 = intersection.primitive_id * 3 + 1;
        int v2 = intersection.primitive_id * 3 + 2;        
        // 3. get texture index from vertex index
        size_t ti0 = vertexTextureIndices[v0];
        size_t ti1 = vertexTextureIndices[v1];
        size_t ti2 = vertexTextureIndices[v2];
        // 4. get texture from vertexTextures array
        texture2d<half, access::sample> t0 = vertexTextures[ti0];
        texture2d<half, access::sample> t1 = vertexTextures[ti1];
        texture2d<half, access::sample> t2 = vertexTextures[ti2];
        // 5. sample color from texture
        constexpr sampler textureSampler (mag_filter::linear,
                                          min_filter::linear,
                                          address::repeat,
                                          mip_filter::linear);
        
        half4 color0 = t0.sample(textureSampler, vertexArray[v0].texCoordinate);
        half4 color1 = t1.sample(textureSampler, vertexArray[v1].texCoordinate);
        half4 color2 = t2.sample(textureSampler, vertexArray[v2].texCoordinate);
        
        // Barycentric coordinates sum to one
        float2 uv = intersection.triangle_barycentric_coord;
        half4 interpolatedColor = (1.0f - uv.x - uv.y) * color0 + uv.x * color1 + uv.y * color2;
        dstTex.write(interpolatedColor, tid);
    }
}

