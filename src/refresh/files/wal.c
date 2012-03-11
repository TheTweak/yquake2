#include "../header/local.h"

image_t *
LoadWal ( char *oldname )
{
	miptex_t    *mt;
	int width, height, ofs;
	image_t     *image;
	int len;
	char name[256];

	len = strlen( oldname );

	if ( strcmp( oldname + len - 4, ".wal" ) )
	{
		strncpy(name, oldname, 256);
		strncat(name, ".wal", 255);
	}
	else
	{
		strncpy(name, oldname, 256);
	}

	ri.FS_LoadFile( name, (void **) &mt );

	if ( !mt )
	{
		ri.Con_Printf( PRINT_ALL, "LoadWall: can't load %s\n", name );
		return ( r_notexture );
	}

	width = LittleLong( mt->width );
	height = LittleLong( mt->height );
	ofs = LittleLong( mt->offsets [ 0 ] );

	image = R_LoadPic( name, (byte *) mt + ofs, width, height, it_wall, 8 );

	ri.FS_FreeFile( (void *) mt );

	return ( image );
}

qboolean GetWalInfo (char *name, int *width, int *height)
{
	miptex_t	*mt;

	ri.FS_LoadFile (name, (void **)&mt);

	if (!mt)
	{
		return false;
	}

	*width = LittleLong (mt->width);
	*height = LittleLong (mt->height);

	ri.FS_FreeFile ((void *)mt);
	return true;
}

