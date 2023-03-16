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

// Represents an intersection between a ray and the scene, returned by the MPSRayIntersector.
// The intersection type is customized using properties of the MPSRayIntersector.
struct Intersection {
    // The distance from the ray origin to the intersection point. Negative if the ray did not
    // intersect the scene.
    float distance;
    
    // The index of the intersected primitive (triangle), if any. Undefined if the ray did not
    // intersect the scene.
    int primitiveIndex;
    
    // The barycentric coordinates of the intersection point, if any. Undefined if the ray did
    // not intersect the scene.
    float2 coordinates;
};

// Represents a three dimensional ray which will be intersected with the scene. The ray type
// is customized using properties of the MPSRayIntersector.
struct Ray {
    // Starting point
    packed_float3 origin;
    
    // Mask which will be bitwise AND-ed with per-triangle masks to filter out certain
    // intersections. This is used to make the light source visible to the camera but not
    // to shadow or secondary rays.
    uint mask;
    
    // Direction the ray is traveling
    packed_float3 direction;
    
    // Maximum intersection distance to accept. This is used to prevent shadow rays from
    // overshooting the light source when checking for visibility.
    float maxDistance;
    
    // The accumulated color along the ray's path so far
    float3 color;
};

constant unsigned int primes[] = {
    2,   3,  5,  7,
    11, 13, 17, 19,
    23, 29, 31, 37,
    41, 43, 47, 53,
};

// Returns the i'th element of the Halton sequence using the d'th prime number as a
// base. The Halton sequence is a "low discrepency" sequence: the values appear
// random but are more evenly distributed then a purely random sequence. Each random
// value used to render the image should use a different independent dimension 'd',
// and each sample (frame) should use a different index 'i'. To decorrelate each
// pixel, a random offset can be applied to 'i'.
float halton(unsigned int i, unsigned int d) {
    unsigned int b = primes[d];
    
    float f = 1.0f;
    float invB = 1.0f / b;
    
    float r = 0;
    
    while (i > 0) {
        f = f * invB;
        r = r + f * (i % b);
        i = i / b;
    }
    
    return r;
}


// Generates rays starting from the camera origin and traveling towards the image plane aligned
// with the camera's coordinate system.
kernel void rayKernel(uint2 tid [[thread_position_in_grid]],
                      // Buffers bound on the CPU. Note that 'constant' should be used for small
                      // read-only data which will be reused across threads. 'device' should be
                      // used for writable data or data which will only be used by a single thread.
                      constant Uniforms &uniforms,
                      device Ray *rays,
                      texture2d<unsigned int> randomTex,
                      texture2d<half, access::write> dstTex)
{
    // Since we aligned the thread count to the threadgroup size, the thread index may be out of bounds
    // of the render target size.
    if (tid.x < uniforms.width && tid.y < uniforms.height) {
        // Compute linear ray index from 2D position
        unsigned int rayIdx = tid.y * uniforms.width + tid.x;
        
        // Ray we will produce
        device Ray & ray = rays[rayIdx];
        
        // Pixel coordinates for this thread
        float2 pixel = (float2) tid;
        
        /*
        // Apply a random offset to random number index to decorrelate pixels
        unsigned int offset = randomTex.read(tid).x;
        
        // Add a random offset to the pixel coordinates for antialiasing
        float2 r = float2(halton(offset + uniforms.frameIndex, 0),
                          halton(offset + uniforms.frameIndex, 1));
        
        pixel += r;
        */
        
        // Map pixel coordinates to -1..1
        float2 uv = (float2) pixel / float2(uniforms.width, uniforms.height);
        uv = uv * 2.0f - 1.0f;
        
        constant Camera & camera = uniforms.camera;
                        
        // Rays start at the camera position
        ray.origin = camera.position;
        
        // Map normalized pixel coordinates into camera's coordinate system
        ray.direction = normalize(uv.x * camera.right +
                                  uv.y * camera.up +
                                  camera.forward);
        // The camera emits primary rays
        ray.mask = RAY_MASK_PRIMARY;
        
        // Don't limit intersection distance
        ray.maxDistance = INFINITY;
        
        // Start with a fully white color. Each bounce will scale the color as light
        // is absorbed into surfaces.
        ray.color = float3(1.0f, 1.0f, 1.0f);
        
        // Clear the destination image to black
        dstTex.write(half4(0.0f, 0.0f, 0.0f, 0.0f), tid);
    }
}

kernel void shadeKernel(uint2 tid [[thread_position_in_grid]],
                        device Ray *rays [[buffer(1)]],
                        device Intersection *intersections [[buffer(0)]],
                        constant Uniforms &uniforms [[buffer(2)]],
                        constant Vertex *vertexArray [[buffer(3)]],
                        constant size_t *vertexTextureIndices [[buffer(4)]],
                        texture2d<half, access::write> dstTex [[ texture(0) ]],
                        const array<texture2d<half, access::sample>, RT_TEXTURE_ARRAY_SIZE> vertexTextures [[ texture(1) ]])
{
    if (tid.x < uniforms.width && tid.y < uniforms.height) {
        unsigned int rayIdx = tid.y * uniforms.width + tid.x;
        device Ray &ray = rays[rayIdx];
        device Intersection &intersection = intersections[rayIdx];

        if (intersection.distance >= 0.0f) {
            // distance is positive if ray hit something
            
            // 1. get primitive index from intersection
            // 2. convert privimitive index to vertices indices
            int v0 = intersection.primitiveIndex * 3 + 0;
            int v1 = intersection.primitiveIndex * 3 + 1;
            int v2 = intersection.primitiveIndex * 3 + 2;
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
            float3 projectedIntersectionCoord = uniforms.mvpMatrix * float3(intersection.coordinates.x, intersection.coordinates.y, 1.0);
            float2 intersection2d = projectedIntersectionCoord.xy;
            half4 color0 = t0.sample(textureSampler, intersection2d);
            half4 color1 = t1.sample(textureSampler, intersection2d);
            half4 color2 = t2.sample(textureSampler, intersection2d);
            
            // Barycentric coordinates sum to one
            float3 uvw;
            uvw.xy = intersection.coordinates;
            uvw.z = 1.0f - uvw.x - uvw.y;
            
            half4 interpolatedColor = uvw.x * color0 + uvw.y * color1 + uvw.z * color2;
            dstTex.write(interpolatedColor, tid);
        }
    }
}

