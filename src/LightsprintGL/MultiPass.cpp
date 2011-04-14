// --------------------------------------------------------------------------
// MultiPass splits complex rendering setup into simpler ones doable per pass.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "MultiPass.h"
#include "PreserveState.h"

namespace rr_gl
{

MultiPass::MultiPass(const RealtimeLights* _lights, const rr::RRLight* _renderingFromThisLight, UberProgramSetup _mainUberProgramSetup, UberProgram* _uberProgram, float* _clipPlanes, bool _srgbCorrect, const rr::RRVec4* _brightness, float _gamma)
{
	// inputs
	lights = _lights;
	unsigned numLights = 0; // count only non-NULL enabled lights
	if (lights)
		for (unsigned i=0;i<lights->size();i++)
			if ((*lights)[i] && (*lights)[i]->getRRLight().enabled)
				numLights++;
	depthMask = 1;
	colorMask = (!_renderingFromThisLight || _mainUberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB)?1:0;
	mainUberProgramSetup = _mainUberProgramSetup;
	uberProgram = _uberProgram;
	clipPlanes = _clipPlanes;
	brightness = _brightness;
	gamma = _gamma;

	separatedZPass = (mainUberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && (mainUberProgramSetup.MATERIAL_TRANSPARENCY_CONST || mainUberProgramSetup.MATERIAL_TRANSPARENCY_MAP || mainUberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA) && !mainUberProgramSetup.FORCE_2D_POSITION)?1:0; // needs MATERIAL_TRANSPARENCY_BLEND, not triggered by rendering to SM

	// GL3.3 ARB_blend_func_extended can do it without separated pass, we do extra pass to be compatible with GL2
	separatedMultiplyPass = (separatedZPass && mainUberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB)?1:0; // needs MATERIAL_TRANSPARENCY_BLEND, not triggered by rendering to SM

	separatedAmbientPass = _srgbCorrect
		// separate ambient from direct light
		? (!numLights || mainUberProgramSetup.LIGHT_INDIRECT_CONST || mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR || mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR2 || mainUberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL || mainUberProgramSetup.LIGHT_INDIRECT_MAP || mainUberProgramSetup.LIGHT_INDIRECT_MAP2 || mainUberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP || mainUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || mainUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR || mainUberProgramSetup.LIGHT_INDIRECT_auto)
		// do ambient together with first direct light
		: ((!numLights)?1:0);

	lightIndex = -separatedZPass-separatedMultiplyPass-separatedAmbientPass;
	colorPassIndex = -separatedZPass;
}

Program* MultiPass::getNextPass(UberProgramSetup& _outUberProgramSetup, RealtimeLight*& _outLight)
{
	// skip NULL and disabled lights
	if (lights && lightIndex>=0)
		while (lightIndex<(int)lights->size() && (!(*lights)[lightIndex] || !(*lights)[lightIndex]->getRRLight().enabled))
			lightIndex++;

	// return NULL when done
	if (lightIndex>=(int)(lights?lights->size():0))
		return NULL;

	Program* result = getPass(lightIndex,_outUberProgramSetup,_outLight);

	lightIndex++;
	colorPassIndex++;

	return result;
}

// returns program and all outXxx are set, do render
// or returns NULL and outXxx stay unchanged, rendering is done
Program* MultiPass::getPass(int _lightIndex, UberProgramSetup& _outUberProgramSetup, RealtimeLight*& _outLight)
{
	UberProgramSetup uberProgramSetup = mainUberProgramSetup;
	RealtimeLight* light;

	if (colorPassIndex==-1)
	{
		// before Z pass
		glDepthMask(1);
		glColorMask(0,0,0,0);
	}
	else
	if (colorPassIndex==0)
	{
		// before first color pass
		glDepthMask((separatedZPass||mainUberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB)?0:1); // MATERIAL_TRANSPARENCY_TO_RGB is when rendering blended material into shadowmap (z write must be disabled)
		glColorMask(colorMask,colorMask,colorMask,colorMask);

		if (separatedMultiplyPass)
		{
			// before RGB blending
			glBlendFunc(GL_ZERO,GL_SRC_COLOR);
			glEnable(GL_BLEND);
		}
		else
		if (mainUberProgramSetup.MATERIAL_TRANSPARENCY_BLEND)
		{
			// before alpha blending
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
		}
		else
		{
			// before non-blending
			glDisable(GL_BLEND);
		}
	}
	else
	if (colorPassIndex==1)
	{
		// before second color pass
		glDepthMask(0);
		glBlendFunc(GL_ONE,GL_ONE);
		glEnable(GL_BLEND);
	}

	if (colorPassIndex==-1)
	{
		// before Z pass
		light = NULL;
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
		uberProgramSetup.SHADOW_COLOR = 0;
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
		uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = 0;
	}
	else
	if (colorPassIndex==0 && separatedMultiplyPass)
	{
		// before multiply pass
		// disable dif/spec/emi, they write to RGB too
		light = NULL;
		uberProgramSetup.MATERIAL_DIFFUSE = 0;
		uberProgramSetup.MATERIAL_SPECULAR = 0;
		uberProgramSetup.MATERIAL_EMISSIVE_CONST = 0;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = 0;
	}
	else
	if (separatedAmbientPass && _lightIndex==-separatedAmbientPass)
	{
		// before ambient pass
		// adjust program for render without lights
		//uberProgramSetup.setLightDirect(NULL,NULL);
		light = NULL;
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
		uberProgramSetup.SHADOW_COLOR = 0;
		uberProgramSetup.LIGHT_DIRECT = 0;
		uberProgramSetup.LIGHT_DIRECT_COLOR = 0;
		uberProgramSetup.LIGHT_DIRECT_MAP = 0;
		uberProgramSetup.LIGHT_DIRECTIONAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = 0;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = 0;
		//if (uberProgramSetup.LIGHT_INDIRECT_VCOLOR) printf(" %d: indirect\n",_lightIndex); else printf(" %d: nothing\n",_lightIndex);
		if (mainUberProgramSetup.MATERIAL_TRANSPARENCY_BLEND) // final render: stop rgb blend
			uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = 0; // render to shadowmap: keep writing to rgb
	}
	else
	if (_lightIndex>=0 && _lightIndex<(int)(lights?lights->size():0))
	{
		// before n-th light
		// adjust program for n-th light (0-th includes indirect, others have it disabled)
		light = (*lights)[_lightIndex];
		RR_ASSERT(light);
		RR_ASSERT(light->getRRLight().enabled);
		uberProgramSetup.SHADOW_MAPS = mainUberProgramSetup.SHADOW_MAPS ? light->getNumShadowmaps() : 0;
		uberProgramSetup.SHADOW_SAMPLES = mainUberProgramSetup.SHADOW_MAPS ? light->getNumShadowSamples() : 0;
		uberProgramSetup.SHADOW_COLOR = mainUberProgramSetup.SHADOW_MAPS && light->shadowTransparencyActual==RealtimeLight::RGB_SHADOWS;
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

		if (colorPassIndex>separatedMultiplyPass)
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

		uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB = 0;
	}
	else
	{
		// _lightIndex out of range, all passes done
		RR_ASSERT(0); // we should never get here, 'done' is catched earlier
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

	_outUberProgramSetup = uberProgramSetup;
	_outLight = light;
	return program;
}

}; // namespace
