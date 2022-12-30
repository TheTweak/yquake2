//
//  Model.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 23.11.2022.
//

#include <sstream>

#include "Model.hpp"
#include "../image/Image.hpp"
#include "../utils/Constants.h"
#include "../legacy/State.h"

Model::Model() {
    memset(mod_novis, 0xff, sizeof(mod_novis));
}

Model& Model::getInstance() {
    static Model instance;
    return instance;
}

const byte* Model::clusterPVS(int cluster, const mtl_model_t* model) {
    if (cluster == -1 || !model->vis) {
        return mod_novis;
    }
    
    return Mod_DecompressVis((byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS], (model->vis->numclusters + 7) >> 3);
}

model_s* Model::registerModel(char *name, mtl_model_t *worldModel) {
    auto modelOpt = getModel(name, worldModel, false);
    if (modelOpt) {
        if (modelOpt->type == mod_sprite) {
            dsprite_t *sprout = (dsprite_t *) modelOpt->extradata;
            
            for (int i = 0; i < sprout->numframes; i++) {
                modelOpt->skins[i] = Image::getInstance().FindImage(sprout->frames[i].name, it_sprite);
            }
        } else if (modelOpt->type == mod_alias) {
            dmdl_t *pheader = (dmdl_t *) modelOpt->extradata;
            
            for (int i = 0; i < pheader->num_skins; i++) {
                modelOpt->skins[i] = Image::getInstance().FindImage((char *) pheader + pheader->ofs_skins + i * MAX_SKINNAME, it_skin);
            }
            
            modelOpt->numframes = pheader->num_frames;
        } else if (modelOpt->type == mod_brush) {
            for (int i = 0; i < modelOpt->numtexinfo; i++) {
                //                not applicable to metal renderer
                //                model->texinfo[i].image->registration_sequence = registration_sequence;
            }
        }
        
        return modelOpt;
    }
    return NULL;
}

mtl_model_t* Model::getModel(std::string name, mtl_model_t *parent, bool crash) {
    if (name.at(0) == '*' && parent) {
        std::string parentName = parent->name;
        std::stringstream ss;
        ss << parentName << '_' << name;
        std::string fullName = ss.str();
        if (auto it = models.find(fullName); it != models.end()) {
            return it->second;
        }
        
        int i = (int) strtol(name.data() + 1, (char **) NULL, 10);
        
        if (i < 1 || i >= parent->numsubmodels) {
            GAME_API.Sys_Error(ERR_DROP, "%s: bad inline model number", __func__);
        }
        
        mtl_model_t* m = &parent->submodels[i];
        models[fullName] = m;
        return m;
    }
    
    if (auto it = models.find(name); it != models.end()) {
        return it->second;
    }
    
    unsigned* buf;
    int modfilelen = GAME_API.FS_LoadFile(name.data(), (void **)&buf);
    if (!buf) {
        if (crash) {
            GAME_API.Sys_Error(ERR_DROP, "%s: %s not found", __func__, name.data());
        }
        
        return NULL;
    }
    
    /* call the apropriate loader */
    mtl_model_t *result;
    
    switch (LittleLong(*(unsigned *)buf)) {
        case IDALIASHEADER:
            result = loadMD2(name, buf, modfilelen);
            break;

        case IDSPRITEHEADER:
            result = loadSP2(name, buf, modfilelen);
            break;

        case IDBSPHEADER:
            result = loadBrushModel(name, buf, modfilelen);
            break;

        default:
            GAME_API.Sys_Error(ERR_DROP, "%s: unknown fileid for %s", __func__, name.data());
            break;
    }
    result->extradatasize = Hunk_End();
    GAME_API.FS_FreeFile(buf);
    
    models[name] = result;
    
    return result;
}

