//
//  BinarySpacePart.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 26.12.2022.
//

#include "BinarySpacePart.hpp"
#include "../utils/Utils.hpp"
#include "../legacy/ConsoleVars.h"
#include "../model/Model.hpp"

void BinarySpacePart::markLeaves(int *_oldViewCluster, int *_oldViewCluster2, int _viewCluster, int _viewCluster2,
                                 std::shared_ptr<mtl_model_t> worldModel) {
    if (*_oldViewCluster == _viewCluster &&
        *_oldViewCluster2 == _viewCluster2 &&
        !cvar::r_novis->value &&
        _viewCluster != -1) {
        return;
    }
    
    if (cvar::r_lockpvs->value) {
        return;
    }
    
    _visFrameCount++;
    _oldViewCluster = &_viewCluster;
    _oldViewCluster2 = &_viewCluster2;
    
    if (cvar::r_novis->value || _viewCluster == -1 || !worldModel->vis) {
        for (int i = 0; i < worldModel->numleafs; i++) {
            worldModel->leafs[i].visframe = _visFrameCount;
        }
        
        for (int i = 0; i < worldModel->numnodes; i++) {
            worldModel->nodes[i].visframe = _visFrameCount;
        }
        
        return;
    }
    
    const byte* vis = Model::getInstance().clusterPVS(_viewCluster, worldModel.get());
    
    if (_viewCluster2 != _viewCluster) {
        YQ2_ALIGNAS_TYPE(int) byte fatvis[MAX_MAP_LEAFS / 8];
        
        memcpy(fatvis, vis, (worldModel->numleafs + 7) / 8);
        
        vis = Model::getInstance().clusterPVS(_viewCluster2, worldModel.get());
        
        int c = (worldModel->numleafs + 31) / 32;
        for (int i = 0; i < c; i++) {
            ((int *)fatvis)[i] |= ((int *)vis)[i];
        }
        
        vis = fatvis;
    }
    
    mleaf_t *leaf;
    mnode_t *node;
    int i;
    for (i = 0, leaf = worldModel->leafs; i < worldModel->numleafs; i++, leaf++) {
        int cluster = leaf->cluster;
        
        if (cluster == -1) {
            continue;
        }
        
        if (vis[cluster >> 3] & (1 << (cluster & 7))) {
            node = (mnode_t *)leaf;
            
            do {
                if (node->visframe == _visFrameCount) {
                    break;
                }
                
                node->visframe = _visFrameCount;
                node = node->parent;
            }
            while (node);
        }
    }
}

void BinarySpacePart::recursiveWorldNode(entity_t* currentEntity, mnode_t* node, cplane_t frustum[4],
                                         refdef_t mtl_newrefdef, int _frameCount, vec3_t modelOrigin,
                                         msurface_t* alphaSurfaces, std::shared_ptr<mtl_model_t> worldModel,
                                         SkyBox &skyBox, vec3_t origin) {
    if (node->contents == CONTENTS_SOLID ||
        node->visframe != _visFrameCount ||
        Utils::CullBox(node->minmaxs, node->minmaxs+3, frustum)) {
        return;
    }
    
    mleaf_t *pleaf;
    /* if a leaf node, draw stuff */
    if (node->contents != -1) {
        pleaf = (mleaf_t *)node;
        
        /* check for door connected areas */
        if (mtl_newrefdef.areabits) {
            if (!(mtl_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7)))) {
                return; /* not visible */
            }
        }
        
        msurface_t **mark = pleaf->firstmarksurface;
        int c = pleaf->nummarksurfaces;
        
        if (c) {
            do {
                (*mark)->visframe = _frameCount;
                mark++;
            }
            while (--c);
        }
        
        return;
    }
    
    /* node is just a decision point, so go down the apropriate
     sides find which side of the node we are on */
    cplane_t *plane = node->plane;
    float dot;
    
    switch (plane->type) {
        case PLANE_X:
            dot = modelOrigin[0] - plane->dist;
            break;
        case PLANE_Y:
            dot = modelOrigin[1] - plane->dist;
            break;
        case PLANE_Z:
            dot = modelOrigin[2] - plane->dist;
            break;
        default:
            dot = DotProduct(modelOrigin, plane->normal) - plane->dist;
            break;
    }
    
    int side, sidebit;
    
    if (dot >= 0) {
        side = 0;
        sidebit = 0;
    } else {
        side = 1;
        sidebit = SURF_PLANEBACK;
    }
    
    /* recurse down the children, front side first */
    recursiveWorldNode(currentEntity, node->children[side], frustum, mtl_newrefdef, _frameCount, modelOrigin,
                       alphaSurfaces, worldModel, skyBox, origin);
    
    int c;
    msurface_t *surf;
    image_s *image;
    /* draw stuff */
    for (c = node->numsurfaces, surf = worldModel->surfaces + node->firstsurface;
         c; c--, surf++) {
        if (surf->visframe != _frameCount) {
            continue;
        }
        
        if ((surf->flags & SURF_PLANEBACK) != sidebit) {
            continue; /* wrong side */
        }
        
        if (surf->texinfo->flags & SURF_SKY) {
            /* just adds to visible sky bounds */
            skyBox.addSkySurface(surf, origin);
        } else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
            /* add to the translucent chain */
            surf->texturechain = alphaSurfaces;
            alphaSurfaces = surf;
            alphaSurfaces->texinfo->image = Utils::TextureAnimation(currentEntity, surf->texinfo);
        } else {
            /* the polygon is visible, so add it to the texture sorted chain */
            image = Utils::TextureAnimation(currentEntity, surf->texinfo);
            surf->texturechain = image->texturechain;
            image->texturechain = surf;
        }
    }
    
    /* recurse down the back side */
    recursiveWorldNode(currentEntity, node->children[!side], frustum, mtl_newrefdef, _frameCount, modelOrigin,
                       alphaSurfaces, worldModel, skyBox, origin);
}
