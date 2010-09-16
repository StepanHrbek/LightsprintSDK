// --------------------------------------------------------------------------
// MultiPass splits complex rendering setup into simpler ones doable per pass.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "MultiPass.h"
#include "PreserveState.h"

namespace rr_gl
{

MultiPass::MultiPass(const RealtimeLights* _lights, UberProgramSetup _mainUberProgramSetup, UberProgram* _uberProgram, const rr::RRVec4* _brightness, float _gamma, float* _clipPlanes)
{
	// inputs
	lights = _lights;
	unsigned numLights = 0; // count only non-NULL enabled lights
	if (lights)
		for (unsigned i=0;i<lights->size();i++)
			if ((*lights)[i] && (*lights)[i]->getRRLight().enabled)
				numLights++;
	mainUberProgramSetup = _mainUberProgramSetup;
	uberProgram = _uberProgram;
	brightness = _brightness;
	gamma = _gamma;
	clipPlanes = _clipPlanes;
	separatedZPass = (mainUberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && (mainUberProgramSetup.MATERIAL_TRANSPARENCY_CONST || mainUberProgramSetup.MATERIAL_TRANSPARENCY_MAP || mainUberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA) && !mainUberProgramSetup.FORCE_2D_POSITION)?1:0;
	separatedAmbientPass = (!numLights)?1:0;
	lightIndex = -separatedZPass-separatedAmbientPass;
	colorPassIndex = -separatedZPass;
}

Program* MultiPass::getNextPass(UberProgramSetup& _outUberProgramSetup, RealtimeLight*& _outLight)
{
	// skip NULL and disabled lights
	if (lights && lightIndex>=0)
		while (lightIndex<(int)lights->size() && (!(*lights)[lightIndex] || !(*lights)[lightIndex]->getRRLight().enabled))
			lightIndex++;

	Program* result = getPass(lightIndex,_outUberProgramSetup,_outLight);

	lightIndex++;
	colorPassIndex++;

	return result;
}

// returns true and all outXxx are set, do render
// or returns false and outXxx stay unchanged, rendering is done
Program* MultiPass::getPass(int _lightIndex, UberProgramSetup& _outUberProgramSetup, RealtimeLight*& _outLight)
{
	UberProgramSetup uberProgramSetup = mainUberProgramSetup;
	RealtimeLight* light;

	if (separatedZPass && colorPassIndex==-1)
	{
		// before Z pass: write z only
		glColorMask(0,0,0,0);
		//glDepthMask(GL_TRUE); does not work
	}
	if (separatedZPass && colorPassIndex==0)
	{
		// after Z pass: write color only
		glColorMask(1,1,1,1);
		//glDepthMask(0);
	}

	if (separatedZPass && colorPassIndex==-1)
	{
		light = NULL;
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
		uberProgramSetup.LIGHT_DIRECT = 0;
		uberProgramSetup.LIGHT_DIRECT_COLOR = 0;
		uberProgramSetup.LIGHT_DIRECT_MAP = 0;
		uberProgramSetup.LIGHT_DIRECTIONAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = 0;
		uberProgramSetup.LIGHT_INDIRECT_CONST = 0;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 0;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = 0;
		uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
		uberProgramSetup.LIGHT_INDIRECT_MAP2 = 0;
		uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = 0;
		uberProgramSetup.LIGHT_INDIRECT_auto = 0;
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = 0;
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = 0;
		uberProgramSetup.MATERIAL_DIFFUSE = 0;
		uberProgramSetup.MATERIAL_SPECULAR = 0;
		uberProgramSetup.MATERIAL_EMISSIVE_CONST = 0;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = 0;
	}
	else
	if (separatedAmbientPass && _lightIndex==-separatedAmbientPass)
	{
		// adjust program for render without lights
		//uberProgramSetup.setLightDirect(NULL,NULL);
		light = NULL;
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
		uberProgramSetup.LIGHT_DIRECT = 0;
		uberProgramSetup.LIGHT_DIRECT_COLOR = 0;
		uberProgramSetup.LIGHT_DIRECT_MAP = 0;
		uberProgramSetup.LIGHT_DIRECTIONAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = 0;
		//if (uberProgramSetup.LIGHT_INDIRECT_VCOLOR) printf(" %d: indirect\n",_lightIndex); else printf(" %d: nothing\n",_lightIndex);
	}
	else
	if (_lightIndex>=0 && _lightIndex<(int)(lights?lights->size():0))
	{
		// adjust program for n-th light (0-th includes indirect, others have it disabled)
		light = (*lights)[_lightIndex];
		RR_ASSERT(light);
		RR_ASSERT(light->getRRLight().enabled);
		uberProgramSetup.SHADOW_MAPS = mainUberProgramSetup.SHADOW_MAPS ? light->getNumShadowmaps() : 0;
		uberProgramSetup.SHADOW_SAMPLES = mainUberProgramSetup.SHADOW_MAPS ? light->getNumShadowSamples() : 0;
		uberProgramSetup.SHADOW_PENUMBRA = mainUberProgramSetup.SHADOW_MAPS && light->getRRLight().type==rr::RRLight::SPOT;
		uberProgramSetup.SHADOW_CASCADE = mainUberProgramSetup.SHADOW_MAPS && light->getParent()->orthogonal && light->getNumShadowmaps()>1;
		if (uberProgramSetup.SHADOW_SAMPLES && uberProgramSetup.FORCE_2D_POSITION) uberProgramSetup.SHADOW_SAMPLES = 1; // reduce shadow quality for DDI
		uberProgramSetup.LIGHT_DIRECT_COLOR = mainUberProgramSetup.LIGHT_DIRECT_COLOR && light->getRRLight().color!=rr::RRVec3(1);
		uberProgramSetup.LIGHT_DIRECT_MAP = mainUberProgramSetup.LIGHT_DIRECT_MAP && light->getProjectedTexture();
		if (uberProgramSetup.LIGHT_DIRECT_MAP && !uberProgramSetup.SHADOW_MAPS)
		{
			// early correction (affects LIGHT_DIRECT_ATT_SPOT, unlike late correction)
			uberProgramSetup.LIGHT_DIRECT_MAP = false;
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Projecting texture supported only for shadow casting lights.\n"));
		}
		uberProgramSetup.LIGHT_DIRECTIONAL = light->getParent()->orthogonal;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = mainUberProgramSetup.LIGHT_DIRECT_ATT_SPOT && light->getRRLight().type==rr::RRLight::SPOT && !uberProgramSetup.LIGHT_DIRECT_MAP; // disables spot attenuation in presence of projected texture
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = light->getRRLight().distanceAttenuationType==rr::RRLight::PHYSICAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = light->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = light->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL;

		uberProgramSetup.SHADOW_ONLY = light->shadowOnly;

		if (colorPassIndex>0)
		{
			// additional passes don't include indirect
			uberProgramSetup.LIGHT_INDIRECT_auto = 0;
			uberProgramSetup.LIGHT_INDIRECT_CONST = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP2 = 0;
			uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = 0;
			uberProgramSetup.MATERIAL_EMISSIVE_CONST = 0;
			uberProgramSetup.MATERIAL_EMISSIVE_MAP = 0;
			//printf(" %d: direct\n",_lightIndex);
		}
		//else printf(" %d: direct+indirect\n",_lightIndex);
	}
	else
	{
		// _lightIndex out of range, all passes done
		return NULL;
	}
	uberProgramSetup.validate(); // might be useful (however no problems detected without it)
	Program* program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma,clipPlanes);
	if (!program)
	{
		// try disable transparency map
		if (uberProgramSetup.MATERIAL_TRANSPARENCY_MAP)
		{
			uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = 0;
			uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = 1;
			uberProgramSetup.validate(); // might be useful (however no problems detected without it)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma,clipPlanes);
			if (program) RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with transparency map disabled.\n"));
		}
		// try disable specular reflection (saves SceneViewer+GF6150)
		if (!program && uberProgramSetup.MATERIAL_SPECULAR)
		{
			uberProgramSetup.MATERIAL_SPECULAR = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = 0;
			uberProgramSetup.validate(); // is useful (zeroes MATERIAL_SPECULAR_CONST, might do more)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma,clipPlanes);
			if (program) RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with specular disabled.\n"));
		}
		// try disable LDM (saves Lightsmark+GF6150)
		if (!program && uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP)
		{
			uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = 0;
			uberProgramSetup.validate(); // might be useful (however no problems detected without it)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma,clipPlanes);
			if (program) RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with LDM disabled.\n"));
		}
		// try disable emissive map
		if (uberProgramSetup.MATERIAL_EMISSIVE_MAP)
		{
			uberProgramSetup.MATERIAL_EMISSIVE_MAP = 0;
			uberProgramSetup.MATERIAL_EMISSIVE_CONST = 1;
			uberProgramSetup.validate(); // might be useful (however no problems detected without it)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma,clipPlanes);
			if (program) RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with emissive map disabled.\n"));
		}
		// try disable diffuse map (saves SceneViewer+X300,X1650)
		if (uberProgramSetup.MATERIAL_DIFFUSE_MAP)
		{
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = 0;
			uberProgramSetup.MATERIAL_DIFFUSE_CONST = 1;
			uberProgramSetup.validate(); // might be useful (however no problems detected without it)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma,clipPlanes);
			if (program) RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with diffuse map disabled.\n"));
		}
		// try split blending shader in two
		if (!program && (uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 || uberProgramSetup.LIGHT_INDIRECT_MAP2) && _lightIndex==0 && !separatedAmbientPass)
		{
			separatedAmbientPass = 1;
			lightIndex = -1;
			program = getNextPass(_outUberProgramSetup,_outLight);
			if (program) RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok when split in two passes.\n"));
		}
		if (!program)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program.\n"));
			return NULL;
		}
	}

	if (colorPassIndex==1)
	{
		// additional passes add to framebuffer
		// 0. if already blending, there's no simple mode for multipass blending, better skip additional passes
		if (uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND)
			return NULL;
		// 1. set blend mode
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		// 2. disable changes of blendmode
		//uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = false;
	}

	_outUberProgramSetup = uberProgramSetup;
	_outLight = light;
	return program;
}

}; // namespace