mtl_model_t* Model::loadMD2(std::string name, void *buffer, int modfilelen) {
    int i, j;
    dmdl_t *pinmodel, *pheader;
    dstvert_t *pinst, *poutst;
    dtriangle_t *pintri, *pouttri;
    daliasframe_t *pinframe, *poutframe;
    int *pincmd, *poutcmd;
    int version;
    int ofs_end;

    pinmodel = (dmdl_t *)buffer;

    version = LittleLong(pinmodel->version);

    if (version != ALIAS_VERSION) {
        GAME_API.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", name.data(), version, ALIAS_VERSION);
    }

    ofs_end = LittleLong(pinmodel->ofs_end);
    if (ofs_end < 0 || ofs_end > modfilelen) {
        GAME_API.Sys_Error (ERR_DROP, "model %s file size(%d) too small, should be %d", name.data(), modfilelen, ofs_end);
    }
    
    mtl_model_t *mod = static_cast<mtl_model_t*>(std::malloc(sizeof(mtl_model_t)));
    strcpy(mod->name, name.data());
    
    mod->extradata = Hunk_Begin(modfilelen);
    pheader = reinterpret_cast<dmdl_t*>(Hunk_Alloc(ofs_end));

    /* byte swap the header fields and sanity check */
    for (i = 0; i < sizeof(dmdl_t) / 4; i++)
    {
        ((int *)pheader)[i] = LittleLong(((int *)buffer)[i]);
    }

    if (pheader->skinheight > MAX_LBM_HEIGHT) {
        GAME_API.Sys_Error(ERR_DROP, "model %s has a skin taller than %d", mod->name,
                MAX_LBM_HEIGHT);
    }

    if (pheader->num_xyz <= 0) {
        GAME_API.Sys_Error(ERR_DROP, "model %s has no vertices", mod->name);
    }

    if (pheader->num_xyz > MAX_VERTS) {
        GAME_API.Sys_Error(ERR_DROP, "model %s has too many vertices", mod->name);
    }

    if (pheader->num_st <= 0) {
        GAME_API.Sys_Error(ERR_DROP, "model %s has no st vertices", mod->name);
    }

    if (pheader->num_tris <= 0) {
        GAME_API.Sys_Error(ERR_DROP, "model %s has no triangles", mod->name);
    }

    if (pheader->num_frames <= 0) {
        GAME_API.Sys_Error(ERR_DROP, "model %s has no frames", mod->name);
    }

    /* load base s and t vertices (not used in gl version) */
    pinst = (dstvert_t *)((byte *)pinmodel + pheader->ofs_st);
    poutst = (dstvert_t *)((byte *)pheader + pheader->ofs_st);

    for (i = 0; i < pheader->num_st; i++)
    {
        poutst[i].s = LittleShort(pinst[i].s);
        poutst[i].t = LittleShort(pinst[i].t);
    }

    /* load triangle lists */
    pintri = (dtriangle_t *)((byte *)pinmodel + pheader->ofs_tris);
    pouttri = (dtriangle_t *)((byte *)pheader + pheader->ofs_tris);

    for (i = 0; i < pheader->num_tris; i++)
    {
        for (j = 0; j < 3; j++)
        {
            pouttri[i].index_xyz[j] = LittleShort(pintri[i].index_xyz[j]);
            pouttri[i].index_st[j] = LittleShort(pintri[i].index_st[j]);
        }
    }

    /* load the frames */
    for (i = 0; i < pheader->num_frames; i++)
    {
        pinframe = (daliasframe_t *)((byte *)pinmodel
                + pheader->ofs_frames + i * pheader->framesize);
        poutframe = (daliasframe_t *)((byte *)pheader
                + pheader->ofs_frames + i * pheader->framesize);

        memcpy(poutframe->name, pinframe->name, sizeof(poutframe->name));

        for (j = 0; j < 3; j++)
        {
            poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
            poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
        }

        /* verts are all 8 bit, so no swapping needed */
        memcpy(poutframe->verts, pinframe->verts,
                pheader->num_xyz * sizeof(dtrivertx_t));
    }

    mod->type = mod_alias;

    /* load the glcmds */
    pincmd = (int *)((byte *)pinmodel + pheader->ofs_glcmds);
    poutcmd = (int *)((byte *)pheader + pheader->ofs_glcmds);

    for (i = 0; i < pheader->num_glcmds; i++)
    {
        poutcmd[i] = LittleLong(pincmd[i]);
    }

    if (poutcmd[pheader->num_glcmds-1] != 0)
    {
        R_Printf(PRINT_ALL, "%s: Entity %s has possible last element issues with %d verts.\n",
            __func__,
            mod->name,
            poutcmd[pheader->num_glcmds-1]);
    }

    /* register all skins */
    memcpy((char *)pheader + pheader->ofs_skins,
            (char *)pinmodel + pheader->ofs_skins,
            pheader->num_skins * MAX_SKINNAME);

    for (i = 0; i < pheader->num_skins; i++)
    {
        mod->skins[i] = Image::getInstance().FindImage((char *)pheader + pheader->ofs_skins + i * MAX_SKINNAME, it_skin);
    }

    mod->mins[0] = -32;
    mod->mins[1] = -32;
    mod->mins[2] = -32;
    mod->maxs[0] = 32;
    mod->maxs[1] = 32;
    mod->maxs[2] = 32;
    
    return mod;
}

