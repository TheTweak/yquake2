/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __MODEL__
#define __MODEL__

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
typedef struct
{
	vec3_t		position;
} mvertex_t;


// FIXME: differentiate from texinfo SURF_ flags
#define	SURF_PLANEBACK		0x02
#define	SURF_DRAWSKY		0x04	// sky brush face
#define SURF_DRAWTURB		0x10
#define SURF_DRAWBACKGROUND	0x40
#define SURF_DRAWSKYBOX		0x80	// sky box

#define SURF_FLOW		0x100

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	float		vecs[2][4];
	float		mipadjust;
	image_t		*image;
	int		flags;
	int		numframes;
	struct mtexinfo_s	*next;		// animation chain
} mtexinfo_t;

typedef struct msurface_s
{
	int		visframe;		// should be drawn when node is crossed

	int		dlightframe;
	int		dlightbits;

	cplane_t	*plane;
	int		flags;

	int		firstedge;	// look up in model->surfedges[], negative numbers
	int		numedges;	// are backwards edges

	// surface generation data
	struct surfcache_s	*cachespots[MIPLEVELS];

	short		texturemins[2];
	short		extents[2];

	mtexinfo_t	*texinfo;

	// lighting info
	byte		styles[MAXLIGHTMAPS];
	byte		*samples;	// [numstyles*surfsize*3]

	struct msurface_s *nextalphasurface;
} msurface_t;


#define	CONTENTS_NODE	-1
typedef struct mnode_s
{
	// common with leaf
	int		contents;	// CONTENTS_NODE, to differentiate from leafs
	int		visframe;	// node needs to be traversed if current

	short		minmaxs[6];	// for bounding box culling

	struct mnode_s	*parent;

	// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];

	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
	// common with node
	int		contents;	// wil be something other than CONTENTS_NODE
	int		visframe;	// node needs to be traversed if current

	short		minmaxs[6];	// for bounding box culling

	struct mnode_s	*parent;

	// leaf specific
	int		cluster;
	int		area;

	msurface_t	**firstmarksurface;
	int		nummarksurfaces;
	int		key;		// BSP sequence number for leaf's contents
} mleaf_t;


//===================================================================

//
// Whole model
//

typedef struct model_s
{
	char		name[MAX_QPATH];

	int		registration_sequence;

	modtype_t	type;
	int		numframes;

	int		flags;

	//
	// volume occupied by the model graphics
	//
	vec3_t		mins, maxs;
	float		radius;

	//
	// solid volume for clipping (sent from server)
	//
	bool	clipbox;
	vec3_t		clipmins, clipmaxs;

	//
	// brush model
	//
	int		firstmodelsurface, nummodelsurfaces;

	int		numsubmodels;
	struct model_s	*submodels;

	int		numplanes;
	cplane_t	*planes;

	int		numleafs;	// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int		numvertexes;
	mvertex_t	*vertexes;

	int		numedges;
	medge_t		*edges;

	int		numnodes;
	int		firstnode;
	mnode_t		*nodes;

	int		numtexinfo;
	mtexinfo_t	*texinfo;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numsurfedges;
	int		*surfedges;

	int		nummarksurfaces;
	msurface_t	**marksurfaces;

	dvis_t		*vis;

	byte		*lightdata;

	// for alias models and sprites
	image_t		*skins[MAX_MD2SKINS];
	void		*extradata;
	int		extradatasize;

	// submodules
	vec3_t		origin;	// for sounds or lights
} model_t;

//============================================================================

void	Mod_Init(void);

const byte *Mod_ClusterPVS(int cluster, const model_t *model);

void Mod_Modellist_f(void);
void Mod_FreeAll(void);
void Mod_Free(model_t *mod);

extern	int	registration_sequence;

#endif	// __MODEL__
