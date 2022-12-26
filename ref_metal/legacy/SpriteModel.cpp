//
//  SpriteModel.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 26.12.2022.
//

#include "SpriteModel.hpp"

Sprite SpriteModel::createSprite(entity_t* e, model_s* currentmodel, vec3_t vup, vec3_t vright,
                                 MTL::RenderPipelineState *renderPipelineState) {
    dsprite_t* psprite = (dsprite_t *) currentmodel->extradata;
    
    e->frame %= psprite->numframes;
    dsprframe_t* frame = &psprite->frames[e->frame];
    
    float* up = vup;
    float* right = vright;
    
    float alpha = 1.0f;
    if (e->flags & RF_TRANSLUCENT) {
        alpha = e->alpha;
    }
    
    mtl_3D_vtx_t verts[4];
    
    verts[0].texCoord[0] = 0;
    verts[0].texCoord[1] = 1;
    verts[1].texCoord[0] = 0;
    verts[1].texCoord[1] = 0;
    verts[2].texCoord[0] = 1;
    verts[2].texCoord[1] = 0;
    verts[3].texCoord[0] = 1;
    verts[3].texCoord[1] = 1;
    
    VectorMA( e->origin, -frame->origin_y, up, verts[0].pos );
    VectorMA( verts[0].pos, -frame->origin_x, right, verts[0].pos );
    
    VectorMA( e->origin, frame->height - frame->origin_y, up, verts[1].pos );
    VectorMA( verts[1].pos, -frame->origin_x, right, verts[1].pos );
    
    VectorMA( e->origin, frame->height - frame->origin_y, up, verts[2].pos );
    VectorMA( verts[2].pos, frame->width - frame->origin_x, right, verts[2].pos );
    
    VectorMA( e->origin, -frame->origin_y, up, verts[3].pos );
    VectorMA( verts[3].pos, frame->width - frame->origin_x, right, verts[3].pos );
    
    image_s *image = currentmodel->skins[e->frame];
    Quad q;
    q[0] = {
        {verts[0].pos[0], verts[0].pos[1]},
        {verts[0].texCoord[0], verts[0].texCoord[1]}
    };
    q[1] = {
        {verts[1].pos[0], verts[1].pos[1]},
        {verts[1].texCoord[0], verts[1].texCoord[1]}
    };
    q[2] = {
        {verts[2].pos[0], verts[2].pos[1]},
        {verts[2].texCoord[0], verts[2].texCoord[1]}
    };
    
    q[3] = {
        {verts[0].pos[0], verts[0].pos[1]},
        {verts[0].texCoord[0], verts[0].texCoord[1]}
    };
    q[4] = {
        {verts[3].pos[0], verts[3].pos[1]},
        {verts[3].texCoord[0], verts[3].texCoord[1]}
    };
    q[5] = {
        {verts[2].pos[0], verts[2].pos[1]},
        {verts[2].texCoord[0], verts[2].texCoord[1]}
    };
    Sprite sprite{image->path, renderPipelineState};
    sprite.setQuad(q);
    return sprite;
}