mtl_model_t* Model::loadSP2(std::string name, void *buffer, int modfilelen) {
    dsprite_t *sprin, *sprout;
    int i;

    sprin = (dsprite_t *)buffer;
    mtl_model_t *mod = static_cast<mtl_model_t*>(std::malloc(sizeof(mtl_model_t)));
    strcpy(mod->name, name.data());
    
    mod->extradata = Hunk_Begin(modfilelen);
    sprout = reinterpret_cast<dsprite_t*>(Hunk_Alloc(modfilelen));

    sprout->ident = LittleLong(sprin->ident);
    sprout->version = LittleLong(sprin->version);
    sprout->numframes = LittleLong(sprin->numframes);

    if (sprout->version != SPRITE_VERSION)
    {
        GAME_API.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)",
                mod->name, sprout->version, SPRITE_VERSION);
    }

    if (sprout->numframes > MAX_MD2SKINS)
    {
        GAME_API.Sys_Error(ERR_DROP, "%s has too many frames (%i > %i)",
                mod->name, sprout->numframes, MAX_MD2SKINS);
    }

    /* byte swap everything */
    for (i = 0; i < sprout->numframes; i++)
    {
        sprout->frames[i].width = LittleLong(sprin->frames[i].width);
        sprout->frames[i].height = LittleLong(sprin->frames[i].height);
        sprout->frames[i].origin_x = LittleLong(sprin->frames[i].origin_x);
        sprout->frames[i].origin_y = LittleLong(sprin->frames[i].origin_y);
        memcpy(sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
        mod->skins[i] = Image::getInstance().FindImage(sprout->frames[i].name, it_sprite);
    }

    mod->type = mod_sprite;
    
    return mod;
}

// calculate the size that Hunk_Alloc(), called by Mod_Load*() from Mod_LoadBrushModel(),
// will use (=> includes its padding), so we'll know how big the hunk needs to be
static int calcLumpHunkSize(const lump_t *l, int inSize, int outSize)
{
    if (l->filelen % inSize)
    {
        // Mod_Load*() will error out on this because of "funny size"
        // don't error out here because in Mod_Load*() it can print the functionname
        // (=> tells us what kind of lump) before shutting down the game
        return 0;
    }

    int count = l->filelen / inSize;
    int size = count * outSize;

    // round to cacheline, like Hunk_Alloc() does
    size = (size + 31) & ~31;
    return size;
}

static int calcTexinfoAndFacesSize(byte *mod_base, const lump_t *fl, const lump_t *tl)
{
    dface_t* face_in = reinterpret_cast<dface_t*>(mod_base + fl->fileofs);
    texinfo_t* texinfo_in = reinterpret_cast<texinfo_t*>(mod_base + tl->fileofs);

    if (fl->filelen % sizeof(*face_in) || tl->filelen % sizeof(*texinfo_in))
    {
        // will error out when actually loading it
        return 0;
    }

    int ret = 0;

    int face_count = fl->filelen / sizeof(*face_in);
    int texinfo_count = tl->filelen / sizeof(*texinfo_in);

    {
        // out = Hunk_Alloc(count * sizeof(*out));
        int baseSize = face_count * sizeof(msurface_t);
        baseSize = (baseSize + 31) & ~31;
        ret += baseSize;

        int ti_size = texinfo_count * sizeof(mtexinfo_t);
        ti_size = (ti_size + 31) & ~31;
        ret += ti_size;
    }

    int numWarpFaces = 0;

    for (int surfnum = 0; surfnum < face_count; surfnum++, face_in++)
    {
        int numverts = LittleShort(face_in->numedges);
        int ti = LittleShort(face_in->texinfo);
        if ((ti < 0) || (ti >= texinfo_count))
        {
            return 0; // will error out
        }
        int texFlags = LittleLong(texinfo_in[ti].flags);

        /* set the drawing flags */
        if (texFlags & SURF_WARP)
        {
            if (numverts > 60)
                return 0; // will error out in R_SubdividePolygon()

            // GL3_SubdivideSurface(out, loadmodel); /* cut up polygon for warps */
            // for each (pot. recursive) call to R_SubdividePolygon():
            //   sizeof(glpoly_t) + ((numverts - 4) + 2) * sizeof(gl3_3D_vtx_t)

            // this is tricky, how much is allocated depends on the size of the surface
            // which we don't know (we'd need the vertices etc to know, but we can't load
            // those without allocating...)
            // so we just count warped faces and use a generous estimate below

            ++numWarpFaces;
        }
        else
        {
            // GL3_LM_BuildPolygonFromSurface(out);
            // => poly = Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * sizeof(gl3_3D_vtx_t));
            int polySize = sizeof(glpoly_t) + (numverts - 4) * sizeof(mtl_3D_vtx_t);
            polySize = (polySize + 31) & ~31;
            ret += polySize;
        }
    }

    // yeah, this is a bit hacky, but it looks like for each warped face
    // 256-55000 bytes are allocated (usually on the lower end),
    // so just assume 48k per face to be safe
    ret += numWarpFaces * 49152;
    ret += 1000000; // and 1MB extra just in case

    return ret;
}

