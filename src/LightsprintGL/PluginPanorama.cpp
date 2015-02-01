// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Renders 360 degree panorama.
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

	PluginRuntimePanorama(const PluginCreateRuntimeParams& params)
	{
		reentrancy = 1;
		cubeTexture = new Texture(rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,1,1,6,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR); // A for mirroring
		depthTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_DEPTH,false,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR);
		cubeTextureIsSrgb = false;
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsPanorama& pp = *dynamic_cast<const PluginParamsPanorama*>(&_pp);
		
		if (_sp.camera->panoramaMode==rr::RRCamera::PM_OFF || !cubeTexture)
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
		PluginParamsShared sp = _sp;
		rr::RRCamera camera = *_sp.camera;
		camera.panoramaMode = rr::RRCamera::PM_OFF;
		sp.camera = &camera;
		PluginParamsCube ppCube(_pp.next,cubeTexture,depthTexture,(_sp.camera->panoramaMode==rr::RRCamera::PM_FISHEYE)?_sp.camera->panoramaFisheyeFovDeg:360);
		_renderer.render(&ppCube,sp);

		// panoramaCoverage [#44]
		float x0 = 0;
		float y0 = 0;
		float w = 1;
		float h = 1;
		if (_sp.camera->panoramaMode!=rr::RRCamera::PM_EQUIRECTANGULAR)
		switch (_sp.camera->panoramaCoverage)
		{
			case rr::RRCamera::PC_FULL_STRETCH:
				break;
			case rr::RRCamera::PC_FULL:
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
			case rr::RRCamera::PC_TRUNCATE_BOTTOM:
				h = _sp.viewport[2]/float(_sp.viewport[3]);
				y0 = 1-h;
				break;
			case rr::RRCamera::PC_TRUNCATE_TOP:
				h = _sp.viewport[2]/float(_sp.viewport[3]);
				break;
		}
		//x0 -= _sp.camera->getScreenCenter().x;
		//y0 -= _sp.camera->getScreenCenter().y;

		// panoramaScale [#43]
		float scale = (_sp.camera->panoramaMode==rr::RRCamera::PM_FISHEYE) ? _sp.camera->panoramaScale * 360/_sp.camera->panoramaFisheyeFovDeg : _sp.camera->panoramaScale;

		// render to 2d
		ToneParameters tp;
		tp.gamma = _sp.srgbCorrect?0.45f:1.f;
		_renderer.getTextureRenderer()->render2D(cubeTexture,&tp,x0+w/2-w*scale/2,y0+h/2-h*scale/2,w*scale,h*scale,-1,(_sp.camera->panoramaMode==rr::RRCamera::PM_EQUIRECTANGULAR)?"#define CUBE_TO_EQUIRECTANGULAR\n":((_sp.camera->panoramaMode==rr::RRCamera::PM_LITTLE_PLANET)?"#define CUBE_TO_LITTLE_PLANET\n":((_sp.camera->panoramaMode==rr::RRCamera::PM_FISHEYE)?"#define CUBE_TO_FISHEYE\n":NULL)),_sp.camera->panoramaFisheyeFovDeg);
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

PluginRuntime* PluginParamsPanorama::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimePanorama(params);
}

}; // namespace
