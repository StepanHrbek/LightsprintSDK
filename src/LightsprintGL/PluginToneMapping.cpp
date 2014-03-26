// --------------------------------------------------------------------------
// Tone mapping adjustment
// Copyright (C) 2008-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginToneMapping.h"
//#include "Lightsprint/GL/PreserveState.h"
//#include <cstdlib>
#include "Lightsprint/GL/FBO.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeToneMapping

class PluginRuntimeToneMapping : public PluginRuntime
{
	Texture* bigTexture;
	Texture* smallTexture;

public:

	PluginRuntimeToneMapping(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
	{
		bigTexture = NULL;
		smallTexture = NULL;
	}

	virtual ~PluginRuntimeToneMapping()
	{
		if (smallTexture)
		{
			delete smallTexture->getBuffer();
			delete smallTexture;
		}
		if (bigTexture)
		{
			delete bigTexture->getBuffer();
			delete bigTexture;
		}
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsToneMapping& pp = *dynamic_cast<const PluginParamsToneMapping*>(&_pp);

		if (!_renderer.getTextureRenderer()) return;

		if (!bigTexture) bigTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,NULL),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		bigTexture->bindTexture();
		int bwidth = _sp.viewport[2];
		int bheight = _sp.viewport[3];
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,bwidth,bheight,0);

		const unsigned swidth = 32;
		const unsigned sheight = 32;
		unsigned char buf[swidth*sheight*3];
		unsigned histo[256];
		unsigned avg = 0;
		if (!smallTexture)
		{
			smallTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,swidth,sheight,1,rr::BF_RGB,true,NULL),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		}
		FBO oldFBOState = FBO::getState();
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallTexture);
		glViewport(0,0,swidth,sheight);
		_renderer.getTextureRenderer()->render2D(bigTexture,NULL,1,0,0,1,1);
		glReadPixels(0,0,swidth,sheight,GL_RGB,GL_UNSIGNED_BYTE,buf);
		oldFBOState.restore();
		glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);
		for (unsigned i=0;i<256;i++)
			histo[i] = 0;
		for (unsigned i=0;i<swidth*sheight*3;i+=3)
			histo[RR_MAX3(buf[i],buf[i+1],buf[i+2])]++;
		for (unsigned i=0;i<256;i++)
			avg += histo[i]*i;
		if (avg==0)
		{
			// completely black scene
			//  extremely low brightness? -> reset it to 1
			//  disabled lighting? -> avoid increasing brightness ad infinitum
			pp.brightness = rr::RRVec3(1);
		}
		else
		{
			avg = avg/(swidth*sheight)+1;
			if (histo[255]>=swidth*sheight*7/10) avg = 1000; // at least 70% of screen overshot, adjust faster
			pp.brightness *= pow(pp.targetIntensity*255/avg,RR_CLAMPED(pp.secondsSinceLastAdjustment*0.15f,0.0002f,0.2f));
		}
		//rr::RRReporter::report(rr::INF1,"%d\n",avg);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsToneMapping

PluginRuntime* PluginParamsToneMapping::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimeToneMapping(pathToShaders, pathToMaps);
}

}; // namespace