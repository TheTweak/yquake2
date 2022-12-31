//
//  SkyBox.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 27.12.2022.
//

#include <sstream>

#include "SkyBox.hpp"
#include "../texture/TextureCache.hpp"
#include "../image/Image.hpp"
#include "../utils/Constants.h"
#include "../utils/Utils.hpp"
#include "../legacy/ConsoleVars.h"

static const char* suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static const float skyMin = 1.0 / 512;
static const float skyMax = 511.0 / 512;
static const int skytexorder[6] = {0, 2, 1, 3, 4, 5};
static vec3_t skyclip[6] = {
    {1, 1, 0},
    {1, -1, 0},
    {0, -1, 1},
    {0, 1, 1},
    {1, 0, 1},
    {-1, 0, 1}
};
static int vec_to_st[6][3] = {
    {-2, 3, 1},
    {2, 3, -1},
    
    {1, 3, 2},
    {-1, 3, -2},
    
    {-2, -1, 3},
    {-2, 1, -3}
};
static float skymins[2][6], skymaxs[2][6];
static int st_to_vec[6][3] = {
    {3, -1, 2},
    {-3, 1, 2},
    
    {1, 3, 2},
    {-1, -3, 2},
    
    {-2, -1, 3}, /* 0 degrees yaw, look straight up */
    {2, -1, -3} /* look straight down */
};

SkyBox::SkyBox(std::string name, float rotate, vec3_t skyAxis): rotate(rotate), name(name) {
    VectorCopy(skyAxis, axis);
        
    for (int i = 0; i < 6; i++) {
        std::ostringstream ss;
        ss << "env/" << name << suf[i] << ".tga";
        
        image_s *image = Image::getInstance().FindImage(ss.str().data(), it_sky);
        
        if (!image) {
            ss.str("");
            ss.clear();
            ss << "pics/Skies/" << name << suf[i] << ".m8";
            image = Image::getInstance().FindImage(ss.str().data(), it_sky);
        }
        
        if (image) {
            textureNames[i] = image->path;
            TextureCache::getInstance().addTextureForImage(image);
        }
    }
}

static void MakeSkyVec(float s, float t, int axis, Vertex* vert)
{
    vec3_t v, b;
    int j, k;
    
    float dist = (cvar::r_farsee->value == 0) ? 2300.0f : 4096.0f;
    
    b[0] = s * dist;
    b[1] = t * dist;
    b[2] = dist;
    
    for (j = 0; j < 3; j++)
    {
        k = st_to_vec[axis][j];
        
        if (k < 0)
        {
            v[j] = -b[-k - 1];
        }
        else
        {
            v[j] = b[k - 1];
        }
    }
    
    /* avoid bilerp seam */
    s = (s + 1) * 0.5;
    t = (t + 1) * 0.5;
    
    if (s < skyMin)
    {
        s = skyMin;
    }
    else if (s > skyMax)
    {
        s = skyMax;
    }
    
    if (t < skyMin)
    {
        t = skyMin;
    }
    else if (t > skyMax)
    {
        t = skyMax;
    }
    
    t = 1.0 - t;
    
    VectorCopy(v, vert->position);
    
    vert->texCoordinate[0] = s;
    vert->texCoordinate[1] = t;
}

