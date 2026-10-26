/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#include <SDL_image.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "base_config.h"
#include "base_image.h"

#include "fs.h"
#include "fs_rwops.h"

/*---------------------------------------------------------------------------*/

void image_size(int *W, int *H, int w, int h)
{
    /* Round the image size up to the next power-of-two. */

    *W = w ? 1 : 0;
    *H = h ? 1 : 0;

    while (*W < w) *W *= 2;
    while (*H < h) *H *= 2;
}

void image_near2(int *W, int *H, int w, int h)
{
    image_size(W, H, w, h);

    if (*W > 1 && (*W - w > w - (*W / 2)))
        *W /= 2;
    if (*H > 1 && (*H - h > h - (*H / 2)))
        *H /= 2;
}

/*---------------------------------------------------------------------------*/

/* PXA: use streamed SDL RWops, preserving the OpenGL bottom-left row order. */
void *image_load(const char *name,int *width,int *height,int *bytes)
{
    SDL_Surface *surface=IMG_Load_RW(fs_rwops_open(name,"r"),1);
    if(!surface)return NULL;
    unsigned char *pixels=malloc((size_t)surface->w*surface->h*4);
    if(pixels) {
        for(int y=0;y<surface->h;++y)
            memcpy(pixels+(size_t)(surface->h-y-1)*surface->w*4,
                (unsigned char*)surface->pixels+(size_t)y*surface->pitch,(size_t)surface->w*4);
        *width=surface->w;*height=surface->h;*bytes=4;
    }
    SDL_FreeSurface(surface);return pixels;
}

void *image_next2(const void *p, int w, int h, int b, int *w2, int *h2)
{
    unsigned char *src = (unsigned char *) p;
    unsigned char *dst = NULL;

    int W;
    int H;

    image_size(&W, &H, w, h);

    if ((dst = (unsigned char *) calloc(W * H * b, sizeof (unsigned char))))
    {
        const int dr = (H - h) / 2;
        const int dc = (W - w) / 2;

        int r;

        for (r = 0; r < h; ++r)
        {
            const int R = r + dr;
            const int C =     dc;

            memcpy(&dst[(R * W + C) * b], &src[(r * w) * b], w * b);
        }

        if (w2) *w2 = W;
        if (h2) *h2 = H;
    }

    return dst;
}

/*
 * Allocate and return a new down-sampled image buffer.
 */
void *image_scale(const void *p, int w, int h, int b, int *wn, int *hn, int n)
{
    unsigned char *src = (unsigned char *) p;
    unsigned char *dst = NULL;

    int W = w / n;
    int H = h / n;

    if ((dst = (unsigned char *) calloc(W * H * b, sizeof (unsigned char))))
    {
        int si, di;
        int sj, dj;
        int i;

        /* Iterate each component of each destination pixel. */

        for (di = 0; di < H; di++)
            for (dj = 0; dj < W; dj++)
                for (i = 0; i < b; i++)
                {
                    int c = 0;

                    /* Average the NxN source pixel block for each. */

                    for (si = di * n; si < (di + 1) * n; si++)
                        for (sj = dj * n; sj < (dj + 1) * n; sj++)
                            c += src[(si * w + sj) * b + i];

                    dst[(di * W + dj) * b + i] =
                        (unsigned char) (c / (n * n));
                }

        if (wn) *wn = W;
        if (hn) *hn = H;
    }

    return dst;
}

/*
 * Whiten the RGB channels of the given image without touching any alpha.
 */
void image_white(void *p, int w, int h, int b)
{
    unsigned char *s = (unsigned char *) p;

    int i;

    assert(b >= 1 && b <= 4);

    if (b == 1 || b == 3)
    {
        memset(s, 0xFF, w * h * b);
    }
    else if (b == 2)
    {
        for (i = 0; i < w * h * b; i += 2)
            s[i] = 0xFF;
    }
    else
    {
        for (i = 0; i < w * h * b; i += 4)
        {
            s[i + 0] = 0xFF;
            s[i + 1] = 0xFF;
            s[i + 2] = 0xFF;
        }
    }
}

/*
 * Allocate and return an image buffer of the given image flipped horizontally
 * and/or vertically.
 */
void *image_flip(const void *p, int w, int h, int b, int hflip, int vflip)
{
    unsigned char *q;

    assert(hflip || vflip);

    if (!p)
        return NULL;

    if ((q = malloc(w * b * h)))
    {
        int r, c, i;

        for (r = 0; r < h; r++)
            for (c = 0; c < w; c++)
                for (i = 0; i < b; i++)
                {
                    int pr = vflip ? h - r - 1 : r;
                    int pc = hflip ? w - c - 1 : c;

                    int qi = r  * w * b + c  * b + i;
                    int pi = pr * w * b + pc * b + i;

                    q[qi] = ((const unsigned char *) p)[pi];
                }
        return q;
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
