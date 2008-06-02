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
	numLights = lights?lights->size():0;
	separatedAmbientPass = (!numLights||honourExpensiveLightingShadowingFlags)?1:0;
	lightIndex = -separatedAmbientPass;
}

Program* MultiPass::getNextPass(UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, const RealtimeLight*& outLight)
{
	return getPass(lightIndex++,outUberProgramSetup,outRenderedChannels,outLight);
}

// returns true and all outXxx are set, do render
// or returns false and outXxx stay unchanged, rendering is done
Program* MultiPass::getPass(int _lightIndex, UberProgramSetup& _outUberProgramSetup, RendererOfRRObject::RenderedChannels& _outRenderedChannels, const RealtimeLight*& _outLight) const
{
	UberProgramSetup uberProgramSetup = mainUberProgramSetup;
	const RealtimeLight* light;
	if(_lightIndex==-1)
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
		//if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR) printf(" %d: indirect\n",_lightIndex); else printf(" %d: nothing\n",_lightIndex);
	}
	else
	if(_lightIndex<(int)numLights)
	{
		// adjust program for n-th light (0-th includes indirect, others have it disabled)
		light = (*lights)[_lightIndex];
		RR_ASSERT(light);
		uberProgramSetup.SHADOW_MAPS = mainUberProgramSetup.SHADOW_MAPS ? light->getNumInstances() : 0;
		uberProgramSetup.SHADOW_PENUMBRA = light->areaType!=RealtimeLight::POINT;
		uberProgramSetup.SHADOW_CASCADE = light->getParent()->orthogonal && light->getNumInstances()>1;
		if(uberProgramSetup.SHADOW_CASCADE) uberProgramSetup.SHADOW_SAMPLES = 4; // override user setting by optimal setting
		uberProgramSetup.LIGHT_DIRECT_COLOR = mainUberProgramSetup.LIGHT_DIRECT_COLOR && light->origin && light->origin->color!=rr::RRVec3(1);
		uberProgramSetup.LIGHT_DIRECT_MAP = mainUberProgramSetup.LIGHT_DIRECT_MAP && uberProgramSetup.SHADOW_MAPS && light->areaType!=RealtimeLight::POINT && light->lightDirectMap;
		uberProgramSetup.LIGHT_DIRECTIONAL = light->getParent()->orthogonal;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = mainUberProgramSetup.LIGHT_DIRECT_ATT_SPOT && light->origin && light->origin->type==rr::RRLight::SPOT;
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::PHYSICAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = light->origin && light->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		if(_lightIndex>-separatedAmbientPass)
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
			//printf(" %d: direct\n",_lightIndex);
		}
		//else printf(" %d: direct+indirect\n",_lightIndex);
	}
	else
	{
		// _lightIndex out of range, all passes done
		return NULL;
	}
	Program* program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma);
	if(!program)
	{
		// Radeon X300 and GF5/6/7 fail to run some complex shaders in one pass
		// simply disabling transparency map saves the day
		if(uberProgramSetup.MATERIAL_TRANSPARENCY_MAP)
		{
			uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = 0;
			uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = 1;
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma);
			if(program) LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, one feature disabled (transparency map).\n"));
		}
		if(!program)
		{
			LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program.\n"));
			return NULL;
		}
	}
	if(_lightIndex==-separatedAmbientPass+1)
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
	renderedChannels.MATERIAL_DIFFUSE_CONST = uberProgramSetup.MATERIAL_DIFFUSE_CONST;
	renderedChannels.MATERIAL_DIFFUSE_VCOLOR = uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.MATERIAL_EMISSIVE_CONST = uberProgramSetup.MATERIAL_EMISSIVE_CONST;
	renderedChannels.MATERIAL_EMISSIVE_VCOLOR = uberProgramSetup.MATERIAL_EMISSIVE_VCOLOR;
	renderedChannels.MATERIAL_EMISSIVE_MAP = uberProgramSetup.MATERIAL_EMISSIVE_MAP;
	renderedChannels.MATERIAL_TRANSPARENCY_CONST = uberProgramSetup.MATERIAL_TRANSPARENCY_CONST;
	renderedChannels.MATERIAL_TRANSPARENCY_MAP = uberProgramSetup.MATERIAL_TRANSPARENCY_MAP;
	renderedChannels.MATERIAL_CULLING = uberProgramSetup.MATERIAL_CULLING;
	renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;

	_outUberProgramSetup = uberProgramSetup;
	_outRenderedChannels = renderedChannels;
	_outLight = light;
	return program;
}

}; // namespace