static void
Mod_LoadVertexes(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    dvertex_t *in;
    mvertex_t *out;
    int i, count;

    in = reinterpret_cast<dvertex_t*>((mod_base + l->fileofs));

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<mvertex_t*>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->vertexes = out;
    loadmodel->numvertexes = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        out->position[0] = LittleFloat(in->point[0]);
        out->position[1] = LittleFloat(in->point[1]);
        out->position[2] = LittleFloat(in->point[2]);
    }
}

static void
Mod_LoadEdges(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    dedge_t *in;
    medge_t *out;
    int i, count;

    in = reinterpret_cast<dedge_t*>((mod_base + l->fileofs));

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<medge_t*>(Hunk_Alloc((count + 1) * sizeof(*out)));

    loadmodel->edges = out;
    loadmodel->numedges = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        out->v[0] = (unsigned short)LittleShort(in->v[0]);
        out->v[1] = (unsigned short)LittleShort(in->v[1]);
    }
}

static void
Mod_LoadSurfedges(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    int i, count;
    int *in, *out;

    in = reinterpret_cast<int*>(mod_base + l->fileofs);

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);

    if ((count < 1) || (count >= MAX_MAP_SURFEDGES))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: bad surfedges count in %s: %i",
                __func__, loadmodel->name, count);
    }

    out = reinterpret_cast<int*>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->surfedges = out;
    loadmodel->numsurfedges = count;

    for (i = 0; i < count; i++)
    {
        out[i] = LittleLong(in[i]);
    }
}

static void
Mod_LoadLighting(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    if (!l->filelen)
    {
        loadmodel->lightdata = NULL;
        return;
    }

    loadmodel->lightdata = static_cast<byte*>(Hunk_Alloc(l->filelen));
    memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

static void
Mod_LoadPlanes(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    int i, j;
    cplane_t *out;
    dplane_t *in;
    int count;
    int bits;

    in = reinterpret_cast<dplane_t*>(mod_base + l->fileofs);

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<cplane_t*>(Hunk_Alloc(count * 2 * sizeof(*out)));

    loadmodel->planes = out;
    loadmodel->numplanes = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        bits = 0;

        for (j = 0; j < 3; j++)
        {
            out->normal[j] = LittleFloat(in->normal[j]);

            if (out->normal[j] < 0)
            {
                bits |= 1 << j;
            }
        }

        out->dist = LittleFloat(in->dist);
        out->type = LittleLong(in->type);
        out->signbits = bits;
    }
}

void Model::Mod_LoadTexinfo(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    texinfo_t *in;
    mtexinfo_t *out, *step;
    int i, j, count;
    char name[MAX_QPATH];
    int next;

    in = reinterpret_cast<texinfo_t*>((mod_base + l->fileofs));

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<mtexinfo_t*>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->texinfo = out;
    loadmodel->numtexinfo = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        for (j = 0; j < 4; j++)
        {
            out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
            out->vecs[1][j] = LittleFloat(in->vecs[1][j]);
        }

        out->flags = LittleLong(in->flags);
        next = LittleLong(in->nexttexinfo);

        if (next > 0)
        {
            out->next = loadmodel->texinfo + next;
        }
        else
        {
            out->next = NULL;
        }

        Com_sprintf(name, sizeof(name), "textures/%s.wal", in->texture);

        out->image = Image::getInstance().FindImage(name, it_wall);

        if (!out->image)
        {
            Com_sprintf(name, sizeof(name), "textures/%s.m8", in->texture);
            out->image = Image::getInstance().FindImage(name, it_wall);
        }

        if (!out->image)
        {
            R_Printf(PRINT_ALL, "Couldn't load %s\n", name);
        }
    }

    /* count animation frames */
    for (i = 0; i < count; i++)
    {
        out = &loadmodel->texinfo[i];
        out->numframes = 1;

        for (step = out->next; step && step != out; step = step->next)
        {
            out->numframes++;
        }
    }
}

static void
Mod_CalcSurfaceExtents(mtl_model_t *loadmodel, msurface_t *s)
{
    float mins[2], maxs[2], val;
    int i, j, e;
    mvertex_t *v;
    mtexinfo_t *tex;
    int bmins[2], bmaxs[2];

    mins[0] = mins[1] = 999999;
    maxs[0] = maxs[1] = -99999;

    tex = s->texinfo;

    for (i = 0; i < s->numedges; i++)
    {
        e = loadmodel->surfedges[s->firstedge + i];

        if (e >= 0)
        {
            v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
        }
        else
        {
            v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
        }

        for (j = 0; j < 2; j++)
        {
            val = v->position[0] * tex->vecs[j][0] +
                  v->position[1] * tex->vecs[j][1] +
                  v->position[2] * tex->vecs[j][2] +
                  tex->vecs[j][3];

            if (val < mins[j])
            {
                mins[j] = val;
            }

            if (val > maxs[j])
            {
                maxs[j] = val;
            }
        }
    }

    for (i = 0; i < 2; i++)
    {
        bmins[i] = floor(mins[i] / 16);
        bmaxs[i] = ceil(maxs[i] / 16);

        s->texturemins[i] = bmins[i] * 16;
        s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
    }
}

