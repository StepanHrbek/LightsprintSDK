#ifndef _SURFACE_H
#define _SURFACE_H

#include "geometry.h"

namespace rrEngine
{

//////////////////////////////////////////////////////////////////////////////
//
// material aspects of space and surfaces

#define C_COMPS 3
#define C_INDICES 256

typedef real Color[C_COMPS]; // r,g,b 0..1

#define ColorTable unsigned * // 256 colors in common 32bit format (BGRA)

struct Material
{
	real    transparency;
	real    lightspeed;
	Color   color;
};

enum Emittance
{
	areaLight   =0, // svitici plocha
	pointLight  =1, // zdroj uprostred plochy, bod sviti do vsech smeru
	nearLight   =2, // zdroj v point, plocha sviti smerem od pointu
	distantLight=3, // zdroj v -nekonecno*point, plocha sviti smerem point
	nearLight2  =4, // docasne napraseno kvuli dvere.bsp
};

struct Texture;
struct Surface
{
	U8      sides;		// 1 if surface is 1-sided, 2 for 2-sided
	real    diffuseReflectance;
	Color   diffuseReflectanceColor;
	ColorTable diffuseReflectanceColorTable;
	real    diffuseTransmittance;
	Color   diffuseTransmittanceColor;
	real    diffuseEmittance;
	Color   diffuseEmittanceColor;
	Emittance diffuseEmittanceType;
	Point3  diffuseEmittancePoint;
	real    specularReflectance;
	Color   specularReflectanceColor;
	real    specularReflectanceRoughness;
	real    specularTransmittance;
	Color   specularTransmittanceColor;
	real    specularTransmittanceRoughness;
	real    refractionReal;
	real    refractionImaginary;
	Material *outside;
	Material *inside;
	Texture *texture;

	real    _rd;//needed when calculating different illumination for different components
	real    _rdcx;
	real    _rdcy;
	real    _ed;//needed by turnLight
};

//////////////////////////////////////////////////////////////////////////////
//
// surface behaviour bits

struct SideBits
{
	U8 renderFrom:1;  // is visible from that halfspace
	U8 emitTo:1;      // emits energy to that halfspace
	U8 catchFrom:1;   // stops rays from that halfspace and performs following operations: (otherwise ray continues as if nothing happened)
	U8 receiveFrom:1; //  receives energy from that halfspace
	U8 reflect:1;     //  reflects energy from that halfspace to that halfspace
	U8 transmitFrom:1;//  transmits energy from that halfspace to other halfspace
};

extern SideBits sideBits[3][2];

} // namespace

#endif
