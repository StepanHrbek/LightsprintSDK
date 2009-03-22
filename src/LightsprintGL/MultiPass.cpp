// --------------------------------------------------------------------------
// MultiPass splits complex rendering setup into simpler ones doable per pass.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "MultiPass.h"
#include "PreserveState.h"

namespace rr_gl
{

MultiPass::MultiPass(const RealtimeLights* _lights, UberProgramSetup _mainUberProgramSetup, UberProgram* _uberProgram, const rr::RRVec4* _brightness, float _gamma)
{
	// inputs
	lights = _lights;
	mainUberProgramSetup = _mainUberProgramSetup;
	uberProgram = _uberProgram;
	brightness = _brightness;
	gamma = _gamma;
	numLights = lights?lights->size():0;
	separatedZPass = (mainUberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && (mainUberProgramSetup.MATERIAL_TRANSPARENCY_CONST || mainUberProgramSetup.MATERIAL_TRANSPARENCY_MAP || mainUberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA) && !mainUberProgramSetup.FORCE_2D_POSITION)?1:0;
	separatedAmbientPass = (!numLights)?1:0;
	lightIndex = -separatedZPass-separatedAmbientPass;
}

Program* MultiPass::getNextPass(UberProgramSetup& outUberProgramSetup, RendererOfRRObject::RenderedChannels& outRenderedChannels, RealtimeLight*& outLight)
{
	return getPass(lightIndex++,outUberProgramSetup,outRenderedChannels,outLight);
}

// returns true and all outXxx are set, do render
// or returns false and outXxx stay unchanged, rendering is done
Program* MultiPass::getPass(int _lightIndex, UberProgramSetup& _outUberProgramSetup, RendererOfRRObject::RenderedChannels& _outRenderedChannels, RealtimeLight*& _outLight)
{
	UberProgramSetup uberProgramSetup = mainUberProgramSetup;
	RealtimeLight* light;

	if (separatedZPass && _lightIndex==-separatedZPass-separatedAmbientPass)
	{
		// before Z pass: write z only
		glColorMask(0,0,0,0);
		//glDepthMask(GL_TRUE); does not work
	}
	if (separatedZPass && _lightIndex==-separatedZPass-separatedAmbientPass+1)
	{
		// after Z pass: write color only
		glColorMask(1,1,1,1);
		//glDepthMask(0);
	}

	if (separatedZPass && _lightIndex==-separatedZPass-separatedAmbientPass)
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
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = 0;
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = 0;
		uberProgramSetup.LIGHT_INDIRECT_auto = 0;
		uberProgramSetup.MATERIAL_DIFFUSE = 0;
		uberProgramSetup.MATERIAL_SPECULAR = 0;
		uberProgramSetup.MATERIAL_EMISSIVE_CONST = 0;
		uberProgramSetup.MATERIAL_EMISSIVE_VCOLOR = 0;
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
	if (_lightIndex<(int)numLights)
	{
		// adjust program for n-th light (0-th includes indirect, others have it disabled)
		light = (*lights)[_lightIndex];
		RR_ASSERT(light);
		uberProgramSetup.SHADOW_MAPS = mainUberProgramSetup.SHADOW_MAPS ? light->getNumShadowmaps() : 0;
		uberProgramSetup.SHADOW_SAMPLES = light->getNumShadowSamples(0);
		uberProgramSetup.SHADOW_PENUMBRA = light->getRRLight().type!=rr::RRLight::POINT;
		uberProgramSetup.SHADOW_CASCADE = light->getParent()->orthogonal && light->getNumShadowmaps()>1;
		if (uberProgramSetup.SHADOW_SAMPLES && uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE) uberProgramSetup.SHADOW_SAMPLES = 1; // reduce shadow quality for moving objects, for DDI
		if (uberProgramSetup.SHADOW_SAMPLES && uberProgramSetup.SHADOW_CASCADE) uberProgramSetup.SHADOW_SAMPLES = 4; // increase shadow quality for cascade (even moving objects)
		if (uberProgramSetup.SHADOW_SAMPLES && uberProgramSetup.FORCE_2D_POSITION) uberProgramSetup.SHADOW_SAMPLES = 1; // reduce shadow quality for DDI (even cascade)
		uberProgramSetup.LIGHT_DIRECT_COLOR = mainUberProgramSetup.LIGHT_DIRECT_COLOR && light->getRRLight().color!=rr::RRVec3(1);
		uberProgramSetup.LIGHT_DIRECT_MAP = mainUberProgramSetup.LIGHT_DIRECT_MAP && light->getProjectedTexture();
		if (uberProgramSetup.LIGHT_DIRECT_MAP && !uberProgramSetup.SHADOW_MAPS)
		{
			// early correction (affects LIGHT_DIRECT_ATT_SPOT, unlike late correction)
			uberProgramSetup.LIGHT_DIRECT_MAP = false;
			LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Projecting texture supported only for shadow casting lights.\n"));
		}
		uberProgramSetup.LIGHT_DIRECTIONAL = light->getParent()->orthogonal;
		uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = mainUberProgramSetup.LIGHT_DIRECT_ATT_SPOT && light->getRRLight().type==rr::RRLight::SPOT && !uberProgramSetup.LIGHT_DIRECT_MAP; // disables spot attenuation in presence of projected texture
		uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL = light->getRRLight().distanceAttenuationType==rr::RRLight::PHYSICAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL = light->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL;
		uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = light->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL;
		if (_lightIndex>-separatedAmbientPass)
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
	uberProgramSetup.validate(); // might be useful (however no problems detected without it)
	Program* program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma);
	if (!program)
	{
		// Radeon X300 fails to run some complex shaders in one pass
		// disabling transparency map saves SceneViewer sample
		if (uberProgramSetup.MATERIAL_TRANSPARENCY_MAP)
		{
			uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = 0;
			uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = 1;
			uberProgramSetup.validate(); // might be useful (however no problems detected without it)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma);
			if (program) LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with one feature disabled (transparency map).\n"));
		}
		// disabling specular reflection saves SceneViewer sample (helps GF6150)
		if (!program && uberProgramSetup.MATERIAL_SPECULAR)
		{
			uberProgramSetup.MATERIAL_SPECULAR = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = 0;
			uberProgramSetup.validate(); // is useful (zeroes MATERIAL_SPECULAR_CONST, might do more)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma);
			if (program) LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with some features disabled.\n"));
		}
		// disabling light detail map saves Lightsmark (helps GF6150)
		if (!program && uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP)
		{
			uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = 0;
			uberProgramSetup.validate(); // might be useful (however no problems detected without it)
			program = uberProgramSetup.useProgram(uberProgram,light,0,brightness,gamma);
			if (program) LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok with LDM disabled.\n"));
		}
		// splitting shader in two saves MovingSun sample (this might be important also for GF5/6/7)
		if (!program && (uberProgramSetup.LIGHT_INDIRECT_VCOLOR2 || uberProgramSetup.LIGHT_INDIRECT_MAP2) && _lightIndex==0 && !separatedAmbientPass)
		{
			separatedAmbientPass = 1;
			lightIndex = -1;
			program = getNextPass(_outUberProgramSetup,_outRenderedChannels,_outLight);
			if (program) LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Requested shader too big, ok when split in two passes.\n"));
		}
		if (!program)
		{
			LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program.\n"));
			return NULL;
		}
	}

	bool hasTransparency = uberProgramSetup.MATERIAL_TRANSPARENCY_CONST || uberProgramSetup.MATERIAL_TRANSPARENCY_MAP || uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA;

	if (_lightIndex==-separatedAmbientPass+1)
	{
		// additional passes add to framebuffer
		// 1. set blend mode
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		// 2. disable any further changes of blendmode
		hasTransparency = false;
	}

	RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.NORMALS = uberProgramSetup.LIGHT_DIRECT || uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR || uberProgramSetup.POSTPROCESS_NORMALS;
	renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_VCOLOR = uberProgramSetup.LIGHT_INDIRECT_VCOLOR;
	renderedChannels.LIGHT_INDIRECT_VCOLOR2 = uberProgramSetup.LIGHT_INDIRECT_VCOLOR2;
	renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.LIGHT_INDIRECT_MAP2 = uberProgramSetup.LIGHT_INDIRECT_MAP2;
	renderedChannels.LIGHT_INDIRECT_DETAIL_MAP = uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP;
	renderedChannels.MATERIAL_DIFFUSE_CONST = uberProgramSetup.MATERIAL_DIFFUSE_CONST;
	renderedChannels.MATERIAL_DIFFUSE_VCOLOR = uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.MATERIAL_SPECULAR_CONST = uberProgramSetup.MATERIAL_SPECULAR_CONST;
	renderedChannels.MATERIAL_EMISSIVE_CONST = uberProgramSetup.MATERIAL_EMISSIVE_CONST;
	renderedChannels.MATERIAL_EMISSIVE_VCOLOR = uberProgramSetup.MATERIAL_EMISSIVE_VCOLOR;
	renderedChannels.MATERIAL_EMISSIVE_MAP = uberProgramSetup.MATERIAL_EMISSIVE_MAP;
	renderedChannels.MATERIAL_TRANSPARENCY_CONST = uberProgramSetup.MATERIAL_TRANSPARENCY_CONST;
	renderedChannels.MATERIAL_TRANSPARENCY_MAP = uberProgramSetup.MATERIAL_TRANSPARENCY_MAP;
	renderedChannels.MATERIAL_TRANSPARENCY_BLENDING = hasTransparency && uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND;
	renderedChannels.MATERIAL_TRANSPARENCY_KEYING = hasTransparency && !uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND;
	renderedChannels.MATERIAL_CULLING = uberProgramSetup.MATERIAL_CULLING;
	renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;

	_outUberProgramSetup = uberProgramSetup;
	_outRenderedChannels = renderedChannels;
	_outLight = light;
	return program;
}

}; // namespace