static void
R_BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
    int i, j;
    float *v;

    mins[0] = mins[1] = mins[2] = 9999;
    maxs[0] = maxs[1] = maxs[2] = -9999;
    v = verts;

    for (i = 0; i < numverts; i++)
    {
        for (j = 0; j < 3; j++, v++)
        {
            if (*v < mins[j])
            {
                mins[j] = *v;
            }

            if (*v > maxs[j])
            {
                maxs[j] = *v;
            }
        }
    }
}

static const float SUBDIVIDE_SIZE = 64.0f;

static void
R_SubdividePolygon(int numverts, float *verts, msurface_t *warpface)
{
    int i, j, k;
    vec3_t mins, maxs;
    float m;
    float *v;
    vec3_t front[64], back[64];
    int f, b;
    float dist[64];
    float frac;
    glpoly_t *poly;
    float s, t;
    vec3_t total;
    float total_s, total_t;
    vec3_t normal;

    VectorCopy(warpface->plane->normal, normal);

    if (numverts > 60)
    {
        GAME_API.Sys_Error(ERR_DROP, "numverts = %i", numverts);
    }

    R_BoundPoly(numverts, verts, mins, maxs);

    for (i = 0; i < 3; i++)
    {
        m = (mins[i] + maxs[i]) * 0.5;
        m = SUBDIVIDE_SIZE * floor(m / SUBDIVIDE_SIZE + 0.5);

        if (maxs[i] - m < 8)
        {
            continue;
        }

        if (m - mins[i] < 8)
        {
            continue;
        }

        /* cut it */
        v = verts + i;

        for (j = 0; j < numverts; j++, v += 3)
        {
            dist[j] = *v - m;
        }

        /* wrap cases */
        dist[j] = dist[0];
        v -= i;
        VectorCopy(verts, v);

        f = b = 0;
        v = verts;

        for (j = 0; j < numverts; j++, v += 3)
        {
            if (dist[j] >= 0)
            {
                VectorCopy(v, front[f]);
                f++;
            }

            if (dist[j] <= 0)
            {
                VectorCopy(v, back[b]);
                b++;
            }

            if ((dist[j] == 0) || (dist[j + 1] == 0))
            {
                continue;
            }

            if ((dist[j] > 0) != (dist[j + 1] > 0))
            {
                /* clip point */
                frac = dist[j] / (dist[j] - dist[j + 1]);

                for (k = 0; k < 3; k++)
                {
                    front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
                }

                f++;
                b++;
            }
        }

        R_SubdividePolygon(f, front[0], warpface);
        R_SubdividePolygon(b, back[0], warpface);
        return;
    }

    /* add a point in the center to help keep warp valid */
    poly = reinterpret_cast<glpoly_t*>(Hunk_Alloc(sizeof(glpoly_t) + ((numverts - 4) + 2) * sizeof(mtl_3D_vtx_t)));
    poly->next = warpface->polys;
    warpface->polys = poly;
    poly->numverts = numverts + 2;
    VectorClear(total);
    total_s = 0;
    total_t = 0;

    for (i = 0; i < numverts; i++, verts += 3)
    {
        VectorCopy(verts, poly->vertices[i + 1].pos);
        s = DotProduct(verts, warpface->texinfo->vecs[0]);
        t = DotProduct(verts, warpface->texinfo->vecs[1]);

        total_s += s;
        total_t += t;
        VectorAdd(total, verts, total);

        poly->vertices[i + 1].texCoord[0] = s;
        poly->vertices[i + 1].texCoord[1] = t;
        VectorCopy(normal, poly->vertices[i + 1].normal);
        poly->vertices[i + 1].lightFlags = 0;
    }

    VectorScale(total, (1.0 / numverts), poly->vertices[0].pos);
    poly->vertices[0].texCoord[0] = total_s / numverts;
    poly->vertices[0].texCoord[1] = total_t / numverts;
    VectorCopy(normal, poly->vertices[0].normal);

    /* copy first vertex to last */
    //memcpy(poly->vertices[i + 1], poly->vertices[1], sizeof(poly->vertices[0]));
    poly->vertices[i + 1] = poly->vertices[1];
}

void
GL3_SubdivideSurface(msurface_t *fa, mtl_model_t* loadmodel)
{
    vec3_t verts[64];
    int numverts;
    int i;
    int lindex;
    float *vec;

    /* convert edges back to a normal polygon */
    numverts = 0;

    for (i = 0; i < fa->numedges; i++)
    {
        lindex = loadmodel->surfedges[fa->firstedge + i];

        if (lindex > 0)
        {
            vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
        }
        else
        {
            vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
        }

        VectorCopy(vec, verts[numverts]);
        numverts++;
    }

    R_SubdividePolygon(numverts, verts[0], fa);
}

