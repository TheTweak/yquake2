//
//  Utils.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 14.11.2022.
//
#include <cmath>

#include "Utils.hpp"
#include "../../src/common/header/shared.h"

static float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../../src/client/refresh/constants/anorms.h"
};

float Utils::toRadians(float deg) {
    return deg * (pi / 180.0f);
}

bool TexNameTransMatKey::operator==(const TexNameTransMatKey& other) const {
    if (textureName != other.textureName) {
        return false;
    }
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (transModelMat.columns[i][j] != other.transModelMat.columns[i][j]) {
                return false;
            }
        }
    }
    
    return true;
}

/*
 * Returns true if the box is completely outside the frustom
 */
bool Utils::CullBox(vec3_t mins, vec3_t maxs, cplane_t *frustum) {
    int i;

    for (i = 0; i < 4; i++) {
        if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2) {
            return true;
        }
    }

    return false;
}
/*
 * Returns the proper texture for a given time and base texture
 */
image_s* Utils::TextureAnimation(entity_t *currententity, mtexinfo_t *tex)
{
    int c;

    if (!tex->next)
    {
        return tex->image;
    }

    c = currententity->frame % tex->numframes;

    while (c)
    {
        tex = tex->next;
        c--;
    }

    return tex->image;
}

bool Utils::CullAliasModel(vec3_t bbox[8], entity_t *e, cplane_t *frustum) {
    int i;
    vec3_t mins, maxs;
    dmdl_t *paliashdr;
    vec3_t vectors[3];
    vec3_t thismins, oldmins, thismaxs, oldmaxs;
    daliasframe_t *pframe, *poldframe;
    vec3_t angles;

    mtl_model_t* model = e->model;

    paliashdr = (dmdl_t *)model->extradata;

    if ((e->frame >= paliashdr->num_frames) || (e->frame < 0))
    {
        R_Printf(PRINT_DEVELOPER, "R_CullAliasModel %s: no such frame %d\n",
                model->name, e->frame);
        e->frame = 0;
    }

    if ((e->oldframe >= paliashdr->num_frames) || (e->oldframe < 0))
    {
        R_Printf(PRINT_DEVELOPER, "R_CullAliasModel %s: no such oldframe %d\n",
                model->name, e->oldframe);
        e->oldframe = 0;
    }

    pframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
            e->frame * paliashdr->framesize);

    poldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
            e->oldframe * paliashdr->framesize);

    /* compute axially aligned mins and maxs */
    if (pframe == poldframe)
    {
        for (i = 0; i < 3; i++)
        {
            mins[i] = pframe->translate[i];
            maxs[i] = mins[i] + pframe->scale[i] * 255;
        }
    }
    else
    {
        for (i = 0; i < 3; i++)
        {
            thismins[i] = pframe->translate[i];
            thismaxs[i] = thismins[i] + pframe->scale[i] * 255;

            oldmins[i] = poldframe->translate[i];
            oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255;

            if (thismins[i] < oldmins[i])
            {
                mins[i] = thismins[i];
            }
            else
            {
                mins[i] = oldmins[i];
            }

            if (thismaxs[i] > oldmaxs[i])
            {
                maxs[i] = thismaxs[i];
            }
            else
            {
                maxs[i] = oldmaxs[i];
            }
        }
    }

    /* compute a full bounding box */
    for (i = 0; i < 8; i++)
    {
        vec3_t tmp;

        if (i & 1)
        {
            tmp[0] = mins[0];
        }
        else
        {
            tmp[0] = maxs[0];
        }

        if (i & 2)
        {
            tmp[1] = mins[1];
        }
        else
        {
            tmp[1] = maxs[1];
        }

        if (i & 4)
        {
            tmp[2] = mins[2];
        }
        else
        {
            tmp[2] = maxs[2];
        }

        VectorCopy(tmp, bbox[i]);
    }

    /* rotate the bounding box */
    VectorCopy(e->angles, angles);
    angles[YAW] = -angles[YAW];
    AngleVectors(angles, vectors[0], vectors[1], vectors[2]);

    for (i = 0; i < 8; i++)
    {
        vec3_t tmp;

        VectorCopy(bbox[i], tmp);

        bbox[i][0] = DotProduct(vectors[0], tmp);
        bbox[i][1] = -DotProduct(vectors[1], tmp);
        bbox[i][2] = DotProduct(vectors[2], tmp);

        VectorAdd(e->origin, bbox[i], bbox[i]);
    }

    int p, f, aggregatemask = ~0;

    for (p = 0; p < 8; p++)
    {
        int mask = 0;

        for (f = 0; f < 4; f++)
        {
            float dp = DotProduct(frustum[f].normal, bbox[p]);

            if ((dp - frustum[f].dist) < 0)
            {
                mask |= (1 << f);
            }
        }

        aggregatemask &= mask;
    }

    if (aggregatemask)
    {
        return true;
    }

    return false;
}

