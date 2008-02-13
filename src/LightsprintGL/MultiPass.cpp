// --------------------------------------------------------------------------
// MultiPass splits complex rendering setup into simpler ones doable per pass.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "MultiPass.h"
#include "PreserveState.h"

namespace rr_gl
{

MultiPass::MultiPass(const RealtimeLights* _lights, UberProgramSetup _mainUberProgramSetup, UberProgram* _uberProgram, const rr::RRVec4* _brightness, float _gamma, bool _honourExpensiveLightingShadowingFlags)
{
	// inputs
	lights = _lights;
	mainUberProgramSetup = _mainUberProgramSetup;
	uberProgram = _uberProgram;
	brightness = _brightness;
	gamma = _gamma;
	honourExpensiveLightingShadowingFlags = _honourExpensiveLightingShadowingFlags;
	// intermediates
	//  don't put direct+indirect+emissive in one pass, prerender indirect+emissive in separated ambient pass
	//  (direct pointlight(6 spots)+indirect vcolor+emissive vcolor nezvladnou v jednom passu GF5/6/7 a asi ani Radeony)
	numLights = lights?lights->size():0;
	separatedAmbientPass = (!numLights||honourExpensiveLightingShadowingFlags||mainUberProgramSetup.MATERIAL_EMISSIVE_VCOLOR)?1:0;
	lightIndex = -separatedAmbientPass;
}

Program* MultiPass::getNextPass(UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, const RealtimeLight*& outLight)
{
	return getPass(lightIndex++,outUberProgramSetup,outRenderedChannels,outLight);
}

// returns true and all outXxx are set, do render
// or returns false and outXxx stay unchanged, rendering is done
Program* MultiPass::getPass(int lightIndex, UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, const RealtimeLight*& outLight) const
{
	UberProgramSetup uberProgramSetup = mainUberProgramSetup;
	const RealtimeLight* light;
	if(lightIndex==-1)
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
		//if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR) printf(" %d: indirect\n",lightIndex); else printf(" %d: nothing\n",lightIndex);
	}
	else
	if(lightIndex<(int)numLights)
	{
		// adjust program for n-th light (0-th includes indirect, others have it disabled)
		light = (*lights)[lightIndex];
		RR_ASSERT(light);
		uberProgramSetup.SHADOW_MAPS = mainUberProgramSetup.SHADOW_MAPS ? light->getNumInstances() : 0;
		uberProgramSetup.SHADOW_PENUMBRA = light->areaType!=RealtimeLight::POINT;
		uberProgramSetup.LIGHT_DIRECT_COLOR = mainUberProgramSetup.LIGHT_DIRECT_COLOR && light->origin && light->origin->color!=rr::RRVec3(1);
		uberProgramSetup.LIGHT_DIRECT_MAP = mainUberProgramSetup.LIGHT_DIRECT_MAP && uberProgramSetup.SHADOW_MAPS && light->areaType!=RealtimeLight::POINT && light->lightDirectMap;
		uberProgramSetup.LIGHT_DIRECTIONAL = light->getParent()->orthogonal;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = mainUberProgramSetup.LIGHT_DIRECT_ATT_SPOT && light->origin && light->origin->type==rr::RRLight::SPOT;
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::PHYSICAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		if(lightIndex>-separatedAmbientPass)
		{
			// additional passes don't include indirect
			uberProgramSetup.LIGHT_INDIRECT_auto = 0;
			uberProgramSetup.LIGHT_INDIRECT_CONST = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP2 = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = 0;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = 0;
			uberProgramSetup.MATERIAL_EMISSIVE_CONST = 0;
			uberProgramSetup.MATERIAL_EMISSIVE_VCOLOR = 0;
			uberProgramSetup.MATERIAL_EMISSIVE_MAP = 0;
			//printf(" %d: direct\n",lightIndex);
		}
		//else printf(" %d: direct+indirect\n",lightIndex);
	}
	else
	{
		// lightIndex out of range, all passes done
		return NULL;
	}
	Program* program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma);
	if(!program)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program.\n");
		return NULL;
	}
	if(lightIndex==-separatedAmbientPass+1)
	{
		// additional passes add to framebuffer
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
	}

	RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.NORMALS = uberProgramSetup.LIGHT_DIRECT || uberProgramSetup.LIGHT_INDIRECT_ENV || uberProgramSetup.POSTPROCESS_NORMALS;
	renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_VCOLOR = uberProgramSetup.LIGHT_INDIRECT_VCOLOR;
	renderedChannels.LIGHT_INDIRECT_VCOLOR2 = uberProgramSetup.LIGHT_INDIRECT_VCOLOR2;
	renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.LIGHT_INDIRECT_MAP2 = uberProgramSetup.LIGHT_INDIRECT_MAP2;
	renderedChannels.LIGHT_INDIRECT_ENV = uberProgramSetup.LIGHT_INDIRECT_ENV;
	renderedChannels.MATERIAL_DIFFUSE_VCOLOR = uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.MATERIAL_EMISSIVE_VCOLOR = uberProgramSetup.MATERIAL_EMISSIVE_VCOLOR;
	renderedChannels.MATERIAL_EMISSIVE_MAP = uberProgramSetup.MATERIAL_EMISSIVE_MAP;
	renderedChannels.MATERIAL_CULLING = (uberProgramSetup.MATERIAL_DIFFUSE || uberProgramSetup.MATERIAL_SPECULAR) && !uberProgramSetup.FORCE_2D_POSITION; // should be enabled for all except for shadowmaps and force_2d
	renderedChannels.MATERIAL_BLENDING = lightIndex==-separatedAmbientPass; // material wishes are respected only in first pass, other passes use adding
	renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;

	outUberProgramSetup = uberProgramSetup;
	outRenderedChannels = renderedChannels;
	outLight = light;
	return program;
}

}; // namespace
