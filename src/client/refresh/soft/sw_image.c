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

#include "header/local.h"

#define	MAX_RIMAGES	1024
static image_t          *r_whitetexture_mip = NULL;
static image_t		r_images[MAX_RIMAGES];
static int		numr_images;
static int		image_max = 0;


/*
===============
R_ImageList_f
===============
*/
void
R_ImageList_f (void)
{
	int		i, used, texels;
	image_t	*image;
	bool	freeup;

	R_Printf(PRINT_ALL, "------------------\n");
	texels = 0;
	used = 0;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		char *in_use = "";

		if (image->registration_sequence == registration_sequence)
		{
			in_use = "*";
			used++;
		}

		if (image->registration_sequence <= 0)
			continue;
		texels += image->width*image->height;
		switch (image->type)
		{
		case it_skin:
			R_Printf(PRINT_ALL, "M");
			break;
		case it_sprite:
			R_Printf(PRINT_ALL, "S");
			break;
		case it_wall:
			R_Printf(PRINT_ALL, "W");
			break;
		case it_pic:
			R_Printf(PRINT_ALL, "P");
			break;
		default:
			R_Printf(PRINT_ALL, " ");
			break;
		}

		R_Printf(PRINT_ALL,  " %3i %3i : %s (%dx%d) %s\n",
			image->asset_width, image->asset_height, image->name,
			image->width, image->height, in_use);
	}
	R_Printf(PRINT_ALL, "Total texel count: %i\n", texels);
	freeup = R_ImageHasFreeSpace();
	R_Printf(PRINT_ALL, "Used %d of %d images%s.\n", used, image_max, freeup ? ", has free space" : "");
}

//=======================================================

static image_t *
R_FindFreeImage (void)
{
	image_t		*image;
	int			i;

	// find a free image_t
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
	{
		if (!image->registration_sequence)
			break;
	}
	if (i == numr_images)
	{
		if (numr_images == MAX_RIMAGES)
			ri.Sys_Error(ERR_DROP, "%s: Max images", __func__);
		numr_images++;
	}
	image = &r_images[i];

	return image;
}

static void
R_ImageShrink(const unsigned char* src, unsigned char *dst, int width, int realwidth, int height, int realheight)
{
	int x, y;
	float xstep, ystep;

	xstep = (float)width / realwidth;
	ystep = (float)height / realheight;

	for (y=0; y<realheight; y++)
	{
		for (x=0; x<realwidth; x++)
		{
			// inplace update
			dst[x + y * realwidth] = src[(int)(x * xstep)  + (int)(y * ystep) * width];
		}
	}
}

static void
R_RestoreMips(image_t *image, int min_mips)
{
	int i;

	for (i=min_mips; i<(NUM_MIPS-1); i++)
	{
		R_ImageShrink(image->pixels[i], image->pixels[i + 1],
			      image->width >> i, image->width >> (i + 1),
			      image->height >> i, image->height >> (i + 1));
	}
}

static size_t
R_GetImageMipsSize(size_t mip1_size)
{
	size_t full_size = 0;
	int i;

	for (i=0; i<NUM_MIPS; i++)
	{
		full_size += mip1_size >> (i * 2);
	}
	return full_size;
}

static void
R_RestoreImagePointers(image_t *image, int min_mips)
{
	int i;

	for (i=min_mips; i<NUM_MIPS-1; i++)
	{
		image->pixels[i + 1] = image->pixels[i] + (
			image->width * image->height / (1 << (i * 2)));
	}
	image->mip_levels = NUM_MIPS;
}

byte *
Get_BestImageSize(const image_t *image, int *req_width, int *req_height)
{
	int width, height, i;
	width = image->width;
	height = image->height;

	// return last mip before smaller image size
	for (i = 0; i < (image->mip_levels - 1); i++)
	{
		if (image->pixels[i] &&
		    ((width / 2) < *req_width ||
		    (height / 2) < *req_height))
		{
			*req_width = width;
			*req_height = height;
			return image->pixels[i];
		}

		width = width / 2;
		height = height / 2;
	}

	if (image->pixels[image->mip_levels - 1])
	{
		*req_width = image->width >> (image->mip_levels - 1);
		*req_height = image->height >> (image->mip_levels - 1);
		return image->pixels[image->mip_levels - 1];
	}
	else
	{
		*req_width = image->width;
		*req_height = image->height;
		return image->pixels[0];
	}
}