simd_float4x4 Utils::gluPerspective(float fovy, float aspect, float zNear, float zFar) {
    // calculation of left, right, bottom, top is from R_MYgluPerspective() of old gl backend
    // which seems to be slightly different from the real gluPerspective()
    // and thus also from HMM_Perspective()
    float left, right, bottom, top;
    float A, B, C, D;

    top = zNear * tan(fovy * M_PI / 360.0);
    bottom = -top;

    left = bottom * aspect;
    right = top * aspect;

    // TODO:  stereo stuff
    // left += - gl1_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;
    // right += - gl1_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;

    // the following emulates glFrustum(left, right, bottom, top, zNear, zFar)
    // see https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glFrustum.xml
    //  or http://docs.gl/gl2/glFrustum#description (looks better in non-Firefox browsers)
    A = (right+left)/(right-left);
    B = (top+bottom)/(top-bottom);
    C = -(zFar+zNear)/(zFar-zNear);
    D = -(2.0*zFar*zNear)/(zFar-zNear);

    simd_float4x4 ret = simd_matrix_from_rows(
        simd_make_float4(static_cast<float>((2.0*zNear)/(right-left)), 0, A, 0),
        simd_make_float4(0, static_cast<float>((2.0*zNear)/(top-bottom)), B, 0),
        simd_make_float4(0, 0, C, D),
        simd_make_float4(0, 0, -1.0, 0)
    );

    return ret;
}

void Utils::LerpVerts(bool powerUpEffect, int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3])
{
    int i;

    if (powerUpEffect)
    {
        for (i = 0; i < nverts; i++, v++, ov++, lerp += 4)
        {
            float *normal = r_avertexnormals[verts[i].lightnormalindex];

            lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0] +
                      normal[0] * POWERSUIT_SCALE;
            lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1] +
                      normal[1] * POWERSUIT_SCALE;
            lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2] +
                      normal[2] * POWERSUIT_SCALE;
        }
    }
    else
    {
        for (i = 0; i < nverts; i++, v++, ov++, lerp += 4)
        {
            lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0];
            lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1];
            lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2];
        }
    }
}

simd_float4x4 Utils::rotateAroundAxisZYX(float aroundZdeg, float aroundYdeg, float aroundXdeg) {
    float alpha = toRadians(aroundZdeg);
    float beta = toRadians(aroundYdeg);
    float gamma = toRadians(aroundXdeg);
    
    float sinA = std::sinf(alpha);
    float cosA = std::cosf(alpha);
    float sinB = std::sinf(beta);
    float cosB = std::cosf(beta);
    float sinG = std::sinf(gamma);
    float cosG = std::cosf(gamma);
    
    simd_float4x4 result = simd_matrix_from_rows(
        simd_make_float4(cosA*cosB, cosA*sinB*sinG - sinA*cosG, cosA*sinB*cosG + sinA*sinG, 0),
        simd_make_float4(sinA*cosB, sinA*sinB*sinG + cosA*cosG, sinA*sinB*cosG - cosA*sinG, 0),
        simd_make_float4(-sinB, cosB*sinG, cosB*cosG, 0),
        simd_make_float4(0, 0, 0, 1)
    );
    
    return result;
}

simd_float4x4 Utils::rotateForEntity(entity_t* entity) {
    // angles: pitch (around y), yaw (around z), roll (around x)
    // rot matrices to be multiplied in order Z, Y, X (yaw, pitch, roll)
    simd_float4x4 transMat = rotateAroundAxisZYX(entity->angles[1], -entity->angles[0], -entity->angles[2]);
    
    for (int i = 0; i < 3; i++) {
        transMat.columns[3][i] = entity->origin[i];
    }
    
    return simd_mul(matrix_identity_float4x4, transMat);
}

int Utils::SignbitsForPlane(cplane_t *out) {
    int bits, j;
    
    /* for fast box on planeside test */
    bits = 0;
    
    for (j = 0; j < 3; j++)
    {
        if (out->normal[j] < 0)
        {
            bits |= 1 << j;
        }
    }
    
    return bits;
}
