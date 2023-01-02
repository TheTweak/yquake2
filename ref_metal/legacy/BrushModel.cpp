//
//  BrushModel.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 25.12.2022.
//

#include "BrushModel.hpp"
#include "../utils/Constants.h"
#include "../texture/TextureCache.hpp"
#include "../utils/PolygonUtils.hpp"
#include "../MetalRenderer.hpp"

void BrushModel::createPolygons(entity_t* entity, model_s* model, cplane_t frustum[4], refdef_t mtl_newrefdef, vec3_t modelOrigin,
                                std::unordered_map<TexNameTransMatKey, Polygon, TexNameTransMatKeyHash> &worldPolygonsByTexture,
                                MTL::RenderPipelineState *renderPipelineState) {
    if (model->nummodelsurfaces == 0) {
        return;
    }
    
    bool rotated;
    vec3_t mins, maxs;
    
    if (entity->angles[0] || entity->angles[1] || entity->angles[2]) {
        rotated = true;
        
        for (int i = 0; i < 3; i++) {
            mins[i] = entity->origin[i] - model->radius;
            maxs[i] = entity->origin[i] + model->radius;
        }
    } else {
        rotated = false;
        VectorAdd(entity->origin, model->mins, mins);
        VectorAdd(entity->origin, model->maxs, maxs);
    }
    
    if (Utils::CullBox(mins, maxs, frustum)) {
        return;
    }
    
    VectorSubtract(mtl_newrefdef.vieworg, entity->origin, modelOrigin);
    
    if (rotated) {
        vec3_t temp;
        vec3_t forward, right, up;
        
        VectorCopy(modelOrigin, temp);
        AngleVectors(entity->angles, forward, right, up);
        modelOrigin[0] = DotProduct(temp, forward);
        modelOrigin[1] = -DotProduct(temp, right);
        modelOrigin[2] = DotProduct(temp, up);
    }
    entity->angles[0] = -entity->angles[0];
    entity->angles[2] = -entity->angles[2];
    simd_float4x4 transModelMat = Utils::rotateForEntity(entity);
    entity->angles[0] = -entity->angles[0];
    entity->angles[2] = -entity->angles[2];
    
    msurface_t *psurf = &model->surfaces[model->firstmodelsurface];
    
    for (int i = 0; i < model->nummodelsurfaces; i++, psurf++) {
        cplane_t *pplane = psurf->plane;
        
        float dot = DotProduct(modelOrigin, pplane->normal) - pplane->dist;
        
        /* draw the polygon */
        if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
            (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
            if (psurf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
                psurf->texturechain = MetalRenderer::getInstance().getAlphaSurfaces();
                MetalRenderer::getInstance().setAlphaSurfaces(psurf);
            } else {
                image_s* image = Utils::TextureAnimation(entity, psurf->texinfo);
                TextureCache::getInstance().addTextureForImage(image);
                glpoly_t *p = psurf->polys;
                
                if (!p || !p->numverts) continue;
                
                TexNameTransMatKey key{image->path, transModelMat};
                worldPolygonsByTexture.insert({key, Polygon{image->path, transModelMat, 1.0f, renderPipelineState}});
                
                for (int i = 2; i < p->numverts; i++) {
                    auto vertexArray = PolygonUtils::cutTriangle(p, i);
                    for (int j = 0; j < vertexArray.size(); j++) {
                        worldPolygonsByTexture[key].addVertex(vertexArray[j]);
                    }
                }
            }
        }
    }

}