/*
================
R_LoadPic

================
*/
static image_t *
R_LoadPic (char *name, byte *pic, int width, int realwidth, int height, int realheight,
	size_t data_size, imagetype_t type)
{
	image_t	*image;
	size_t	size, full_size;

	size = width * height;

	/* data_size/size are unsigned */
	if (!pic || data_size == 0 || width <= 0 || height <= 0 || size == 0)
		return NULL;

	image = R_FindFreeImage();
	if (strlen(name) >= sizeof(image->name))
		ri.Sys_Error(ERR_DROP, "%s: '%s' is too long", __func__, name);
	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->asset_width = realwidth;
	image->asset_height = realheight;
	image->type = type;

	full_size = R_GetImageMipsSize(size);
	image->pixels[0] = malloc(full_size);
	if (!image->pixels[0])
	{
		ri.Sys_Error(ERR_FATAL, "%s: Can't allocate image.", __func__);
		// code never returns after ERR_FATAL
		return NULL;
	}

	image->transparent = false;
	if (type != it_wall)
	{
		size_t i;

		for (i=0 ; i<size ; i++)
		{
			if (pic[i] == 255)
			{
				image->transparent = true;
				break;
			}
		}
	}

	if (data_size > full_size)
	{
		data_size = full_size;
	}
	memcpy(image->pixels[0], pic, data_size);

	// restore mips
	R_RestoreImagePointers(image, 0);

	if (full_size > data_size)
	{
		// looks short, restore everything from first image
		R_RestoreMips(image, 0);
	}

	return image;
}

/*
================
R_LoadWal
================
*/
static image_t *
R_LoadWal (char *name, imagetype_t type)
{
	miptex_t	*mt;
	int		ofs;
	image_t		*image;
	size_t		file_size, width, height;

	file_size = ri.FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, name);
		return r_notexture_mip;
	}

	if (file_size < sizeof(miptex_t))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small header\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return r_notexture_mip;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong(mt->offsets[0]);

	/* width/height are unsigned */
	if ((ofs <= 0) || (width == 0) || (height == 0) ||
	    ((file_size - ofs) / width < height))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small body\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return r_notexture_mip;
	}

	image = R_LoadPic(name, (byte *)mt + ofs,
					width, width,
					height, height,
					(file_size - ofs), type);

	ri.FS_FreeFile((void *)mt);

	return image;
}

static byte *d_16to8table = NULL; // 16 to 8 bit conversion table

/*
 * Apply color light to texture pixel
 *
 * TODO: -22% fps lost
 */
pixel_t
R_ApplyLight(pixel_t pix, const light3_t light)
{
	light3_t light_masked;
	pixel_t i_r, i_g, i_b;
	byte b_r, b_g, b_b;
	int i_c;

	light_masked[0] = light[0] & LIGHTMASK;
	light_masked[1] = light[1] & LIGHTMASK;
	light_masked[2] = light[2] & LIGHTMASK;

	/* same light or colorlight == 0 */
	if (light_masked[0] == light_masked[1] && light_masked[0] == light_masked[2])
		return vid_colormap[pix + light_masked[0]];

	/* get index of color component of each component */
	i_r = vid_colormap[light_masked[0] + pix];
	i_g = vid_colormap[light_masked[1] + pix];
	i_b = vid_colormap[light_masked[2] + pix];

	/* get color component for each component */
	b_r = d_8to24table[i_r * 4 + 0];
	b_g = d_8to24table[i_g * 4 + 1];
	b_b = d_8to24table[i_b * 4 + 2];

	/* convert back to indexed color */
	b_r = ( b_r >> 3 ) & 31;
	b_g = ( b_g >> 2 ) & 63;
	b_b = ( b_b >> 3 ) & 31;

	i_c = b_r | ( b_g << 5 ) | ( b_b << 11 );

	return d_16to8table[i_c & 0xFFFF];
}