void SkyBox::render(MTL::RenderCommandEncoder *encoder, vector_uint2 viewportSize, vec3_t origin, refdef_t mtl_newrefdef, simd_float4x4 mvpMatrix, MTL::RenderPipelineState *pipelineState) {
    if (rotate) {   /* check for no sky at all */
        int i;
        for (i = 0; i < 6; i++) {
            if ((skymins[0][i] < skymaxs[0][i]) && (skymins[1][i] < skymaxs[1][i])) {
                break;
            }
        }
        
        if (i == 6) {
            return; /* nothing visible */
        }
    }
    
    simd_float4x4 transMatrix = matrix_identity_float4x4;
    transMatrix.columns[3][0] = origin[0];
    transMatrix.columns[3][1] = origin[1];
    transMatrix.columns[3][2] = origin[2];
    
    simd_float4x4 modMVmat = simd_mul(matrix_identity_float4x4, transMatrix);
    if (rotate != 0.0f) {
        simd_float3 rotAxis = simd_make_float3(axis[0], axis[1], axis[2]);
        modMVmat = simd_mul(modMVmat, Utils::rotate(mtl_newrefdef.time * rotate, rotAxis));
    }
    
    std::array<Vertex, 4> vertices;
    encoder->setRenderPipelineState(pipelineState);
    float alpha = 1.0f;
    for (int i = 0; i < 6; i++) {
        if (rotate != 0.0f) {
            skymins[0][i] = -1;
            skymins[1][i] = -1;
            skymaxs[0][i] = 1;
            skymaxs[1][i] = 1;
        }
        
        if ((skymins[0][i] >= skymaxs[0][i]) || (skymins[1][i] >= skymaxs[1][i])) {
            continue;
        }
        
        auto texture = TextureCache::getInstance().getTexture(textureNames[skytexorder[i]]);
        
        MakeSkyVec( skymins [ 0 ] [ i ], skymins [ 1 ] [ i ], i, &vertices[0] );
        MakeSkyVec( skymins [ 0 ] [ i ], skymaxs [ 1 ] [ i ], i, &vertices[1] );
        MakeSkyVec( skymaxs [ 0 ] [ i ], skymaxs [ 1 ] [ i ], i, &vertices[2] );
        MakeSkyVec( skymaxs [ 0 ] [ i ], skymins [ 1 ] [ i ], i, &vertices[3] );
                
        // convert from 4 vertices (used as triangle fan primitive type in GL renderer)
        // to 6 vertices for Metal
        std::array<Vertex, 6> triangles;
        triangles[0] = vertices[0];
        triangles[1] = vertices[1];
        triangles[2] = vertices[2];
        triangles[3] = vertices[0];
        triangles[4] = vertices[2];
        triangles[5] = vertices[3];
                
        encoder->setVertexBytes(&modMVmat, sizeof(modMVmat), VertexInputIndex::VertexInputIndexTransModelMatrix);
        encoder->setVertexBytes(&mvpMatrix, sizeof(mvpMatrix), VertexInputIndex::VertexInputIndexMVPMatrix);
        encoder->setVertexBytes(triangles.data(), sizeof(Vertex)*triangles.size(), VertexInputIndex::VertexInputIndexVertices);
        encoder->setVertexBytes(&alpha, sizeof(alpha), VertexInputIndex::VertexInputIndexAlpha);
        bool clamp = false;
        encoder->setFragmentBytes(&clamp, sizeof(clamp), TextureIndex::TextureIndexScaleDepth);
        encoder->setFragmentTexture(texture, TextureIndex::TextureIndexBaseColor);
        encoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
    }
}

static void DrawSkyPolygon(int nump, vec3_t vecs) {
    int i, j;
    vec3_t v, av;
    float s, t, dv;
    int axis;
    float *vp;
    
    /* decide which face it maps to */
    VectorCopy(vec3_origin, v);
    
    for (i = 0, vp = vecs; i < nump; i++, vp += 3)
    {
        VectorAdd(vp, v, v);
    }
    
    av[0] = fabs(v[0]);
    av[1] = fabs(v[1]);
    av[2] = fabs(v[2]);
    
    if ((av[0] > av[1]) && (av[0] > av[2]))
    {
        if (v[0] < 0)
        {
            axis = 1;
        }
        else
        {
            axis = 0;
        }
    }
    else if ((av[1] > av[2]) && (av[1] > av[0]))
    {
        if (v[1] < 0)
        {
            axis = 3;
        }
        else
        {
            axis = 2;
        }
    }
    else
    {
        if (v[2] < 0)
        {
            axis = 5;
        }
        else
        {
            axis = 4;
        }
    }
    
    /* project new texture coords */
    for (i = 0; i < nump; i++, vecs += 3)
    {
        j = vec_to_st[axis][2];
        
        if (j > 0)
        {
            dv = vecs[j - 1];
        }
        else
        {
            dv = -vecs[-j - 1];
        }
        
        if (dv < 0.001)
        {
            continue; /* don't divide by zero */
        }
        
        j = vec_to_st[axis][0];
        
        if (j < 0)
        {
            s = -vecs[-j - 1] / dv;
        }
        else
        {
            s = vecs[j - 1] / dv;
        }
        
        j = vec_to_st[axis][1];
        
        if (j < 0)
        {
            t = -vecs[-j - 1] / dv;
        }
        else
        {
            t = vecs[j - 1] / dv;
        }
        
        if (s < skymins[0][axis])
        {
            skymins[0][axis] = s;
        }
        
        if (t < skymins[1][axis])
        {
            skymins[1][axis] = t;
        }
        
        if (s > skymaxs[0][axis])
        {
            skymaxs[0][axis] = s;
        }
        
        if (t > skymaxs[1][axis])
        {
            skymaxs[1][axis] = t;
        }
    }
}

