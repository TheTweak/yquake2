//
//  ParticleShaders.metal
//  ref_metal
//
//  Created by SOROKIN EVGENY on 18.11.2022.
//

#include <metal_stdlib>
#include <simd/simd.h>
#include "SharedTypes.h"

using namespace metal;

typedef struct {
    float4 position [[ position ]];
    float pointSize [[ point_size ]];
    float4 color;
} ParticleRasteriserData;

vertex ParticleRasteriserData
particleVertexShader(uint vertexID [[ vertex_id ]],
                     constant Particle *particleArray [[ buffer(ParticleInputIndexVertices) ]],
                     constant float4x4 *mvpMatrix [[ buffer(ParticleInputIndexMVPMatrix) ]],
                     constant float4x4 *identityMatrix [[ buffer(ParticleInputIndexIdentityM) ]])
{
    ParticleRasteriserData out;
    Particle p = particleArray[vertexID];
    out.position = *mvpMatrix * *identityMatrix * float4(p.position, 1.0);
    out.color = p.color;
    out.pointSize = p.size * p.dist * 0.0001;    
    return out;
}

fragment float4
particleFragFunc(ParticleRasteriserData in [[stage_in]])
{
    return in.color;
}