void GL3_LM_BuildPolygonFromSurface(mtl_model_t *currentmodel, msurface_t *fa) {
    int i, lindex, lnumverts;
    medge_t *pedges, *r_pedge;
    float *vec;
    float s, t;
    glpoly_t *poly;
    vec3_t total;
    vec3_t normal;

    /* reconstruct the polygon */
    pedges = currentmodel->edges;
    lnumverts = fa->numedges;

    VectorClear(total);

    /* draw texture */
    poly = reinterpret_cast<glpoly_t*>(Hunk_Alloc(sizeof(glpoly_t) +
                                                  (lnumverts - 4) * sizeof(mtl_3D_vtx_t)));
    poly->next = fa->polys;
    poly->flags = fa->flags;
    fa->polys = poly;
    poly->numverts = lnumverts;

    VectorCopy(fa->plane->normal, normal);

    if(fa->flags & SURF_PLANEBACK)
    {
        // if for some reason the normal sticks to the back of the plane, invert it
        // so it's usable for the shader
        for (i=0; i<3; ++i)  normal[i] = -normal[i];
    }

    for (i = 0; i < lnumverts; i++)
    {
        mtl_3D_vtx_t* vert = &poly->vertices[i];

        lindex = currentmodel->surfedges[fa->firstedge + i];

        if (lindex > 0)
        {
            r_pedge = &pedges[lindex];
            vec = currentmodel->vertexes[r_pedge->v[0]].position;
        }
        else
        {
            r_pedge = &pedges[-lindex];
            vec = currentmodel->vertexes[r_pedge->v[1]].position;
        }

        s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
        s /= fa->texinfo->image->width;

        t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
        t /= fa->texinfo->image->height;

        VectorAdd(total, vec, total);
        VectorCopy(vec, vert->pos);
        vert->texCoord[0] = s;
        vert->texCoord[1] = t;

        /* lightmap texture coordinates */
        s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
        s -= fa->texturemins[0];
        s += fa->light_s * 16;
        s += 8;
        s /= BLOCK_WIDTH * 16; /* fa->texinfo->texture->width; */

        t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
        t -= fa->texturemins[1];
        t += fa->light_t * 16;
        t += 8;
        t /= BLOCK_HEIGHT * 16; /* fa->texinfo->texture->height; */

        vert->lmTexCoord[0] = s;
        vert->lmTexCoord[1] = t;

        VectorCopy(normal, vert->normal);
        vert->lightFlags = 0;
    }
}

static void
Mod_LoadFaces(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    dface_t *in;
    msurface_t *out;
    int i, count, surfnum;
    int planenum, side;
    int ti;

    in = reinterpret_cast<dface_t*>(mod_base + l->fileofs);

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<msurface_t*>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->surfaces = out;
    loadmodel->numsurfaces = count;

//    GL3_LM_BeginBuildingLightmaps(loadmodel);

    for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
    {
        out->firstedge = LittleLong(in->firstedge);
        out->numedges = LittleShort(in->numedges);
        out->flags = 0;
        out->polys = NULL;

        planenum = LittleShort(in->planenum);
        side = LittleShort(in->side);

        if (side)
        {
            out->flags |= SURF_PLANEBACK;
        }

        out->plane = loadmodel->planes + planenum;

        ti = LittleShort(in->texinfo);

        if ((ti < 0) || (ti >= loadmodel->numtexinfo))
        {
            GAME_API.Sys_Error(ERR_DROP, "%s: bad texinfo number",
                    __func__);
        }

        out->texinfo = loadmodel->texinfo + ti;

        Mod_CalcSurfaceExtents(loadmodel, out);

        /* lighting info */
        for (i = 0; i < MAX_LIGHTMAPS_PER_SURFACE; i++)
        {
            out->styles[i] = in->styles[i];
        }

        i = LittleLong(in->lightofs);

        if (i == -1)
        {
            out->samples = NULL;
        }
        else
        {
            out->samples = loadmodel->lightdata + i;
        }

        /* set the drawing flags */
        if (out->texinfo->flags & SURF_WARP)
        {
            out->flags |= SURF_DRAWTURB;

            for (i = 0; i < 2; i++)
            {
                out->extents[i] = 16384;
                out->texturemins[i] = -8192;
            }

            GL3_SubdivideSurface(out, loadmodel); /* cut up polygon for warps */
        }
        
        /*
        if (r_fixsurfsky->value)
        {
            if (out->texinfo->flags & SURF_SKY)
            {
                out->flags |= SURF_DRAWSKY;
            }
        }
        */

        /* create lightmaps and polygons */
        /*
        if (!(out->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
        {
            GL3_LM_CreateSurfaceLightmap(out);
        }
        */
        if (!(out->texinfo->flags & SURF_WARP))
        {
            GL3_LM_BuildPolygonFromSurface(loadmodel, out);
        }
    }

//    GL3_LM_EndBuildingLightmaps();
}

