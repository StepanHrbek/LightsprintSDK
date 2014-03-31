// --------------------------------------------------------------------------
// Renders 360 degree panorama.
// Copyright (C) 2010-2014 Stepan Hrbek, Lightsprint. All rights reserved.
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

		// resize cube
		unsigned size = (unsigned)sqrtf(_sp.viewport[2]*_sp.viewport[3]/6.f+1);
		size = RR_MIN3(size,_sp.viewport[2],_sp.viewport[3]);
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
		float x0 = 0;
		float y0 = 0;
		float w = 1;
		float h = 1;
		if (pp.panoramaMode!=rr_gl::PM_EQUIRECTANGULAR)
		switch (pp.panoramaCoverage)
		{
			case rr_gl::PC_FULL_STRETCH:
				break;
			case rr_gl::PC_FULL:
				if (_sp.viewport[2]>_sp.viewport[3])
				{
					w = _sp.viewport[3]/float(_sp.viewport[2]);
					x0 = (1-w)/2;
				}
				else
				{
					h = _sp.viewport[2]/float(_sp.viewport[3]);
					y0 = (1-h)/2;
				}
				break;
			case rr_gl::PC_TRUNCATE_BOTTOM:
				h = _sp.viewport[2]/float(_sp.viewport[3]);
				y0 = 1-h;
				break;
			case rr_gl::PC_TRUNCATE_TOP:
				h = _sp.viewport[2]/float(_sp.viewport[3]);
				break;
		}
		x0 -= _sp.camera->getScreenCenter().x;
		y0 -= _sp.camera->getScreenCenter().y;
		float scale = (pp.panoramaMode==PM_FISHEYE) ? pp.scale * 360/pp.fisheyeFovDeg : pp.scale;
		ToneParameters tp;
		tp.gamma = _sp.srgbCorrect?0.45f:1.f;
		_renderer.getTextureRenderer()->render2D(cubeTexture,&tp,x0+w/2-w*scale/2,y0+h/2-h*scale/2,w*scale,h*scale,-1,(pp.panoramaMode==PM_EQUIRECTANGULAR)?"#define CUBE_TO_EQUIRECTANGULAR\n":((pp.panoramaMode==PM_LITTLE_PLANET)?"#define CUBE_TO_LITTLE_PLANET\n":((pp.panoramaMode==PM_FISHEYE)?"#define CUBE_TO_FISHEYE\n":NULL)),pp.fisheyeFovDeg);
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
