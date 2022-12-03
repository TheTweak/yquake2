//
//  Image.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.09.2022.
//

#include <fstream>
#include <utility>
#include <array>
#include <memory>

#include "Image.hpp"

image_s::~image_s() {
    if (data != NULL) {
        free(data);
    }
}

static std::array<int, 768> _palette;
static bool _palette_loaded = false;

std::array<int, 768> _LoadPalette() {
    pcx_t *pcx;
    std::byte *raw;
    
    int size = ri.FS_LoadFile("pics/colormap.pcx", (void **)&raw);

    /* parse the PCX file */
    pcx = (pcx_t *)raw;
    
    std::byte *pal = static_cast<std::byte*>(malloc(768));
    assert(pal);
    
    memcpy(pal, (std::byte *)pcx + size - 768, 768);
    
    std::array<int, 768> result;
    for (int i = 0; i < 256; i++) {
        int r = (int)pal[i * 3 + 0];
        int g = (int)pal[i * 3 + 1];
        int b = (int)pal[i * 3 + 2];
        
        result[i * 3] = r;
        result[i * 3 + 1] = g;
        result[i * 3 + 2] = b;
    }
        
    free(pal);
        
    return result;
}

std::pair<int, int> _LoadPCX(byte **pic, char* origname) {
    
    if (!_palette_loaded) {
        _palette = _LoadPalette();
        _palette_loaded = true;
    }
    
    byte *raw;
    pcx_t *pcx;
    int x = 0, y = 0;
    int len = 0, full_size = 0;
    int pcx_width = 0, pcx_height = 0;
    bool image_issues = false;
    int dataByte = 0, runLength = 0;
    byte *out, *pix;
    
    *pic = NULL;

    char filename[256];

    Q_strlcpy(filename, origname, sizeof(filename));

    /* Add the extension */
    if (strcmp(COM_FileExtension(filename), "pcx"))
    {
        Q_strlcat(filename, ".pcx", sizeof(filename));
    }
    
    /* load the file */
    len = ri.FS_LoadFile(filename, (void **)&raw);
    
    if (!raw || len < sizeof(pcx_t))
    {
        R_Printf(PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
        return {0, 0};
    }
    
    /* parse the PCX file */
    pcx = (pcx_t *)raw;
    raw = reinterpret_cast<byte*>(&pcx->data);

    pcx_width = pcx->xmax - pcx->xmin;
    pcx_height = pcx->ymax - pcx->ymin;
    
    if ((pcx->manufacturer != 0x0a) || (pcx->version != 5) ||
        (pcx->encoding != 1) || (pcx->bits_per_pixel != 8) ||
        (pcx_width >= 4096) || (pcx_height >= 4096))
    {
        R_Printf(PRINT_ALL, "Bad pcx file %s\n", filename);
        ri.FS_FreeFile(pcx);
        return {0, 0};
    }

    full_size = 4 * (pcx_height + 1) * (pcx_width + 1);
    out = static_cast<byte*>(malloc(full_size));
    
    if (!out)
    {
        R_Printf(PRINT_ALL, "Can't allocate\n");
        ri.FS_FreeFile(pcx);
        return {0, 0};
    }
    
    *pic = out;

    pix = out;
        
    for (y = 0; y <= pcx_height; y++)
    {
        for (x = 0; x <= pcx_width;)
        {
            if (raw - (byte *)pcx > len)
            {
                // no place for read
                image_issues = true;
                x = pcx_width;
                break;
            }
            dataByte = static_cast<int>(*raw++);

            if ((dataByte & 0xC0) == 0xC0)
            {
                runLength = dataByte & 0x3F;
                if (raw - (byte *)pcx > len)
                {
                    // no place for read
                    image_issues = true;
                    x = pcx_width;
                    break;
                }
                dataByte = static_cast<int>(*raw++);
            }
            else
            {
                runLength = 1;
            }
                        
            while (runLength-- > 0)
            {
                if ((*pic + full_size) <= (pix + x))
                {
                    // no place for write
                    image_issues = true;
                    x += runLength;
                    runLength = 0;
                } else {
                    size_t index = (y * pcx_width + x) * 4;
                    x++;
                    
                    pix[index] = byte(_palette[dataByte * 3 + 2]);    // blue
                    pix[index + 1] = byte(_palette[dataByte * 3 + 1]); // green
                    pix[index + 2] = byte(_palette[dataByte * 3]);    // red
                    pix[index + 3] = static_cast<byte>(dataByte == 255 ? 0 : 255); // alpha
                }
            }
        }
    }
        
    if (image_issues)
    {
        R_Printf(PRINT_ALL, "PCX file %s has possible size issues.\n", filename);
    }
                
    ri.FS_FreeFile(pcx);
    return {pcx_width, pcx_height};
}

void ApplyPalette(byte* paletted, byte* orig, int width, int height) {
    if (!_palette_loaded) {
        _palette = _LoadPalette();
        _palette_loaded = true;
    }
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t index = (y * width + x) * 4;
            int colorIndex = static_cast<int>(*orig++);
            paletted[index] = byte(_palette[colorIndex * 3 + 2]);
            paletted[index + 1] = byte(_palette[colorIndex * 3 + 1]);
            paletted[index + 2] = byte(_palette[colorIndex * 3]);
            paletted[index + 3] = byte{255};
        }
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
    size_t size = 4 * max(width, 1) * max(height, 1);
    result->data = static_cast<byte*>(malloc(size));
    if (type != it_pic) {
        ApplyPalette(result->data, pic, width, height);
    } else {
        memcpy(result->data, pic, size);
    }
    return result;
}

