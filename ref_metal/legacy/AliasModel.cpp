//
//  AliasModel.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 25.12.2022.
//

#include "AliasModel.hpp"
#include "../utils/Utils.hpp"
#include "../legacy/ConsoleVars.h"
#include "../texture/TextureCache.hpp"

Polygon* AliasModel::createPolygon(entity_t* entity, cplane_t frustum[4], LegacyLight legacyLight,
                                   refdef_t mtl_newrefdef, mtl_model_t *worldModel,
                                   simd_float4x4 &modelViewMatrix, MTL::RenderPipelineState *renderPipelineState) {
    vec3_t bbox[8];
    if (!(entity->flags & RF_WEAPONMODEL) && Utils::CullAliasModel(bbox, entity, frustum)) {
        return NULL;
    }
    
    if ((entity->flags & RF_WEAPONMODEL) && cvar::gl_lefthand->value == 2) {
        return NULL;
    }
    
    mtl_model_t* model = entity->model;
    dmdl_t *paliashdr = (dmdl_t *) model->extradata;
    
    vec3_t shadelight;
    
    /* get lighting information */
    if (entity->flags &
        (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED |
         RF_SHELL_BLUE | RF_SHELL_DOUBLE))
    {
    }
    else if (entity->flags & RF_FULLBRIGHT)
    {
    }
    else
    {
        legacyLight.lightPoint(entity, entity->origin, shadelight, mtl_newrefdef, worldModel);
        
        /* player lighting hack for communication back to server */
        if (entity->flags & RF_WEAPONMODEL)
        {
            /* pick the greatest component, which should be
             the same as the mono value returned by software */
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
    }
    
    if (entity->flags & RF_DEPTHHACK) {
        //gl set depth range todo: alternative in metal
    }
    
    std::optional<simd_float4x4> projMatOpt = std::nullopt;
    if (entity->flags & RF_WEAPONMODEL) {
        float screenaspect = (float)mtl_newrefdef.width / mtl_newrefdef.height;
        float dist = (cvar::r_farsee->value == 0) ? 4096.0f : 8192.0f;
        
        simd_float4x4 projMat;
        if (cvar::r_gunfov->value < 0) {
            projMat = Utils::gluPerspective(mtl_newrefdef.fov_y, screenaspect, 2, dist);
        } else {
            projMat = Utils::gluPerspective(cvar::r_gunfov->value, screenaspect, 2, dist);
        }
        
        if (cvar::gl_lefthand->value == 1.0f) {
            // to mirror gun so it's rendered left-handed, just invert X-axis column
            // of projection matrix
            for(int i=0; i<4; ++i) {
                projMat.columns[0][i] = -projMat.columns[0][i];
            }
        }
        projMatOpt = simd_mul(projMat, modelViewMatrix);
    }
    entity->angles[PITCH] = -entity->angles[PITCH];
    simd_float4x4 transModelMat = Utils::rotateForEntity(entity);
    entity->angles[PITCH] = -entity->angles[PITCH];
    
    image_s *skin;
    if (entity->skin) {
        skin = entity->skin;
    } else {
        if (entity->skinnum >= MAX_MD2SKINS) {
            skin = model->skins[0];
        } else {
            skin = model->skins[entity->skinnum];
            if (!skin) {
                skin = model->skins[0];
            }
        }
    }
    assert(skin);
    
    if ((entity->frame >= paliashdr->num_frames) ||
        (entity->frame < 0)) {
        R_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such frame %d\n",
                 model->name, entity->frame);
        entity->frame = 0;
        entity->oldframe = 0;
    }
    
    if ((entity->oldframe >= paliashdr->num_frames) ||
        (entity->oldframe < 0)) {
        R_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such oldframe %d\n",
                 model->name, entity->oldframe);
        entity->frame = 0;
        entity->oldframe = 0;
    }
    
    {
        bool colorOnly = 0 != (entity->flags &
                               (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE |
                                RF_SHELL_HALF_DAM));
        
        daliasframe_t *frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->frame * paliashdr->framesize);
        
        dtrivertx_t *v, *ov, *verts;
        verts = frame->verts;
        v = verts;
        
        daliasframe_t *oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + entity->oldframe * paliashdr->framesize);
        ov = oldframe->verts;
        
        int *order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
        
        float alpha;
        if (entity->flags & RF_TRANSLUCENT) {
            alpha = entity->alpha * 0.666f;
        } else {
            alpha = 1.0;
        }
        
        vec3_t delta;
        _VectorSubtract(entity->oldorigin, entity->origin, delta);
        vec3_t vectors[3];
        AngleVectors(entity->angles, vectors[0], vectors[1], vectors[2]);
        
        vec3_t move;
        move[0] = DotProduct(delta, vectors[0]); /* forward */
        move[1] = -DotProduct(delta, vectors[1]); /* left */
        move[2] = DotProduct(delta, vectors[2]); /* up */
        
        VectorAdd(move, oldframe->translate, move);
        
        float backlerp = entity->backlerp;
        float frontlerp = 1.0 - backlerp;
        float *lerp;
        
        vec3_t frontv, backv;
        for (int i = 0; i < 3; i++) {
            move[i] = backlerp * move[i] + frontlerp * frame->translate[i];
            
            frontv[i] = frontlerp * frame->scale[i];
            backv[i] = backlerp * oldframe->scale[i];
        }
        
        lerp = s_lerped[0];
        Utils::LerpVerts(colorOnly, paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);
        
        int count;
        int index_xyz;
        
        TextureCache::getInstance().addTextureForImage(skin);
        Polygon *aliasModel = new Polygon(skin->path, transModelMat, alpha, renderPipelineState);
        aliasModel->setClamp(entity->flags & RF_WEAPONMODEL);
        if (projMatOpt) {
            aliasModel->setMVP(projMatOpt.value());
        }
        while (1) {
            
            count = *order++;
            
            if (!count) {
                break;
            }
            
            if (count < 0) {
                count = -count;
                aliasModel->setIsTriangle(true);
            } else {
                aliasModel->setIsTriangle(false);
            }
            
            std::vector<Vertex> vertices;
            if (colorOnly) {
                // todo
            } else {
                for (int i = 0; i < count; i++) {
                    Vertex v;
                    v.texCoordinate[0] = ((float *) order)[0];
                    v.texCoordinate[1] = ((float *) order)[1];
                    
                    index_xyz = order[2];
                    
                    order += 3;
                    for (int j = 0; j < 3; j++) {
                        v.position[j] = s_lerped[index_xyz][j];
                    }
                    
                    vertices.push_back(v);
                }
            }
            
            if (aliasModel->isTriangle()) {
                for (int i = 1; i < count-1; i++) {
                    aliasModel->addVertex(vertices.at(0));
                    aliasModel->addVertex(vertices.at(i));
                    aliasModel->addVertex(vertices.at(i+1));
                }
            } else {
                int i;
                for (i = 1; i < count - 2; i += 2) {
                    aliasModel->addVertex(vertices.at(i-1));
                    aliasModel->addVertex(vertices.at(i));
                    aliasModel->addVertex(vertices.at(i+1));
                    
                    aliasModel->addVertex(vertices.at(i));
                    aliasModel->addVertex(vertices.at(i+2));
                    aliasModel->addVertex(vertices.at(i+1));
                }
                if (i < count - 1) {
                    aliasModel->addVertex(vertices.at(i-1));
                    aliasModel->addVertex(vertices.at(i));
                    aliasModel->addVertex(vertices.at(i+1));
                }
            }
        }
        return aliasModel;
    }
}