static void ClipSkyPolygon(int nump, vec3_t vecs, int stage) {
    float *norm;
    float *v;
    bool front, back;
    float d, e;
    float dists[MAX_CLIP_VERTS];
    int sides[MAX_CLIP_VERTS];
    vec3_t newv[2][MAX_CLIP_VERTS];
    int newc[2];
    int i, j;
    
    if (nump > MAX_CLIP_VERTS - 2)
    {
        ri.Sys_Error(ERR_DROP, "R_ClipSkyPolygon: MAX_CLIP_VERTS");
    }
    
    if (stage == 6)
    {
        /* fully clipped, so draw it */
        DrawSkyPolygon(nump, vecs);
        return;
    }
    
    front = back = false;
    norm = skyclip[stage];
    
    for (i = 0, v = vecs; i < nump; i++, v += 3)
    {
        d = DotProduct(v, norm);
        
        if (d > ON_EPSILON)
        {
            front = true;
            sides[i] = SIDE_FRONT;
        }
        else if (d < -ON_EPSILON)
        {
            back = true;
            sides[i] = SIDE_BACK;
        }
        else
        {
            sides[i] = SIDE_ON;
        }
        
        dists[i] = d;
    }
    
    if (!front || !back)
    {
        /* not clipped */
        ClipSkyPolygon(nump, vecs, stage + 1);
        return;
    }
    
    /* clip it */
    sides[i] = sides[0];
    dists[i] = dists[0];
    VectorCopy(vecs, (vecs + (i * 3)));
    newc[0] = newc[1] = 0;
    
    for (i = 0, v = vecs; i < nump; i++, v += 3)
    {
        switch (sides[i])
        {
            case SIDE_FRONT:
                VectorCopy(v, newv[0][newc[0]]);
                newc[0]++;
                break;
            case SIDE_BACK:
                VectorCopy(v, newv[1][newc[1]]);
                newc[1]++;
                break;
            case SIDE_ON:
                VectorCopy(v, newv[0][newc[0]]);
                newc[0]++;
                VectorCopy(v, newv[1][newc[1]]);
                newc[1]++;
                break;
        }
        
        if ((sides[i] == SIDE_ON) ||
            (sides[i + 1] == SIDE_ON) ||
            (sides[i + 1] == sides[i]))
        {
            continue;
        }
        
        d = dists[i] / (dists[i] - dists[i + 1]);
        
        for (j = 0; j < 3; j++)
        {
            e = v[j] + d * (v[j + 3] - v[j]);
            newv[0][newc[0]][j] = e;
            newv[1][newc[1]][j] = e;
        }
        
        newc[0]++;
        newc[1]++;
    }
    
    /* continue */
    ClipSkyPolygon(newc[0], newv[0][0], stage + 1);
    ClipSkyPolygon(newc[1], newv[1][0], stage + 1);
}

void SkyBox::addSkySurface(msurface_t *fa, vec3_t origin) {
    vec3_t verts[MAX_CLIP_VERTS];
    glpoly_t *p;
    
    /* calculate vertex values for sky box */
    for (p = fa->polys; p; p = p->next) {
        for (int i = 0; i < p->numverts; i++) {
            VectorSubtract(p->vertices[i].pos, origin, verts[i]);
        }
        
        ClipSkyPolygon(p->numverts, verts[0], 0);
    }
}

void SkyBox::clearSkyBox() {
    for (int i = 0; i < 6; i++)
    {
        skymins[0][i] = skymins[1][i] = 9999;
        skymaxs[0][i] = skymaxs[1][i] = -9999;
    }
}
