#ifndef _RASTERGL_H
#define _RASTERGL_H

#include "geometry.h"
#include "surface.h"

void raster_Init(int xres, int yres);
void raster_Clear();

void raster_SetFOV(float xfov, float yfov);
void raster_SetMatrix(MATRIX *cam, MATRIX *inv);
//void raster_SetLight(real pos[3], real dir[3], real col[3]);
void raster_SetMatrix(Point3 &cam,Vec3 &dir);

void raster_BeginTriangles();
void raster_ZFlat(Point3 *p, unsigned *col, real brightness);
void raster_ZGouraud(Point3 *p, unsigned *col, real brightness[3]);
void raster_EndTriangles();

//////////////////////////////////////////////////////////////////////////
//
// Occlusion queries
/*
struct OcclusionQueries
{
	OcclusionQueries(int amax);
	~OcclusionQueries();

	void Reset(bool newFrame);
	void Begin();
	void End();

	private:
		GLuint *queries;
		GLuint *samples;
		int qnow;
		int qmax;
};

extern OcclusionQueries queries;
*/
#endif
