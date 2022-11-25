//
//  Utils.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 14.11.2022.
//

#include "Utils.hpp"

float Utils::toRadians(float deg) {
    return deg * (pi / 180.0f);
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
