//
//  LegacyLight.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 19.12.2022.
//

#include "LegacyLight.hpp"
#include "ConsoleVars.h"
#include "../utils/Constants.h"
#include "../../src/common/header/shared.h"

int LegacyLight::recursiveLightPoint(mnode_t *node, vec3_t start, vec3_t end, refdef_t mtl_newrefdef,
                                     std::shared_ptr<mtl_model_t> worldModel) {
    float front, back, frac;
    int side;
    cplane_t *plane;
    vec3_t mid;
    msurface_t *surf;
    int s, t, ds, dt;
    int i;
    mtexinfo_t *tex;
    byte *lightmap;
    int maps;
    int r;
    
    if (node->contents != -1)
    {
        return -1;     /* didn't hit anything */
    }
    
    /* calculate mid point */
    plane = node->plane;
    front = DotProduct(start, plane->normal) - plane->dist;
    back = DotProduct(end, plane->normal) - plane->dist;
    side = front < 0;
    
    if ((back < 0) == side)
    {
        return recursiveLightPoint(node->children[side], start, end, mtl_newrefdef, worldModel);
    }
    
    frac = front / (front - back);
    mid[0] = start[0] + (end[0] - start[0]) * frac;
    mid[1] = start[1] + (end[1] - start[1]) * frac;
    mid[2] = start[2] + (end[2] - start[2]) * frac;
    
    /* go down front side */
    r = recursiveLightPoint(node->children[side], start, mid, mtl_newrefdef, worldModel);
    
    if (r >= 0)
    {
        return r;     /* hit something */
    }
    
    if ((back < 0) == side)
    {
        return -1;     /* didn't hit anuthing */
    }
    
    /* check for impact on this node */
    VectorCopy(mid, lightspot);
    lightplane = plane;
    
    surf = worldModel->surfaces + node->firstsurface;
    
    for (i = 0; i < node->numsurfaces; i++, surf++)
    {
        if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
        {
            continue; /* no lightmaps */
        }
        
        tex = surf->texinfo;
        
        s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
        t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];
        
        if ((s < surf->texturemins[0]) ||
            (t < surf->texturemins[1]))
        {
            continue;
        }
        
        ds = s - surf->texturemins[0];
        dt = t - surf->texturemins[1];
        
        if ((ds > surf->extents[0]) || (dt > surf->extents[1]))
        {
            continue;
        }
        
        if (!surf->samples)
        {
            return 0;
        }
        
        ds >>= 4;
        dt >>= 4;
        
        lightmap = surf->samples;
        VectorCopy(vec3_origin, pointcolor);
        
        lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);
        
        for (maps = 0; maps < MAX_LIGHTMAPS_PER_SURFACE && surf->styles[maps] != 255;
             maps++)
        {
            const float *rgb;
            int j;
            
            rgb = mtl_newrefdef.lightstyles[surf->styles[maps]].rgb;
            
            /* Apply light level to models */
            for (j = 0; j < 3; j++)
            {
                float scale;
                
                scale = rgb[j] * cvar::r_modulate->value;
                pointcolor[j] += lightmap[j] * scale * (1.0 / 255);
            }
            
            lightmap += 3 * ((surf->extents[0] >> 4) + 1) *
            ((surf->extents[1] >> 4) + 1);
        }
        
        return 1;
    }
    
    /* go down back side */
    return recursiveLightPoint(node->children[!side], mid, end, mtl_newrefdef, worldModel);
}

void LegacyLight::lightPoint(entity_t* currententity, vec3_t p, vec3_t color, refdef_t mtl_newrefdef,
                             std::shared_ptr<mtl_model_t> worldModel) {
    vec3_t end;
    float r;
    int lnum;
    dlight_t *dl;
    vec3_t dist;
    float add;
    
    if (!worldModel->lightdata || !currententity)
    {
        color[0] = color[1] = color[2] = 1.0;
        return;
    }
    
    end[0] = p[0];
    end[1] = p[1];
    end[2] = p[2] - 2048;
    
    // TODO: don't just aggregate the color, but also save position of brightest+nearest light
    //       for shadow position and maybe lighting on model?
    
    r = recursiveLightPoint(worldModel->nodes, p, end, mtl_newrefdef, worldModel);
    
    if (r == -1)
    {
        VectorCopy(vec3_origin, color);
    }
    else
    {
        VectorCopy(pointcolor, color);
    }
    
    /* add dynamic lights */
    dl = mtl_newrefdef.dlights;
    
    for (lnum = 0; lnum < mtl_newrefdef.num_dlights; lnum++, dl++)
    {
        VectorSubtract(currententity->origin,
                       dl->origin, dist);
        add = dl->intensity - VectorLength(dist);
        add *= (1.0f / 256.0f);
        
        if (add > 0)
        {
            VectorMA(color, add, dl->color, color);
        }
    }
    
    VectorScale(color, cvar::r_modulate->value, color);
}

void LegacyLight::setLightLevel(entity_t* currententity, refdef_t mtl_newrefdef, std::shared_ptr<mtl_model_t> worldModel) {
    vec3_t shadelight = {0};
    
    if (mtl_newrefdef.rdflags & RDF_NOWORLDMODEL) {
        return;
    }
    
    /* save off light value for server to look at */
    lightPoint(currententity, mtl_newrefdef.vieworg, shadelight, mtl_newrefdef, worldModel);
    
    /* pick the greatest component, which should be the
     * same as the mono value returned by software */
    if (shadelight[0] > shadelight[1])
    {
        if (shadelight[0] > shadelight[2])
        {
            cvar::r_lightlevel->value = 150 * shadelight[0];
        }
        else
        {
            cvar::r_lightlevel->value = 150 * shadelight[2];
        }
    }
    else
    {
        if (shadelight[1] > shadelight[2])
        {
            cvar::r_lightlevel->value = 150 * shadelight[1];
        }
        else
        {
            cvar::r_lightlevel->value = 150 * shadelight[2];
        }
    }
    
}