static void
R_Convert32To8bit(const unsigned char* pic_in, pixel_t* pic_out, size_t size)
{
	size_t i;

	if (!d_16to8table)
		return;

	for(i=0; i < size; i++)
	{
		unsigned int r, g, b, c;

		r = ( pic_in[0] >> 3 ) & 31;
		g = ( pic_in[1] >> 2 ) & 63;
		b = ( pic_in[2] >> 3 ) & 31;

		c = r | ( g << 5 ) | ( b << 11 );

		pic_out[i] = d_16to8table[c & 0xFFFF];
		pic_in += 4;
	}
}

static void
R_FixPalette(pixel_t* pixels, size_t size, const rgb_t* pallette)
{
	pixel_t* convert = malloc(256);

	size_t i;

	if (!d_16to8table)
	{
		free(convert);
		return;
	}

	for(i=0; i < 256; i ++)
	{
		unsigned int r, g, b, c;

		r = ( pallette[i].r >> 3 ) & 31;
		g = ( pallette[i].g >> 2 ) & 63;
		b = ( pallette[i].b >> 3 ) & 31;

		c = r | ( g << 5 ) | ( b << 11 );

		convert[i] = d_16to8table[c & 0xFFFF];
	}

	for(i=0; i < size; i++)
	{
		pixels[i] = convert[pixels[i]];
	}
	free(convert);
}

/*
================
R_LoadM8
================
*/
static image_t *
R_LoadM8 (char *name, imagetype_t type)
{
	m8tex_t	*mt;
	int		ofs, file_size;
	image_t		*image;
	int		size;

	file_size = ri.FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, name);
		return r_notexture_mip;
	}

	if (file_size < sizeof(m8tex_t))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small header\n", __func__, name);
		ri.FS_FreeFile ((void *)mt);
		return r_notexture_mip;
	}

	if (LittleLong (mt->version) != M8_VERSION)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, wrong magic value.\n", __func__, name);
		ri.FS_FreeFile ((void *)mt);
		return r_notexture_mip;
	}

	image = R_FindFreeImage ();
	strcpy (image->name, name);
	image->width = LittleLong (mt->width[0]);
	image->height = LittleLong (mt->height[0]);
	image->asset_width = image->width;
	image->asset_height = image->height;
	image->type = type;
	image->registration_sequence = registration_sequence;
	ofs = LittleLong (mt->offsets[0]);
	size = image->width * image->height * (256+64+16+4)/256;

	if ((ofs <= 0) || (image->width <= 0) || (image->height <= 0) ||
	    ((file_size - ofs) / image->width < image->height))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small body\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return r_notexture_mip;
	}

	image->pixels[0] = malloc (size);
	image->pixels[1] = image->pixels[0] + image->width*image->height;
	image->pixels[2] = image->pixels[1] + image->width*image->height/4;
	image->pixels[3] = image->pixels[2] + image->width*image->height/16;

	if (size > (file_size - ofs))
	{
		memcpy(image->pixels[0], (byte *)mt + ofs, file_size - ofs);
		// looks short, restore everything from first image
		R_ImageShrink(image->pixels[0], image->pixels[1],
			      image->height, image->height/2,
			      image->width, image->width/2);
		R_ImageShrink(image->pixels[1], image->pixels[2],
			      image->height/2, image->height/4,
			      image->width/2, image->width/4);
		R_ImageShrink(image->pixels[2], image->pixels[3],
			      image->height/4, image->height/8,
			      image->width/4, image->width/8);
	}
	else
	{
		memcpy ( image->pixels[0], (byte *)mt + ofs, size);
	}

	R_FixPalette(image->pixels[0], size, mt->palette);
	ri.FS_FreeFile ((void *)mt);
	return image;
}

