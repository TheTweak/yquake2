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

void SkyBox::render(MTL::RenderCommandEncoder *encoder, vector_uint2 viewportSize) {
    
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
