#ifndef _SURFACE_H
#define _SURFACE_H

#include "geometry.h"
#include "Lightsprint/RRObject.h"

#define ColorTable unsigned * // 256 colors in common 32bit format (BGRA)
#define Color rr::RRVec3

struct Material
{
	real       transparency;
	real       lightspeed;
	RRVec3    color;
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
