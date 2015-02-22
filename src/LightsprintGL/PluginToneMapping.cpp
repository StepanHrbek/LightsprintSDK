// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Tone mapping
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginToneMapping.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeToneMapping

class PluginRuntimeToneMapping : public PluginRuntime
{
	Texture* colorTexture;

public:

	PluginRuntimeToneMapping(const PluginCreateRuntimeParams& params)
	{
		colorTexture = nullptr;
	}

	virtual ~PluginRuntimeToneMapping()
	{
		if (colorTexture)
		{
			delete colorTexture->getBuffer();
			delete colorTexture;
		}
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsToneMapping& pp = *dynamic_cast<const PluginParamsToneMapping*>(&_pp);

		if (!_renderer.getTextureRenderer()) return;

		if (!colorTexture) colorTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,nullptr),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		colorTexture->bindTexture();
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3],0);
		_renderer.getTextureRenderer()->render2D(colorTexture,&pp.tp,0,0,1,1);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsToneMapping

PluginRuntime* PluginParamsToneMapping::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeToneMapping(params);
}

}; // namespace
