// --------------------------------------------------------------------------
// Renders 360 degree panorama.
// Copyright (C) 2010-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginPanorama.h"
#include "Lightsprint/GL/PluginCube.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimePanorama

class PluginRuntimePanorama : public PluginRuntime
{
	Texture* cubeTexture;
	Texture* depthTexture;
	bool cubeTextureIsSrgb;

public:

	PluginRuntimePanorama(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
	{
		cubeTexture = new Texture(rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,1,1,6,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR); // A for mirroring
		depthTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_DEPTH,false,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR);
		cubeTextureIsSrgb = false;
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsPanorama& pp = *dynamic_cast<const PluginParamsPanorama*>(&_pp);
		
		if (pp.panoramaMode==PM_OFF || !cubeTexture)
			return;

		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT,viewport);

		// resize cube
		unsigned size = (unsigned)sqrtf(viewport[2]*viewport[3]/6.f+1);
		size = RR_MIN3(size,(unsigned)viewport[2],(unsigned)viewport[3]);
		if (cubeTexture->getBuffer()->getWidth()!=size || _sp.srgbCorrect!=cubeTextureIsSrgb)
		{
			cubeTexture->getBuffer()->reset(rr::BT_CUBE_TEXTURE,size,size,6,rr::BF_RGBA,true,RR_GHOST_BUFFER); // A for mirroring
			cubeTexture->reset(false,false,_sp.srgbCorrect);
			cubeTextureIsSrgb = _sp.srgbCorrect;
		}

		// render to cube
		PluginParamsCube ppCube(_pp.next,cubeTexture,depthTexture);
		_renderer.render(&ppCube,_sp);

		// composite
		_renderer.getTextureRenderer()->render2D(cubeTexture,NULL,_sp.srgbCorrect?0.45f:1.f,0,0,1,1,-1,(pp.panoramaMode==PM_LITTLE_PLANET)?"#define LITTLE_PLANET\n":NULL);

	}

	virtual ~PluginRuntimePanorama()
	{
		if (depthTexture)
			delete depthTexture->getBuffer();
		RR_SAFE_DELETE(depthTexture);
		if (cubeTexture)
			delete cubeTexture->getBuffer();
		RR_SAFE_DELETE(cubeTexture);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsPanorama

PluginRuntime* PluginParamsPanorama::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimePanorama(pathToShaders, pathToMaps);
}

}; // namespace