static image_t	*
R_LoadHiColorImage(char *name, const char* namewe, const char *ext, imagetype_t type)
{
	image_t	*image = NULL;
	byte *pic = NULL;
	int realwidth = 0, realheight = 0;
	int width = 0, height = 0;

	if (strcmp(ext, "pcx") == 0)
	{
		/* Get size of the original texture */
		GetPCXInfo(name, &realwidth, &realheight);
	}
	else if (strcmp(ext, "wal") == 0)
	{
		/* Get size of the original texture */
		GetWalInfo(name, &realwidth, &realheight);
	}
	else if (strcmp(ext, "m8") == 0)
	{
		/* Get size of the original texture */
		GetM8Info(name, &realwidth, &realheight);
	}

	/* try to load a tga, png or jpg (in that order/priority) */
	if (  LoadSTB(namewe, "tga", &pic, &width, &height)
	   || LoadSTB(namewe, "png", &pic, &width, &height)
	   || LoadSTB(namewe, "jpg", &pic, &width, &height) )
	{
		if (width >= realwidth && height >= realheight)
		{
			// resulted image
			byte* pic8 = NULL;
			// resulted image memory size
			size_t size8;

			if (realheight == 0 || realwidth == 0)
			{
				realheight = height;
				realwidth = width;
			}

			size8 = R_GetImageMipsSize(width * height);
			pic8 = malloc(size8);

			if (!pic8)
			{
				ri.Sys_Error(ERR_FATAL, "%s: Can't allocate image.", __func__);
				// code never returns after ERR_FATAL
				return NULL;
			}

			if (width != realwidth || height != realheight)
			{
				// temporary place for shrinked image
				byte* pic32 = NULL;
				// temporary image memory size
				size_t size32;
				int uploadwidth, uploadheight;

				if (type == it_pic)
				{
					uploadwidth = realwidth;
					uploadheight = realheight;

					// search next scale up
					while ((uploadwidth < width) && (uploadheight < height))
					{
						uploadwidth *= 2;
						uploadheight *= 2;
					}

					// one step back
					if ((uploadwidth > width) || (uploadheight > height))
					{
						uploadwidth /= 2;
						uploadheight /= 2;
					}
				}
				else
				{
					uploadwidth = realwidth;
					uploadheight = realheight;
				}

				// resize image
				size32 = width * height * 4;
				pic32 = malloc(size32);

				if (ResizeSTB(pic, width, height,
					      pic32, uploadwidth, uploadheight))
				{
					R_Convert32To8bit(pic32, pic8, uploadwidth * uploadheight);
					image = R_LoadPic(name, pic8,
								uploadwidth, realwidth,
								uploadheight, realheight,
								uploadwidth * uploadheight, type);
				}
				free(pic32);
			}
			else
			{
				R_Convert32To8bit(pic, pic8, width * height);
				image = R_LoadPic(name, pic8,
								width, width,
								height, height,
								width * height, type);
			}
			free(pic8);
		}
	}

	if (pic)
	{
		free(pic);
	}

	return image;
}

static image_t	*
R_LoadImage(char *name, const char* namewe, const char *ext, imagetype_t type)
{
	image_t	*image = NULL;

	// with retexturing and not skin
	if (r_retexturing->value)
	{
		image = R_LoadHiColorImage(name, namewe, ext, type);
	}

	if (!image)
	{
		if (strcmp(ext, "pcx") == 0)
		{
			byte *pic = NULL;
			byte	*palette = NULL;
			int width = 0, height = 0;

			LoadPCX (name, &pic, &palette, &width, &height);
			if (!pic)
				return NULL;

			if (r_scale8bittextures->value && type == it_pic)
			{
				byte *scaled = NULL;
				int realwidth, realheight;

				// save original size
				realwidth = width;
				realheight = height;

				scaled = malloc(width * height * 4);
				if (!scaled)
					return NULL;

				scale2x(pic, scaled, width, height);
				width *= 2;
				height *= 2;
				image = R_LoadPic(name, scaled,
								width, realwidth,
								height, realheight,
								width * height, type);
				free(scaled);
			}
			else
			{
				image = R_LoadPic(name, pic,
								width, width,
								height, height,
								width * height, type);
			}

			if (palette)
			{
				free(palette);
			}
			free(pic);
		}
		else if (strcmp(ext, "wal") == 0)
		{
			image = R_LoadWal(name, type);
		}
		else if (strcmp(ext, "m8") == 0)
		{
			image = R_LoadM8 (name, type);
		}
	}

	return image;
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*
R_FindImage(char *name, imagetype_t type)
{
	image_t	*image;
	int	i, len;
	char *ptr;
	char namewe[256];
	const char* ext;

	if (!name)
	{
		return NULL;
	}

	/* just return white image if show lighmap only */
	if ((type == it_wall || type == it_skin) && r_lightmap->value)
	{
		return r_whitetexture_mip;
	}

	ext = COM_FileExtension(name);
	if(!ext[0])
	{
		/* file has no extension */
		return NULL;
	}

	len = strlen(name);

	/* Remove the extension */
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - (strlen(ext) + 1));

	if (len < 5)
	{
		return NULL;
	}

	/* fix backslashes */
	while ((ptr = strchr(name, '\\')))
	{
		*ptr = '/';
	}

	// look for it
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//
	// load the pic from disk
	//
	return R_LoadImage(name, namewe, ext, type);
}