std::optional<image_s*> Img::LoadM8(char *origname, imagetype_t type) {
    m8tex_t *mt;
    int width, height, ofs, size;
    image_s *image;
    char name[256];
    unsigned char *image_buffer = NULL;

    Q_strlcpy(name, origname, sizeof(name));

    /* Add the extension */
    if (strcmp(COM_FileExtension(name), "m8"))
    {
        Q_strlcat(name, ".m8", sizeof(name));
    }

    size = ri.FS_LoadFile(name, (void **)&mt);

    if (!mt)
    {
        R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, name);
        return std::nullopt;
    }

    if (size < sizeof(m8tex_t))
    {
        R_Printf(PRINT_ALL, "%s: can't load %s, small header\n", __func__, name);
        ri.FS_FreeFile((void *)mt);
        return std::nullopt;
    }

    if (LittleLong (mt->version) != M8_VERSION)
    {
        R_Printf(PRINT_ALL, "LoadWal: can't load %s, wrong magic value.\n", name);
        ri.FS_FreeFile ((void *)mt);
        return std::nullopt;
    }

    width = LittleLong(mt->width[0]);
    height = LittleLong(mt->height[0]);
    ofs = LittleLong(mt->offsets[0]);

    if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
        (((size - ofs) / height) < width))
    {
        R_Printf(PRINT_ALL, "%s: can't load %s, small body\n", __func__, name);
        ri.FS_FreeFile((void *)mt);
        return std::nullopt;
    }

    image_buffer = static_cast<unsigned char*>(malloc (width * height * 4));
    for(int i=0; i<width * height; i++)
    {
        unsigned char value = *((byte *)mt + ofs + i);
        image_buffer[i * 4 + 0] = mt->palette[value].r;
        image_buffer[i * 4 + 1] = mt->palette[value].g;
        image_buffer[i * 4 + 2] = mt->palette[value].b;
        image_buffer[i * 4 + 3] = value == 255 ? 0 : 255;
    }

    image = LoadPic(name, image_buffer, width, height, 0, 0, type, 32);
    free(image_buffer);

    ri.FS_FreeFile((void *)mt);

    return image;
}

