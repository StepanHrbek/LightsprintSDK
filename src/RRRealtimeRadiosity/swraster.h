#ifndef _RASTER_H
#define _RASTER_H

typedef unsigned char raster_COLOR;

struct raster_VERTEX
{
        float sx,sy; // input: position
        float u;     // input: color 0..1
};

struct raster_POLYGON
{
        raster_VERTEX *point;
        raster_POLYGON *next;
};

int  raster_LGouraud(raster_POLYGON *p, raster_COLOR *lightmap, int width);
int  raster_LFlat(raster_POLYGON *p, raster_COLOR *lightmap, int width, raster_COLOR color);

#endif
