// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Workarounds for driver and GPU quirks.
// --------------------------------------------------------------------------

#include <cstring>
#include "Lightsprint/GL/Program.h" // includes <glew.h>
#include "Workaround.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Workaround

static const char* s_renderer = nullptr;
static bool        s_isGeforce = false;
static bool        s_isQuadro = false;
static bool        s_isRadeon = false;
static bool        s_isFire = false;
static bool        s_isIntel = false;
static unsigned    s_modelNumber = 0;
static bool        s_supportsLods = true;

#include <string.h>

static void init()
{
	static bool inited = 0;
	if (!inited)
	{
		inited = 1;

		s_renderer = (const char*)glGetString(GL_RENDERER);
		if (!s_renderer) s_renderer = "";

		s_isIntel = strstr(s_renderer,"Intel")!=nullptr;
		s_isGeforce = strstr(s_renderer,"GeForce")!=nullptr;
		s_isQuadro = strstr(s_renderer,"Quadro")!=nullptr;
		s_isRadeon = strstr(s_renderer,"Radeon") || strstr(s_renderer,"RADEON");
		s_isFire = strstr(s_renderer,"Fire")!=nullptr;

		// find 3-4digit model number
		#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
		for (unsigned i=0;s_renderer[i];i++)
			if (!IS_DIGIT(s_renderer[i]) && IS_DIGIT(s_renderer[i+1]) && IS_DIGIT(s_renderer[i+2]) && IS_DIGIT(s_renderer[i+3]) && IS_DIGIT(s_renderer[i+4]) && !IS_DIGIT(s_renderer[i+5]))
			{
				s_modelNumber = (s_renderer[i+1]-'0')*1000 + (s_renderer[i+2]-'0')*100 + (s_renderer[i+3]-'0')*10 + (s_renderer[i+4]-'0');
				break;
			}
			else
			if (!IS_DIGIT(s_renderer[i]) && IS_DIGIT(s_renderer[i+1]) && IS_DIGIT(s_renderer[i+2]) && IS_DIGIT(s_renderer[i+3]) && !IS_DIGIT(s_renderer[i+4]))
			{
				s_modelNumber = (s_renderer[i+1]-'0')*100 + (s_renderer[i+2]-'0')*10 + (s_renderer[i+3]-'0');
				break;
			}

		// try to compile texture2DLod()
		const GLchar* source[] = {"uniform samplerCube map; void main() { gl_FragColor = textureCubeLod(map,vec3(1.0,1.0,1.0),2.0); }\n",nullptr};
		GLuint handle = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(handle, 1, source, nullptr);
		glCompileShader(handle);
		GLint compiled;
		glGetShaderiv(handle, GL_COMPILE_STATUS, &compiled);
		if (!compiled)
		{
			s_supportsLods = false;
			rr::RRReporter::report(rr::WARN,"textureCubeLod() not available.\n");
		}
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
	return s_isRadeon && (s_modelNumber>=9500 || s_modelNumber<2199);
}

bool Workaround::needsOneSampleShadowmaps(const rr::RRLight& light)
{
	// legacy AMD (X300, X1650) render shadow acne in distant 1-sample shadows if near shadows use 4 or 8 samples.
	// increased bias did not help, forcing 1-sample for near shadows helps
	// near4-distant1-sample mode was removed, maybe this workaround can be removed too
	init();
	return light.type==rr::RRLight::DIRECTIONAL && s_isRadeon && (s_modelNumber>=9500 || s_modelNumber<2199);
}

void Workaround::needsIncreasedBias(float& slopeBias,float& fixedBias,const rr::RRLight& light)
{
}

bool Workaround::needsDDI4x4()
{
	// known to driver-crash or otherwise fail with 8x8: X300, FireGL 3200
	// known to work with 8x8: X1600, X2400, HD3780, HD4850, HD5870, 6150, 7100, 8800
	// so our automatic pick is:
	//   4x4 for radeon 9500..9999, x100..1299, FireGL
	//   8x8 for others
	init();
	if ( s_isRadeon && (s_modelNumber>=9500 || s_modelNumber<=1299) ) return true; // fixes X300
	//if ( isGeforce && (s_modelNumber>=5000 && s_modelNumber<=5999) ) detectionQuality = DDI_4X4; // never tested
	if ( s_isFire ) return true; // fixes FireGL 3200
	return false;
}

bool Workaround::needsHardPointShadows()
{
	init();
	// I vaguely remember X300 crashing with smooth point shadows few years ago, it needs retesting
	return s_isRadeon && (s_modelNumber>=9500 || s_modelNumber<=1299);
}

unsigned Workaround::needsReducedQualityPenumbra(unsigned SHADOW_MAPS)
{
	init();
	unsigned instancesPerPassOrig = SHADOW_MAPS;
	// workaround for Catalyst bug (driver crashes or outputs garbage on long shader)
	if ( s_isRadeon || s_isFire )
	{
		if ( (s_modelNumber>=1300 && s_modelNumber<=1999) )
		{
			// X1950 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
			// X1650 in Lightsmark2008 8->4, otherwise reads garbage from last shadowmap
			SHADOW_MAPS = RR_MIN(SHADOW_MAPS,4);
		}
		else
		if ( (s_modelNumber>=9500 || s_modelNumber<=1299) )
		{
			// X300 in Lightsmark2008 5->2or1, otherwise reads garbage from last shadowmap
			SHADOW_MAPS = RR_MIN(SHADOW_MAPS,1);
		}
	}
	// 2 is ugly, prefer 1
	if (SHADOW_MAPS==2) SHADOW_MAPS--;
	rr::RRReporter::report(rr::INF2,"Penumbra quality: %d/%d on %s.\n",SHADOW_MAPS,instancesPerPassOrig,s_renderer);
	return SHADOW_MAPS;
}

bool Workaround::needsNoLods()
{
	return !s_supportsLods;
}

bool Workaround::supportsDepthClamp()
{
	// if true, volume of detailed shadows is reduced in near/far direction,
	// but visible shadow bias is smaller in this volume
	//
	// true: all geforces, radeon 5870
	// false:
	// unknown: older radeons
#ifdef RR_GL_ES2
	return false;
#else
	return GLEW_ARB_depth_clamp!=GL_FALSE; // added in GL 3.2
#endif
}

bool Workaround::supportsSRGB()
{
#ifdef RR_GL_ES2
	return false;
#else
	return !s_isIntel // sRGB looks supported but is broken on Intel HD Graphics 4000 9.18.10.3257, probably others too
		&& (GLEW_EXT_framebuffer_sRGB || GLEW_ARB_framebuffer_sRGB || GLEW_VERSION_3_0) // added in GL 3.0
		&& (GLEW_EXT_texture_sRGB || GLEW_VERSION_2_1); // added in GL 2.1
#endif
}

}; // namespace
