//
//  RayTracer.metal
//  ref_metal
//
//  Created by SOROKIN EVGENY on 11.01.2023.
//

#include <metal_stdlib>
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

kernel void shadeKernel(uint2 tid [[thread_position_in_grid]],
                        device Intersection *intersections)
{
    
}

