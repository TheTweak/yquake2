//
//  Constants.h
//  yq2
//
//  Created by SOROKIN EVGENY on 20.12.2022.
//

#ifndef Constants_h
#define Constants_h

#define PIXEL_FORMAT MTL::PixelFormatBGRA8Unorm
#define MAX_FRAMES_IN_FLIGHT 3
#define MAX_LIGHTMAPS_PER_SURFACE 4
#define FADE_SCREEN_TEXTURE "_FADE_SCREEN"
#define FLASH_SCREEN_TEXTURE "_FLASH_SCREEN"
#define FILL_TEXTURE "_FILL_SCREEN_"
#define BACKFACE_EPSILON 0.01
#define MAX_CLIP_VERTS 64
#define ON_EPSILON 0.1 /* point on plane side epsilon */

#define RAY_MASK_PRIMARY 3
#define RAY_STRIDE 48
#define TRIANGLE_MASK_GEOMETRY 1
#define TRIANGLE_MASK_LIGHT    2

#define RT_TEXTURE_ARRAY_SIZE 100

#endif /* Constants_h */