static void
Mod_LoadMarksurfaces(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    int i, j, count;
    short *in;
    msurface_t **out;

    in = reinterpret_cast<short*>(mod_base + l->fileofs);

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<msurface_t**>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->marksurfaces = out;
    loadmodel->nummarksurfaces = count;

    for (i = 0; i < count; i++)
    {
        j = LittleShort(in[i]);

        if ((j < 0) || (j >= loadmodel->numsurfaces))
        {
            GAME_API.Sys_Error(ERR_DROP, "%s: bad surface number", __func__);
        }

        out[i] = loadmodel->surfaces + j;
    }
}

static void
Mod_LoadVisibility(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    int i;

    if (!l->filelen)
    {
        loadmodel->vis = NULL;
        return;
    }

    loadmodel->vis = reinterpret_cast<dvis_t*>(Hunk_Alloc(l->filelen));
    memcpy(loadmodel->vis, mod_base + l->fileofs, l->filelen);

    loadmodel->vis->numclusters = LittleLong(loadmodel->vis->numclusters);

    for (i = 0; i < loadmodel->vis->numclusters; i++)
    {
        loadmodel->vis->bitofs[i][0] = LittleLong(loadmodel->vis->bitofs[i][0]);
        loadmodel->vis->bitofs[i][1] = LittleLong(loadmodel->vis->bitofs[i][1]);
    }
}

static void
Mod_LoadLeafs(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    dleaf_t *in;
    mleaf_t *out;
    int i, j, count, p;

    in = reinterpret_cast<dleaf_t*>(mod_base + l->fileofs);

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<mleaf_t*>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->leafs = out;
    loadmodel->numleafs = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        unsigned firstleafface;

        for (j = 0; j < 3; j++)
        {
            out->minmaxs[j] = LittleShort(in->mins[j]);
            out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
        }

        p = LittleLong(in->contents);
        out->contents = p;

        out->cluster = LittleShort(in->cluster);
        out->area = LittleShort(in->area);

        // make unsigned long from signed short
        firstleafface = LittleShort(in->firstleafface) & 0xFFFF;
        out->nummarksurfaces = LittleShort(in->numleaffaces) & 0xFFFF;

        out->firstmarksurface = loadmodel->marksurfaces + firstleafface;
        if ((firstleafface + out->nummarksurfaces) > loadmodel->nummarksurfaces)
        {
            GAME_API.Sys_Error(ERR_DROP, "%s: wrong marksurfaces position in %s",
                __func__, loadmodel->name);
        }
    }
}

static void
Mod_SetParent(mnode_t *node, mnode_t *parent)
{
    node->parent = parent;

    if (node->contents != -1)
    {
        return;
    }

    Mod_SetParent(node->children[0], node);
    Mod_SetParent(node->children[1], node);
}

static void
Mod_LoadNodes(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    int i, j, count, p;
    dnode_t *in;
    mnode_t *out;

    in = reinterpret_cast<dnode_t*>(mod_base + l->fileofs);

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<mnode_t*>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->nodes = out;
    loadmodel->numnodes = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        for (j = 0; j < 3; j++)
        {
            out->minmaxs[j] = LittleShort(in->mins[j]);
            out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
        }

        p = LittleLong(in->planenum);
        out->plane = loadmodel->planes + p;

        out->firstsurface = LittleShort(in->firstface);
        out->numsurfaces = LittleShort(in->numfaces);
        out->contents = -1; /* differentiate from leafs */

        for (j = 0; j < 2; j++)
        {
            p = LittleLong(in->children[j]);

            if (p >= 0)
            {
                out->children[j] = loadmodel->nodes + p;
            }
            else
            {
                out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
            }
        }
    }

    Mod_SetParent(loadmodel->nodes, NULL); /* sets nodes and leafs */
}

