// --------------------------------------------------------------------------
// Workarounds for driver and GPU quirks.
// Copyright (C) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstring>
#include <GL/glew.h>
#include "Workaround.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Workaround

static const char* renderer = NULL;
static bool isGeforce = false;
static bool isQuadro = false;
static bool isRadeon = false;
static bool isFire = false;
static unsigned modelNumber = 0;
static bool isDepthClampSupported = false;

#include <string.h>

static bool isExtensionSupported(const char *extension)
{
	// Extension names should not have spaces.
	GLubyte* where = (GLubyte*)strchr(extension,' ');
	if (where || *extension==0)
		return false;

	const GLubyte* extensions = glGetString(GL_EXTENSIONS);
	// It takes a bit of care to be fool-proof about parsing the OpenGL extensions string. Don't be fooled by sub-strings, etc.
	const GLubyte* start = extensions;
	for(GLubyte* where=(GLubyte*)strstr((const char*)start,extension);where;)
	{
		GLubyte *terminator = where + strlen(extension);
		if (where==start || *(where-1)==' ')
			if (*terminator==' ' || *terminator==0)
				return true;
		start = terminator;
	}
	return false;
}

static void init()
{
	static bool inited = 0;
	if (!inited)
	{
		inited = 1;

		renderer = (const char*)glGetString(GL_RENDERER);
		if (!renderer) renderer = "";

		isGeforce = strstr(renderer,"GeForce")!=NULL;
		isQuadro = strstr(renderer,"Quadro")!=NULL;
		isRadeon = strstr(renderer,"Radeon") || strstr(renderer,"RADEON");
		isFire = strstr(renderer,"Fire")!=NULL;

		// find 3-4digit model number
		#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
		for (unsigned i=0;renderer[i];i++)
			if (!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
			{
				modelNumber = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
				break;
			}
			else
			if (!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && !IS_DIGIT(renderer[i+4]))
			{
				modelNumber = (renderer[i+1]-'0')*100 + (renderer[i+2]-'0')*10 + (renderer[i+3]-'0');
				break;
			}

		isDepthClampSupported = isExtensionSupported("GL_ARB_depth_clamp");
	}
}

bool Workaround::needsUnfilteredShadowmaps()
{
	// legacy AMD (X300, X1650) doesn't work properly with GL_LINEAR on shadowmaps, it needs GL_NEAREST
	// HD 2400,3870,4850 is ok
	// we don't know legacy firegl numbers, therefore we treat all firegl as new, with pros/cons:
	//  + new firegl have shadows filtered
	//  - legacy firegl pointlight shadows contain wireframe cube
	init();
	return isRadeon && (modelNumber>=9500 || modelNumber<2199);
}

bool Workaround::needsOneSampleShadowmaps(const rr::RRLight& light)
{
	// legacy AMD (X300, X1650) render shadow acne in distant 1-sample shadows if near shadows use 4 or 8 samples.
	// increased bias did not help, forcing 1-sample for near shadows helps
	init();
	return light.type==rr::RRLight::DIRECTIONAL && isRadeon && (modelNumber>=9500 || modelNumber<2199);
}

void Workaround::needsIncreasedBias(float& slopeBias,float& fixedBias,const rr::RRLight& light)
{
	// single bias value thet works everywhere would create unnecessarily high bias on modern GPUs
	// therefore we adjust bias differently for different GPU families
	init();
	if (light.type==rr::RRLight::POINT && (isFire || isRadeon)) // fixes all radeons (with 24 or 32bit depth, not necessary for 16bit), firegl not tested
		fixedBias *= 15;
	if (isQuadro || (isGeforce && (modelNumber>=5000 && modelNumber<=7999))) // fixes 7100
		fixedBias *= 256;
}

bool Workaround::needsDDI4x4()
{
	// known to driver-crash or otherwise fail with 8x8: X300, FireGL 3200
	// known to work with 8x8: X1600, X2400, HD3780, HD4850, HD5870, 6150, 7100, 8800
	// so our automatic pick is:
	//   4x4 for radeon 9500..9999, x100..1299, FireGL
	//   8x8 for others
	init();
	if ( isRadeon && (modelNumber>=9500 || modelNumber<=1299) ) return true; // fixes X300
	//if ( isGeforce && (modelNumber>=5000 && modelNumber<=5999) ) detectionQuality = DDI_4X4; // never tested
	if ( isFire ) return true; // fixes FireGL 3200
	return false;
}

unsigned Workaround::needsReducedQualityPenumbra(unsigned SHADOW_MAPS)
{
	init();
	unsigned instancesPerPassOrig = SHADOW_MAPS;
	// workaround for Catalyst bug (driver crashes or outputs garbage on long shader)
	if ( isRadeon || isFire )
	{
		if ( (modelNumber>=1300 && modelNumber<=1999) )
		{
			// X1950 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
			// X1650 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
			SHADOW_MAPS = RR_MIN(SHADOW_MAPS,4);
		}
		else
		if ( (modelNumber>=9500 || modelNumber<=1299) )
		{
			// X300 in Lightsmark2008 5->2or1, otherwise reads garbage from last shadowmap
			SHADOW_MAPS = RR_MIN(SHADOW_MAPS,1);
		}
	}
	// 2 is ugly, prefer 1
	if (SHADOW_MAPS==2) SHADOW_MAPS--;
	rr::RRReporter::report(rr::INF2,"Penumbra quality: %d/%d on %s.\n",SHADOW_MAPS,instancesPerPassOrig,renderer);
	return SHADOW_MAPS;
}

bool Workaround::supportsDepthClamp()
{
	// if true, volume of detailed shadows is reduced in near/far direction,
	// but visible shadow bias is smaller in this volume
	//
	// true: all geforces, radeon 5870
	// false:
	// unknown: older radeons
	init();
	return isDepthClampSupported;
}

}; // namespace
