// --------------------------------------------------------------------------
// Lens flare effect.
// Copyright (C) 2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include <cstdlib>
#include <ctime>
#include "Lightsprint/GL/LensFlare.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"
#include "tmpstr.h"

namespace rr_gl
{

#define NO_SYSTEM_MEMORY (unsigned char*)1

LensFlare::LensFlare(const char* pathToShaders)
{
	for (unsigned i=0;i<NUM_PRIMARY_MAPS;i++)
		primaryMap[i] = rr::RRBuffer::load(tmpstr("%s../maps/flare_prim%d.png",pathToShaders,i+1));
	for (unsigned i=0;i<NUM_SECONDARY_MAPS;i++)
		secondaryMap[i] = rr::RRBuffer::load(tmpstr("%s../maps/flare_sec%d.png",pathToShaders,i+1));
}

LensFlare::~LensFlare()
{
	for (unsigned i=0;i<NUM_PRIMARY_MAPS;i++)
		delete primaryMap[i];
	for (unsigned i=0;i<NUM_SECONDARY_MAPS;i++)
		delete secondaryMap[i];
}

void LensFlare::renderLensFlare(float _flareSize, unsigned _flareId, TextureRenderer* _textureRenderer, float _aspect, rr::RRVec2 _lightPositionInWindow)
{
	// set GL state
	PreserveBlend p3;
	PreserveBlendFunc p4;
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	// generate everything from _flareId
	unsigned oldSeed = rand();
	srand(_flareId);

	rr::RRVec2 baseSize = rr::RRVec2(1/sqrtf(_aspect),sqrtf(_aspect))*_flareSize/30;
	rr::RRVec2 size = baseSize * 6;
	rr::RRVec2 center = _lightPositionInWindow;
	rr::RRVec2 topleft = center-size/2;
	_textureRenderer->render2D(getTexture(primaryMap[rand()%NUM_PRIMARY_MAPS]),NULL,topleft.x*0.5f+0.5f,topleft.y*0.5f+0.5f,size.x*0.5f,size.y*0.5f);

	unsigned numSecondaryFlares = rand()%10;
	for (unsigned i=0;i<numSecondaryFlares;i++)
	{
		float color[4] = {0,0,0,0};
		color[rand()%3] = rand()*1.2f/RAND_MAX;
		size = baseSize * (float)(1+(rand()%5));
		center -= _lightPositionInWindow*(rand()*2.0f/RAND_MAX);
		topleft = center-size/2;
		_textureRenderer->render2D(getTexture(secondaryMap[rand()%NUM_SECONDARY_MAPS]),color,topleft.x*0.5f+0.5f,topleft.y*0.5f+0.5f,size.x*0.5f,size.y*0.5f);
	}

	// cleanup
	srand(oldSeed);
}

void LensFlare::renderLensFlares(float _flareSize, unsigned _flareId, TextureRenderer* _textureRenderer, rr::RRRay* _ray, const Camera& _eye, const rr::RRLights& _lights, const rr::RRObject* _scene)
{
	if (_textureRenderer)
	{
		for (unsigned i=0;i<_lights.size();i++)
		{
			// is it sun above horizon?
			rr::RRLight* light = _lights[i];
			if (light && light->type==rr::RRLight::DIRECTIONAL && light->direction.y<=0)
			{
				// is it on screen?
				rr::RRVec2 lightPositionInWindow = _eye.getPositionInWindow(_eye.pos-light->direction*1e10f);
				if (_eye.dir.dot(light->direction)<0 && lightPositionInWindow.x>-1 && lightPositionInWindow.x<1 && lightPositionInWindow.y>-1 && lightPositionInWindow.y<1)
				{
					// is it visible, not occluded?
					bool occluded = false;
					if (_scene && _ray)
					{
						_ray->rayOrigin = _eye.getRayOrigin(_eye.pos);
						_ray->rayDirInv = rr::RRVec3(-1)/light->direction;
						_ray->rayLengthMin = 0;
						_ray->rayLengthMax = 1e10f;
						_ray->rayFlags = 0;
						occluded = _scene->getCollider()->intersect(_ray);
					}
					if (!occluded)
					{
						renderLensFlare(_flareSize,_flareId,_textureRenderer,_eye.getAspect(),lightPositionInWindow);
					}
				}
			}
		}
	}
}

}; // namespace
