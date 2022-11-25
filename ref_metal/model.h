/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Header for the model stuff.
 *
 * =======================================================================
 */

#ifndef model_h
#define model_h
#define MAXLIGHTMAPS 4

#pragma once

#include <string>
#include "../src/client/refresh/ref_shared.h"

enum {
    SIDE_FRONT = 0,
    SIDE_BACK = 1,
    SIDE_ON = 2
};

enum {
    SURF_PLANEBACK = 2,
    SURF_DRAWSKY = 4,
    SURF_DRAWTURB = 0x10,
    SURF_DRAWBACKGROUND = 0x40,
    SURF_UNDERWATER = 0x80
};

// used for vertex array elements when drawing brushes, sprites, sky and more
// (ok, it has the layout used for rendering brushes, but is not used there)
typedef struct mtl_3D_vtx_s {
    vec3_t pos;
    float texCoord[2];
    float lmTexCoord[2]; // lightmap texture coordinate (sometimes unused)
    vec3_t normal;
    size_t lightFlags; // bit i set means: dynlight i affects surface
} mtl_3D_vtx_t;

// used for vertex array elements when drawing models
typedef struct mtl_alias_vtx_s {
    float pos[3];
    float texCoord[2];
    float color[4];
} mtl_alias_vtx_t;

/* in memory representation */
typedef struct
{
    vec3_t position;
} mvertex_t;

typedef struct
{
    vec3_t mins, maxs;
    vec3_t origin; /* for sounds or lights */
    float radius;
    int headnode;
    int visleafs; /* not including the solid leaf 0 */
    int firstface, numfaces;
} mmodel_t;

typedef struct
{
    unsigned short v[2];
    unsigned int cachededgeoffset;
} medge_t;

typedef struct glpoly_s
{
    struct  glpoly_s *next;
    struct  glpoly_s *chain;
    int numverts;
    int flags; /* for SURF_UNDERWATER (not needed anymore?) */
    mtl_3D_vtx_t vertices[4]; /* variable sized */
} glpoly_t;

typedef struct mtexinfo_s
{
    float vecs[2][4];
    int flags;
    int numframes;
    struct mtexinfo_s *next; /* animation chain */
    image_s *image;
} mtexinfo_t;

struct msurface_t
{
    int visframe; /* should be drawn when node is crossed */

    cplane_t *plane;
    int flags;

    int firstedge;          /* look up in model->surfedges[], negative numbers */
    int numedges;           /* are backwards edges */

    short texturemins[2];
    short extents[2];

    int light_s, light_t;           /* gl lightmap coordinates */
    int dlight_s, dlight_t;         /* gl lightmap coordinates for dynamic lightmaps */

    glpoly_t *polys;                /* multiple if warped */
    struct  msurface_t *texturechain;
    // struct  msurface_s *lightmapchain; not used/needed anymore

    mtexinfo_t *texinfo;

    /* lighting info */
    int dlightframe;
    int dlightbits;

    int lightmaptexturenum;
    byte styles[MAXLIGHTMAPS]; // MAXLIGHTMAPS = MAX_LIGHTMAPS_PER_SURFACE (defined in local.h)
    // I think cached_light is not used/needed anymore
    //float cached_light[MAXLIGHTMAPS];       /* values currently used in lightmap */
    byte *samples;                          /* [numstyles*surfsize] */
};

struct image_s {
    std::string path;
    int width, height;
    byte* data;
    msurface_t *texturechain;
    ~image_s();
};

typedef struct mnode_s
{
    /* common with leaf */
    int contents;               /* -1, to differentiate from leafs */
    int visframe;               /* node needs to be traversed if current */

    float minmaxs[6];           /* for bounding box culling */

    struct mnode_s *parent;

    /* node specific */
    cplane_t *plane;
    struct mnode_s *children[2];

    unsigned short firstsurface;
    unsigned short numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
    /* common with node */
    int contents;               /* wil be a negative contents number */
    int visframe;               /* node needs to be traversed if current */

    float minmaxs[6];           /* for bounding box culling */

    struct mnode_s *parent;

    /* leaf specific */
    int cluster;
    int area;

    msurface_t **firstmarksurface;
    int nummarksurfaces;
} mleaf_t;

/* Whole model */

// this, must be struct model_s, not gl3model_s,
// because struct model_s* is returned by re.RegisterModel()
struct mtl_model_t
{
    char name[MAX_QPATH];

    int registration_sequence;

    modtype_t type;
    int numframes;

    int flags;

    /* volume occupied by the model graphics */
    vec3_t mins, maxs;
    float radius;

    /* solid volume for clipping */
    bool clipbox;
    vec3_t clipmins, clipmaxs;

    /* brush model */
    int firstmodelsurface, nummodelsurfaces;
    int lightmap; /* only for submodels */

    int numsubmodels;
    mtl_model_t *submodels;

    int numplanes;
    cplane_t *planes;

    int numleafs; /* number of visible leafs, not counting 0 */
    mleaf_t *leafs;

    int numvertexes;
    mvertex_t *vertexes;

    int numedges;
    medge_t *edges;

    int numnodes;
    int firstnode;
    mnode_t *nodes;

    int numtexinfo;
    mtexinfo_t *texinfo;

    int numsurfaces;
    msurface_t *surfaces;

    int numsurfedges;
    int *surfedges;

    int nummarksurfaces;
    msurface_t **marksurfaces;

    dvis_t *vis;

    byte *lightdata;

    /* for alias models and skins */
    image_s *skins[MAX_MD2SKINS];

    int extradatasize;
    void *extradata;

    // submodules
    vec3_t        origin;    // for sounds or lights
};


#endif /* model_h */
