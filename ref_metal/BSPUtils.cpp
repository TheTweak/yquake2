//
//  BSP.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 22.11.2022.
//

#include "BSPUtils.hpp"

mleaf_t* BSPUtils::PointInLeaf(vec_t *p, mtl_model_t *model) {
    mnode_t *node;
    float d;
    cplane_t *plane;

    if (!model || !model->nodes)
    {
        ri.Sys_Error(ERR_DROP, "%s: bad model", __func__);
    }

    node = model->nodes;

    while (1)
    {
        if (node->contents != -1)
        {
            return (mleaf_t *)node;
        }

        plane = node->plane;
        d = DotProduct(p, plane->normal) - plane->dist;

        if (d > 0)
        {
            node = node->children[0];
        }
        else
        {
            node = node->children[1];
        }
    }

    return NULL; /* never reached */
}
