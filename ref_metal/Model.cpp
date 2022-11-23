//
//  Model.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 23.11.2022.
//

#include "Model.hpp"
#include "Image.hpp"

std::optional<std::shared_ptr<mtl_model_t>> Model::getModel(std::string name, std::optional<std::shared_ptr<mtl_model_t>> parent, bool crash) {
    if (name.at(0) == '*' && parent) {
        int i = (int) strtol(name.data() + 1, (char **) NULL, 10);
        
        if (i < 1 || i >= parent.value()->numsubmodels) {
            ri.Sys_Error(ERR_DROP, "%s: bad inline model number", __func__);
        }
        
        mtl_model_t* m = &parent.value()->submodels[i];
        return std::shared_ptr<mtl_model_t>(m);
    }
    
    if (auto it = models.find(name); it != models.end()) {
        return it->second;
    }
    
    unsigned* buf;
    int modfilelen = ri.FS_LoadFile(name.data(), (void **)&buf);
    if (!buf) {
        if (crash) {
            ri.Sys_Error(ERR_DROP, "%s: %s not found", __func__, name.data());
        }
        
        return std::nullopt;
    }
    
    /* call the apropriate loader */
    std::shared_ptr<mtl_model_t> result;
    
    switch (LittleLong(*(unsigned *)buf)) {
        case IDALIASHEADER:
            result = loadMD2(name, buf, modfilelen);
            break;

        case IDSPRITEHEADER:
            result = loadSP2(name, buf, modfilelen);
            break;

        case IDBSPHEADER:
//            Mod_LoadBrushModel(mod, buf, modfilelen);
            break;

        default:
            ri.Sys_Error(ERR_DROP, "%s: unknown fileid for %s", __func__, name.data());
            break;
    }
    result->extradatasize = Hunk_End();
    ri.FS_FreeFile(buf);
    
    return result;
}

std::shared_ptr<mtl_model_t> Model::loadMD2(std::string name, void *buffer, int modfilelen) {
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
        ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", name.data(), version, ALIAS_VERSION);
    }

    ofs_end = LittleLong(pinmodel->ofs_end);
    if (ofs_end < 0 || ofs_end > modfilelen) {
        ri.Sys_Error (ERR_DROP, "model %s file size(%d) too small, should be %d", name.data(), modfilelen, ofs_end);
    }
    
    auto mod = std::make_shared<mtl_model_t>();
    strcpy(mod->name, name.data());
    
    mod->extradata = Hunk_Begin(modfilelen);
    pheader = reinterpret_cast<dmdl_t*>(Hunk_Alloc(ofs_end));

    /* byte swap the header fields and sanity check */
    for (i = 0; i < sizeof(dmdl_t) / 4; i++)
    {
        ((int *)pheader)[i] = LittleLong(((int *)buffer)[i]);
    }

    if (pheader->skinheight > MAX_LBM_HEIGHT) {
        ri.Sys_Error(ERR_DROP, "model %s has a skin taller than %d", mod->name,
                MAX_LBM_HEIGHT);
    }

    if (pheader->num_xyz <= 0) {
        ri.Sys_Error(ERR_DROP, "model %s has no vertices", mod->name);
    }

    if (pheader->num_xyz > MAX_VERTS) {
        ri.Sys_Error(ERR_DROP, "model %s has too many vertices", mod->name);
    }

    if (pheader->num_st <= 0) {
        ri.Sys_Error(ERR_DROP, "model %s has no st vertices", mod->name);
    }

    if (pheader->num_tris <= 0) {
        ri.Sys_Error(ERR_DROP, "model %s has no triangles", mod->name);
    }

    if (pheader->num_frames <= 0) {
        ri.Sys_Error(ERR_DROP, "model %s has no frames", mod->name);
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
        mod->skins[i] = Img::FindImage((char *)pheader + pheader->ofs_skins + i * MAX_SKINNAME, it_skin);
    }

    mod->mins[0] = -32;
    mod->mins[1] = -32;
    mod->mins[2] = -32;
    mod->maxs[0] = 32;
    mod->maxs[1] = 32;
    mod->maxs[2] = 32;
    
    return mod;
}