/*
================
R_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void
R_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
		{
			continue; // used this sequence
		}
		if (!image->registration_sequence)
			continue; // free texture
		if (image->type == it_pic)
			continue; // don't free pics
		// free it
		free (image->pixels[0]); // the other mip levels just follow
		memset(image, 0, sizeof(*image));
	}
}

bool
R_ImageHasFreeSpace(void)
{
	int		i, used;
	image_t	*image;

	used = 0;

	for (i = 0, image = r_images; i < numr_images; i++, image++)
	{
		if (!image->name[0])
			continue;
		if (image->registration_sequence == registration_sequence)
		{
			used ++;
		}
	}

	if (image_max < used)
	{
		image_max = used;
	}

	// should same size of free slots as currently used
	return (numr_images + used) < MAX_RIMAGES;
}

static struct texture_buffer {
	image_t	image;
	byte	buffer[4096];
} r_notexture_buffer, r_whitetexture_buffer;

static void
R_InitNoTexture(void)
{
	int	m;

	// create a simple checkerboard texture for the default
	r_notexture_mip = &r_notexture_buffer.image;

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->asset_width = r_notexture_mip->asset_height = 16;

	r_notexture_mip->pixels[0] = r_notexture_buffer.buffer;
	R_RestoreImagePointers(r_notexture_mip, 0);

	for (m=0 ; m<NUM_MIPS ; m++)
	{
		int		x, y;
		byte	*dest;

		dest = r_notexture_mip->pixels[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )

					*dest++ = d_16to8table[0x0000];
				else
					*dest++ = d_16to8table[0xFFFF];
			}
	}
}

static void
R_InitWhiteTexture(void)
{
	// create a simple white texture for the default
	r_whitetexture_mip = &r_whitetexture_buffer.image;

	r_whitetexture_mip->width = r_whitetexture_mip->height = 16;
	r_whitetexture_mip->asset_width = r_whitetexture_mip->asset_height = 16;

	r_whitetexture_mip->pixels[0] = r_whitetexture_buffer.buffer;
	R_RestoreImagePointers(r_whitetexture_mip, 0);

	memset(r_whitetexture_buffer.buffer, d_16to8table[0xFFFF],
		sizeof(r_whitetexture_buffer.buffer));
}

/*
==================
R_InitTextures
==================
*/
static void
R_InitTextures (void)
{
	R_InitNoTexture();
	/* empty white texture for r_lightmap = 1*/
	R_InitWhiteTexture();
}

/*
===============
R_InitImages
===============
*/
void
R_InitImages (void)
{
	unsigned char * table16to8;
	registration_sequence = 1;
	image_max = 0;

	d_16to8table = NULL;
	ri.FS_LoadFile("pics/16to8.dat", (void **)&table16to8);

	if ( !table16to8 )
	{
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't load pics/16to8.dat", __func__);
		// code never returns after ERR_FATAL
		return;
	}

	d_16to8table = malloc(0x10000);
	if ( !d_16to8table )
	{
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't allocate memory for d_16to8table", __func__);
		// code never returns after ERR_FATAL
		return;
	}
	memcpy(d_16to8table, table16to8, 0x10000);
	ri.FS_FreeFile((void *)table16to8);

	R_InitTextures ();
}

/*
===============
R_ShutdownImages
===============
*/
void
R_ShutdownImages (void)
{
	int	i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (!image->registration_sequence)
			continue; // free texture

		// free it
		if (image->pixels[0])
			free(image->pixels[0]); // the other mip levels just follow

		memset(image, 0, sizeof(*image));
	}

	if (d_16to8table)
		free(d_16to8table);
}
