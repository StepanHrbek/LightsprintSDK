#ifndef _SURFACE_H
#define _SURFACE_H

#include "geometry.h"
#include "Lightsprint/RRVision.h"

#define ColorTable unsigned * // 256 colors in common 32bit format (BGRA)
#define Color rr::RRColor

struct Material
{
	real       transparency;
	real       lightspeed;
	RRColor    color;
};

struct Texture;
struct Surface : public RRMaterial
{
	ColorTable diffuseReflectanceColorTable;
	//Point3     diffuseEmittancePoint;
	Material*  outside;
	Material*  inside;
	Texture*   texture;
};

#endif
