//
//  Image.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.09.2022.
//

#include "Image.hpp"

image_s::~image_s() {
    if (data) {
        free(data);
    }
}

image_s* LoadPic(char* name, byte* pic, int width,
                 int height, int realWidth, int realHeight,
                 imagetype_t type, int bits) {
    image_s* result = new image_s();
    result->width = width;
    result->height = height;
    result->path = name;
    if (realWidth && realHeight &&
        (realWidth <= result->width && realHeight <= result->height)) {
        result->width = realWidth;
        result->height = realHeight;
    }
    size_t size = max(width, 1) * max(height, 1);
    result->data = static_cast<byte*>(malloc(size));
    memcpy(result->data, pic, size);
    return result;
}

image_s* Img::FindImage(char* name, imagetype_t type) {
    image_s* image;
    int i, len;
    byte *pic;
    int width, height;
    char *ptr;
    char namewe[256];
    int realwidth = 0, realheight = 0;
    const char* ext;

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
    
    pic = NULL;
    
    if (strcmp(ext, "pcx") == 0)
    {
        LoadPCX(name, &pic, NULL, &width, &height);

        if (!pic)
        {
            return NULL;
        }

        image = LoadPic(name, pic, width, height, 0, 0, type, 8);
    }
    else if (strcmp(ext, "wal") == 0 || strcmp(ext, "m8") == 0)
    {
        if (strcmp(ext, "m8") == 0)
        {
//            image = LoadM8(name, type);

            if (!image)
            {
                /* No texture found */
                return NULL;
            }
        }
        else /* gl_retexture is not set */
        {
//            image = LoadWal(name, type);

            if (!image)
            {
                /* No texture found */
                return NULL;
            }
        }
    }
    else if (strcmp(ext, "tga") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0)
    {
        char tmp_name[256];

        realwidth = 0;
        realheight = 0;

        strcpy(tmp_name, namewe);
        strcat(tmp_name, ".wal");
        GetWalInfo(tmp_name, &realwidth, &realheight);

        if (realwidth == 0 || realheight == 0) {
            strcpy(tmp_name, namewe);
            strcat(tmp_name, ".m8");
            GetM8Info(tmp_name, &realwidth, &realheight);
        }

        if (realwidth == 0 || realheight == 0) {
            /* It's a sky or model skin. */
            strcpy(tmp_name, namewe);
            strcat(tmp_name, ".pcx");
            GetPCXInfo(tmp_name, &realwidth, &realheight);
        }

        /* TODO: not sure if not having realwidth/heigth is bad - a tga/png/jpg
         * was requested, after all, so there might be no corresponding wal/pcx?
         * if (realwidth == 0 || realheight == 0) return NULL;
         */

        if(LoadSTB(name, ext, &pic, &width, &height))
        {
//            image = GL3_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
        } else {
            return NULL;
        }
    }
    else
    {
        return NULL;
    }

    if (pic)
    {
        free(pic);
    }

    return image;
}
