#pragma once

#include <png.h>
#include <stdint.h>

#define FB_WIDTH 1024
#define FB_HEIGHT 768
#define FB_SIZE (FB_WIDTH*FB_HEIGHT)
    
/* A coloured pixel. */
struct pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};
typedef struct pixel pixel_t;

/* A picture. */
struct bitmap {
    pixel_t *pixels;
    size_t width;
    size_t height;
};
typedef struct bitmap bitmap_t;

/* Given "bitmap", this returns the pixel of bitmap at the point 
   ("x", "y"). */
pixel_t *pixel_at(bitmap_t *bitmap, int x, int y);

/* Write "bitmap" to a PNG file specified by "path"; returns 0 on
   success, non-zero on error. */
int gfx_save_png(bitmap_t *bitmap, const char *path);