std::optional<image_s*> Img::LoadWal(char *origname, imagetype_t type) {
    miptex_t *mt;
    int width, height, ofs, size;
    image_s *image;
    char name[256];

    Q_strlcpy(name, origname, sizeof(name));

    /* Add the extension */
    if (strcmp(COM_FileExtension(name), "wal"))
    {
        Q_strlcat(name, ".wal", sizeof(name));
    }

    size = ri.FS_LoadFile(name, (void **)&mt);

    if (!mt)
    {
        R_Printf(PRINT_ALL, "LoadWal: can't load %s\n", name);
        return std::nullopt;
    }

    if (size < sizeof(miptex_t))
    {
        R_Printf(PRINT_ALL, "LoadWal: can't load %s, small header\n", name);
        ri.FS_FreeFile((void *)mt);
        return std::nullopt;
    }

    width = LittleLong(mt->width);
    height = LittleLong(mt->height);
    ofs = LittleLong(mt->offsets[0]);

    if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
        (((size - ofs) / height) < width))
    {
        R_Printf(PRINT_ALL, "LoadWal: can't load %s, small body\n", name);
        ri.FS_FreeFile((void *)mt);
        return std::nullopt;
    }

    image = LoadPic(name, (byte *)mt + ofs, width, height, 0, 0, type, 8);

    ri.FS_FreeFile((void *)mt);

    return image;
}

const std::map<std::string, std::shared_ptr<image_s>>& Img::GetLoadedImages() const {
    return _imageCache;
}

image_s* Img::FindImage(char* name, imagetype_t type) {
    if (auto it = _imageCache.find(name); it != end(_imageCache)) {
        return it->second.get();
    }
    
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
        GetPCXInfo(name, &realwidth, &realheight);
        if(realwidth == 0)
        {
            /* No texture found */
            return NULL;
        }

        /* try to load a tga, png or jpg (in that order/priority) */
        if (  LoadSTB(namewe, "tga", &pic, &width, &height)
           || LoadSTB(namewe, "png", &pic, &width, &height)
           || LoadSTB(namewe, "jpg", &pic, &width, &height) )
        {
            /* upload tga or png or jpg */
            image = LoadPic(name, pic, width, height, realwidth, realheight, type, 32);
        }
        else
        {
            /* PCX if no TGA/PNG/JPEG available (exists always) */
            auto [width, height] = _LoadPCX(&pic, name);

            if (!pic)
            {
                /* No texture found */
                return NULL;
            }

            /* Upload the PCX */
            image = LoadPic(name, pic, width, height, 0, 0, type, 8);
        }
    }
    else if (strcmp(ext, "wal") == 0 || strcmp(ext, "m8") == 0)
    {
        /* Get size of the original texture */
        if (strcmp(ext, "m8") == 0) {
            GetM8Info(name, &realwidth, &realheight);
        } else {
            GetWalInfo(name, &realwidth, &realheight);
        }

        if (realwidth == 0) {
            /* No texture found */
            return NULL;
        }

        /* try to load a tga, png or jpg (in that order/priority) */
        if (  LoadSTB(namewe, "tga", &pic, &width, &height)
           || LoadSTB(namewe, "png", &pic, &width, &height)
           || LoadSTB(namewe, "jpg", &pic, &width, &height) ) {
            /* upload tga or png or jpg */
            image = LoadPic(name, pic, width, height, realwidth, realheight, type, 32);
        } else if (strcmp(ext, "m8") == 0) {
            if (auto img = LoadM8(namewe, type); img != std::nullopt) {
                image = img.value();
            }
        } else {
            /* WAL if no TGA/PNG/JPEG available (exists always) */
            if (auto img = LoadWal(namewe, type); img != std::nullopt) {
                image = img.value();
            }
        }

        if (!image) {
            /* No texture found */
            return NULL;
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
            image = LoadPic(name, pic, width, height, realwidth, realheight, type, 32);
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

    _imageCache[name] = std::shared_ptr<image_s>(image);
    
    return image;
}

std::array<float, 4> Img::GetPalleteColor(int c, float alpha) {
    if (c > 255) {
        c = 255;
    }
    std::array<float, 4> result;
    result[3] = alpha;
    result[2] = _palette[c * 3 + 2];
    result[1] = _palette[c * 3 + 1];
    result[0] = _palette[c * 3];
    return result;
}
