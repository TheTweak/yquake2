//
//  Utils.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 14.11.2022.
//

#ifndef Utils_hpp
#define Utils_hpp

#include <stdio.h>
#include <string>
#include <vector>
#include <optional>
#include <MetalKit/MetalKit.hpp>

#include "SharedTypes.h"
#include "model.h"

#define PIXEL_FORMAT MTL::PixelFormatBGRA8Unorm
#define MAX_FRAMES_IN_FLIGHT 3

#pragma once

namespace Utils {

inline constexpr double pi = 3.14159265358979323846;
float toRadians(float);
bool CullBox(vec3_t mins, vec3_t maxs, cplane_t *frustum);
bool CullAliasModel(vec3_t bbox[8], entity_t *e, cplane_t *frustum);
image_s* TextureAnimation(entity_t *currententity, mtexinfo_t *tex);
simd_float4x4 gluPerspective(float fovy, float aspect, float zNear, float zFar);
void LerpVerts(bool powerUpEffect, int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3]);
simd_float4x4 rotateAroundAxisZYX(float aroundZdeg, float aroundYdeg, float aroundXdeg);

};

struct ImageSize {
    int width, height;
};

struct DrawPicCommandData {
    std::string pic;
    TexVertex textureVertex[6];
};

struct DrawParticleCommandData {
    Particle particle;
};

struct DrawPolyCommandData {
    std::string textureName;
    std::vector<Vertex> vertices;
    std::optional<simd_float4x4> projMat;
    float alpha;
};

struct DrawAliasPolyCommandData {
    std::string textureName;
    std::vector<Vertex> vertices;
    std::optional<simd_float4x4> projMat;
    float alpha;
    MTL::PrimitiveType primitiveType;
};

#endif /* Utils_hpp */
