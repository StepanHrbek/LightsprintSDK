#ifndef _RASTER_H
#define _RASTER_H

typedef unsigned char U8;

typedef float raster_MATRIX[4][4];

typedef struct {
        float x,y,z;
        float u,v,i;
        float sx,sy;
        float tx,ty,tz;
        float dist[4];
        } raster_POINT;

typedef struct raster_VERTEX {
        raster_POINT *point;
        struct raster_VERTEX *next;
        } raster_POLYGON;

#define raster_VERTEX raster_POLYGON

extern int *raster_Output;

void raster_Init(int xres, int yres);
void raster_SetFOV(float xfov, float yfov);
void raster_SetMatrix(raster_MATRIX *cam, raster_MATRIX *inv);
void raster_Clear();
//!!!...
int raster_ZGouraud(raster_POLYGON *p, unsigned *col);
int raster_LGouraud(raster_POLYGON *p, int w, U8 *lightmap);
int raster_ZFlat(raster_POLYGON *p, unsigned *col, float brightness);
int raster_LFlat(raster_POLYGON *p, int w, U8 *lightmap, float brightness);
int raster_ZTexture(raster_POLYGON *p, int w, U8 *lightmap, unsigned *col);

#endif
