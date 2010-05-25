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

bool Workaround::needsUnfilteredShadowmaps()
{
	// AMD doesn't work properly with GL_LINEAR on shadowmaps, it needs GL_NEAREST
	static bool result = false;
	static bool inited = 0;
	if (!inited)
	{
		inited = 1;
		char* renderer = (char*)glGetString(GL_RENDERER);
		if (renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")||strstr(renderer,"FireGL")))
			result = true;
	}
	return result;
}

bool Workaround::needsIncreasedBias(rr::RRLight& light)
{
	if (light.type!=rr::RRLight::POINT)
		return false;
	static bool result = false;
	static bool inited = 0;
	if (!inited)
	{
		inited = 1;
		char* renderer = (char*)glGetString(GL_RENDERER);
		if (renderer && (strstr(renderer,"GeForce")||strstr(renderer,"Quadro")))
			result = true;
	}
	return result;
}

bool Workaround::needsDDI4x4()
{
	// known to driver-crash or otherwise fail with 8x8: X300, FireGL 3200
	// known to work with 8x8: X1600, X2400, HD3780, HD4850, HD5870, 6150, 7100, 8800
	// so our automatic pick is:
	//   4x4 for radeon 9500..9999, x100..1299, FireGL
	//   8x8 for others
	char* renderer = (char*)glGetString(GL_RENDERER);
	if (renderer)
	{
		// find 4digit number
		unsigned number = 0;
		#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
		for (unsigned i=0;renderer[i];i++)
			if (!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
			{
				number = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
				break;
			}
			else
			if (!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && !IS_DIGIT(renderer[i+4]))
			{
				number = (renderer[i+1]-'0')*100 + (renderer[i+2]-'0')*10 + (renderer[i+3]-'0');
				break;
			}

		if ( (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")) && (number<1300 || number>=9500) ) return true; // fixes X300
		//if ( (strstr(renderer,"GeForce")||strstr(renderer,"GEFORCE")) && (number>=5000 && number<6000) ) detectionQuality = DDI_4X4; // never tested
		if ( strstr(renderer,"FireGL") ) return true; // fixes FireGL 3200
	}
	return false;
}

unsigned Workaround::needsReducedQualityPenumbra(unsigned SHADOW_MAPS)
{
	unsigned instancesPerPassOrig = SHADOW_MAPS;
	char* renderer = (char*)glGetString(GL_RENDERER);
	if (renderer)
	{
		// find 4digit number
		unsigned number = 0;
		#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
		for (unsigned i=0;renderer[i];i++)
			if (!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
			{
				number = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
				break;
			}

		// workaround for Catalyst bug (driver crashes or outputs garbage on long shader)
		if ( strstr(renderer,"Radeon")||strstr(renderer,"RADEON")||strstr(renderer,"FireGL") )
		{
			if ( (number>=1300 && number<=1999) )
			{
				// X1950 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
				// X1650 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
				SHADOW_MAPS = RR_MIN(SHADOW_MAPS,4);
			}
			else
			if ( (number>=9500 || number<=1299) )
			{
				// X300 in Lightsmark2008 5->2or1, otherwise reads garbage from last shadowmap
				SHADOW_MAPS = RR_MIN(SHADOW_MAPS,1);
			}
		}
	}
	// 2 is ugly, prefer 1
	if (SHADOW_MAPS==2) SHADOW_MAPS--;
	rr::RRReporter::report(rr::INF2,"Penumbra quality: %d/%d on %s.\n",SHADOW_MAPS,instancesPerPassOrig,renderer?renderer:"");
	return SHADOW_MAPS;
}

}; // namespace
