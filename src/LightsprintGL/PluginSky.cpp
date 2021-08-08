// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Plugin that renders skybox/background.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginSky.h"
#include "Lightsprint/GL/PreserveState.h"
#include "Lightsprint/RRSolver.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeSky

class PluginRuntimeSky : public PluginRuntime
{

public:

	PluginRuntimeSky(const PluginCreateRuntimeParams& params)
	{
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsSky& pp = *dynamic_cast<const PluginParamsSky*>(&_pp);

		// solvers work with multipliers in linear space, convert them to srgb for rendering
		float skyMultiplier = pp.skyMultiplier;
		if (pp.solver->getColorSpace())
			pp.solver->getColorSpace()->fromLinear(skyMultiplier);

		rr::RRReal envAngleRad0 = 0;
		const rr::RRBuffer* env0 = pp.solver->getEnvironment(0,&envAngleRad0);
		if (_renderer.getTextureRenderer() && env0)
		{
			rr::RRReal envAngleRad1 = 0;
			const rr::RRBuffer* env1 = pp.solver->getEnvironment(1,&envAngleRad1);
			float blendFactor = pp.solver->getEnvironmentBlendFactor();
			Texture* texture0 = (env0->getWidth()>2)
				? getTexture(env0,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env0,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				;
			Texture* texture1 = env1 ? ( (env1->getWidth()>2)
				? getTexture(env1,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env1,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				) : nullptr;
			rr::RRVec4 brightness = _sp.brightness*skyMultiplier;
			_renderer.getTextureRenderer()->renderEnvironment(*_sp.camera,texture0,envAngleRad0,texture1,envAngleRad1,blendFactor,&brightness,_sp.gamma*(_sp.srgbCorrect?2.2f:1.f),true);
		}
	}

	virtual ~PluginRuntimeSky()
	{
	};
};


//////////////////////////////////////////////////////////////////////////////
//
// PluginParamsSky

PluginRuntime* PluginParamsSky::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeSky(params);
}

}; // namespace