std::shared_ptr<mtl_model_t> Model::loadSP2(std::string name, void *buffer, int modfilelen) {
    dsprite_t *sprin, *sprout;
    int i;

    sprin = (dsprite_t *)buffer;
    std::shared_ptr<mtl_model_t> mod = std::make_shared<mtl_model_t>();
    strcpy(mod->name, name.data());
    
    mod->extradata = Hunk_Begin(modfilelen);
    sprout = reinterpret_cast<dsprite_t*>(Hunk_Alloc(modfilelen));

    sprout->ident = LittleLong(sprin->ident);
    sprout->version = LittleLong(sprin->version);
    sprout->numframes = LittleLong(sprin->numframes);

    if (sprout->version != SPRITE_VERSION)
    {
        ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)",
                mod->name, sprout->version, SPRITE_VERSION);
    }

    if (sprout->numframes > MAX_MD2SKINS)
    {
        ri.Sys_Error(ERR_DROP, "%s has too many frames (%i > %i)",
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
        mod->skins[i] = Img::FindImage(sprout->frames[i].name, it_sprite);
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
        ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
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
        ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
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
        ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
                __func__, loadmodel->name);
    }

    count = l->filelen / sizeof(*in);

    if ((count < 1) || (count >= MAX_MAP_SURFEDGES))
    {
        ri.Sys_Error(ERR_DROP, "%s: bad surfedges count in %s: %i",
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
        ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
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

static void
Mod_LoadTexinfo(mtl_model_t *loadmodel, byte *mod_base, lump_t *l)
{
    texinfo_t *in;
    mtexinfo_t *out, *step;
    int i, j, count;
    char name[MAX_QPATH];
    int next;

    in = reinterpret_cast<texinfo_t*>((mod_base + l->fileofs));

    if (l->filelen % sizeof(*in))
    {
        ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
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

        out->image = Img::FindImage(name, it_wall);

        if (!out->image)
        {
            Com_sprintf(name, sizeof(name), "textures/%s.m8", in->texture);
            out->image = Img::FindImage(name, it_wall);
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
        ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
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
            ri.Sys_Error(ERR_DROP, "%s: bad texinfo number",
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

        if (r_fixsurfsky->value)
        {
            if (out->texinfo->flags & SURF_SKY)
            {
                out->flags |= SURF_DRAWSKY;
            }
        }

        /* create lightmaps and polygons */
        if (!(out->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
        {
            GL3_LM_CreateSurfaceLightmap(out);
        }

        if (!(out->texinfo->flags & SURF_WARP))
        {
            GL3_LM_BuildPolygonFromSurface(loadmodel, out);
        }
    }

    GL3_LM_EndBuildingLightmaps();
}

std::shared_ptr<mtl_model_t> loadBrushModel(std::string name, void *buffer, int modfilelen) {
    int i;
    dheader_t *header;
    byte *mod_base;

    header = (dheader_t *)buffer;

    i = LittleLong(header->version);

    if (i != BSPVERSION)
    {
        ri.Sys_Error(ERR_DROP, "%s: %s has wrong version number (%i should be %i)",
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

    auto mod = std::make_shared<mtl_model_t>();
    mod->extradata = Hunk_Begin(hunkSize);
    mod->type = mod_brush;
    strcpy(mod->name, name.data());

    /* load into heap */
    Mod_LoadVertexes(mod.get(), mod_base, &header->lumps[LUMP_VERTEXES]);
    Mod_LoadEdges(mod.get(), mod_base, &header->lumps[LUMP_EDGES]);
    Mod_LoadSurfedges(mod.get(), mod_base, &header->lumps[LUMP_SURFEDGES]);
    Mod_LoadLighting(mod.get(), mod_base, &header->lumps[LUMP_LIGHTING]);
    Mod_LoadPlanes(mod.get(), mod_base, &header->lumps[LUMP_PLANES]);
    Mod_LoadTexinfo(mod.get(), mod_base, &header->lumps[LUMP_TEXINFO]);
//    Mod_LoadFaces(mod, mod_base, &header->lumps[LUMP_FACES]);
//    Mod_LoadMarksurfaces(mod, mod_base, &header->lumps[LUMP_LEAFFACES]);
//    Mod_LoadVisibility(mod, mod_base, &header->lumps[LUMP_VISIBILITY]);
//    Mod_LoadLeafs(mod, mod_base, &header->lumps[LUMP_LEAFS]);
//    Mod_LoadNodes(mod, mod_base, &header->lumps[LUMP_NODES]);
//    Mod_LoadSubmodels (mod, mod_base, &header->lumps[LUMP_MODELS]);
    mod->numframes = 2; /* regular and alternate animation */
    
    return mod;
}