static void
Mod_LoadSubmodels(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    dmodel_t *in;
    mtl_model_t *out;
    int i, j, count;

    in = reinterpret_cast<dmodel_t*>((mod_base + l->fileofs));

    if (l->filelen % sizeof(*in))
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);
    out = reinterpret_cast<mtl_model_t*>(Hunk_Alloc(count * sizeof(*out)));

    loadmodel->submodels = out;
    loadmodel->numsubmodels = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        if (i == 0)
        {
            // copy parent as template for first model
            memcpy(out, loadmodel, sizeof(*out));
        }
        else
        {
            // copy first as template for model
            memcpy(out, loadmodel->submodels, sizeof(*out));
        }

        Com_sprintf (out->name, sizeof(out->name), "*%d", i);

        for (j = 0; j < 3; j++)
        {
            /* spread the mins / maxs by a pixel */
            out->mins[j] = LittleFloat(in->mins[j]) - 1;
            out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
            out->origin[j] = LittleFloat(in->origin[j]);
        }

        out->radius = Mod_RadiusFromBounds(out->mins, out->maxs);
        out->firstnode = LittleLong(in->headnode);
        out->firstmodelsurface = LittleLong(in->firstface);
        out->nummodelsurfaces = LittleLong(in->numfaces);
        // visleafs
        out->numleafs = 0;
        //  check limits
        if (out->firstnode >= loadmodel->numnodes)
        {
            GAME_API.Sys_Error(ERR_DROP, "%s: Inline model %i has bad firstnode",
                    __func__, i);
        }
    }
}

mtl_model_t* Model::loadBrushModel(std::string name, void *buffer, int modfilelen) {
    int i;
    dheader_t *header;
    byte *mod_base;

    header = (dheader_t *)buffer;

    i = LittleLong(header->version);

    if (i != BSPVERSION)
    {
        GAME_API.Sys_Error(ERR_DROP, "%s: %s has wrong version number (%i should be %i)",
                __func__, name.data(), i, BSPVERSION);
    }

    /* swap all the lumps */
    mod_base = (byte *)header;

    for (i = 0; i < sizeof(dheader_t) / 4; i++)
    {
        ((int *)header)[i] = LittleLong(((int *)header)[i]);
    }

    // calculate the needed hunksize from the lumps
    int hunkSize = 0;
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_VERTEXES], sizeof(dvertex_t), sizeof(mvertex_t));
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_EDGES], sizeof(dedge_t), sizeof(medge_t));
    hunkSize += sizeof(medge_t) + 31; // for count+1 in Mod_LoadEdges()
    int surfEdgeCount = (header->lumps[LUMP_SURFEDGES].filelen+sizeof(int)-1)/sizeof(int);
    if(surfEdgeCount < MAX_MAP_SURFEDGES) // else it errors out later anyway
        hunkSize += calcLumpHunkSize(&header->lumps[LUMP_SURFEDGES], sizeof(int), sizeof(int));
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_LIGHTING], 1, 1);
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_PLANES], sizeof(dplane_t), sizeof(cplane_t)*2);
    hunkSize += calcTexinfoAndFacesSize(mod_base, &header->lumps[LUMP_FACES], &header->lumps[LUMP_TEXINFO]);
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_LEAFFACES], sizeof(short), sizeof(msurface_t *)); // yes, out is indeeed a pointer!
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_VISIBILITY], 1, 1);
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_LEAFS], sizeof(dleaf_t), sizeof(mleaf_t));
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_NODES], sizeof(dnode_t), sizeof(mnode_t));
    hunkSize += calcLumpHunkSize(&header->lumps[LUMP_MODELS], sizeof(dmodel_t), sizeof(mmodel_t));

    mtl_model_t *mod = static_cast<mtl_model_t*>(std::malloc(sizeof(mtl_model_t)));
    mod->extradata = Hunk_Begin(hunkSize);
    mod->type = mod_brush;
    strcpy(mod->name, name.data());

    /* load into heap */
    Mod_LoadVertexes(mod, mod_base, &header->lumps[LUMP_VERTEXES]);
    Mod_LoadEdges(mod, mod_base, &header->lumps[LUMP_EDGES]);
    Mod_LoadSurfedges(mod, mod_base, &header->lumps[LUMP_SURFEDGES]);
    Mod_LoadLighting(mod, mod_base, &header->lumps[LUMP_LIGHTING]);
    Mod_LoadPlanes(mod, mod_base, &header->lumps[LUMP_PLANES]);
    Mod_LoadTexinfo(mod, mod_base, &header->lumps[LUMP_TEXINFO]);
    Mod_LoadFaces(mod, mod_base, &header->lumps[LUMP_FACES]);
    Mod_LoadMarksurfaces(mod, mod_base, &header->lumps[LUMP_LEAFFACES]);
    Mod_LoadVisibility(mod, mod_base, &header->lumps[LUMP_VISIBILITY]);
    Mod_LoadLeafs(mod, mod_base, &header->lumps[LUMP_LEAFS]);
    Mod_LoadNodes(mod, mod_base, &header->lumps[LUMP_NODES]);
    Mod_LoadSubmodels (mod, mod_base, &header->lumps[LUMP_MODELS]);
    mod->numframes = 2; /* regular and alternate animation */
    
    return mod;
}

Model::~Model() {
    for (auto it = models.begin(); it != models.end(); it++) {
        if (it->second && it->first.find("*") == std::string::npos) {
            std::free(it->second);
        }
    }
}
