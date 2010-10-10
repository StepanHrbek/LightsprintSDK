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

//////////////////////////////////////////////////////////////////////////////
//
// CollisionHandlerTransparency

class CollisionHandlerTransparency : public rr::RRCollisionHandler
{
public:
	virtual void init(rr::RRRay* ray)
	{
		ray->rayFlags |= rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE|rr::RRRay::FILL_POINT2D;
		transparency = rr::RRVec3(1);
	}
	virtual bool collides(const rr::RRRay* ray)
	{
		RR_ASSERT(ray->rayFlags&rr::RRRay::FILL_POINT2D);
		RR_ASSERT(ray->rayFlags&rr::RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray->rayFlags&rr::RRRay::FILL_SIDE);

		rr::RRPointMaterial pointMaterial;
		object->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
		if (pointMaterial.sideBits[ray->hitFrontSide?0:1].renderFrom)
			transparency *= pointMaterial.specularTransmittance.color;
		return transparency==rr::RRVec3(0);
	}
	virtual bool done()
	{
		return false;
	}
	const rr::RRObject* object;
	rr::RRVec3 transparency;
};

//////////////////////////////////////////////////////////////////////////////
//
// LensFlare

#define NO_SYSTEM_MEMORY (unsigned char*)1

LensFlare::LensFlare(const char* pathToShaders)
{
	for (unsigned i=0;i<NUM_PRIMARY_MAPS;i++)
		primaryMap[i] = rr::RRBuffer::load(tmpstr("%s../maps/flare_prim%d.png",pathToShaders,i+1));
	for (unsigned i=0;i<NUM_SECONDARY_MAPS;i++)
		secondaryMap[i] = rr::RRBuffer::load(tmpstr("%s../maps/flare_sec%d.png",pathToShaders,i+1));
	ray = rr::RRRay::create();
	collisionHandlerTransparency = new CollisionHandlerTransparency;
	ray->collisionHandler = collisionHandlerTransparency;
}

LensFlare::~LensFlare()
{
	delete collisionHandlerTransparency;
	delete ray;
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

void LensFlare::renderLensFlares(float _flareSize, unsigned _flareId, TextureRenderer* _textureRenderer, const Camera& _eye, const rr::RRLights& _lights, const rr::RRObject* _scene, unsigned _quality)
{
	if (_textureRenderer)
	{
		if (!_quality) _quality = 1;
		for (unsigned i=0;i<_lights.size();i++)
		{
			// is it sun above horizon?
			rr::RRLight* light = _lights[i];
			if (light && light->enabled && light->type==rr::RRLight::DIRECTIONAL && light->direction.y<=0)
			{
				// is it on screen?
				rr::RRVec2 lightPositionInWindow = _eye.getPositionInWindow(_eye.pos-light->direction*1e10f);
				if (_eye.dir.dot(light->direction)<0 && lightPositionInWindow.x>-1 && lightPositionInWindow.x<1 && lightPositionInWindow.y>-1 && lightPositionInWindow.y<1)
				{
					// is it visible, not occluded?
					rr::RRVec3 transparencySum(0);
					rr::RRVec3 dirSum(0);
					if (_scene)
					{
						ray->rayOrigin = _eye.getRayOrigin(_eye.pos);
						ray->rayLengthMin = 0;
						ray->rayLengthMax = 1e10f;
						ray->rayFlags = 0;
						collisionHandlerTransparency->object = _scene;

						// generate everything from _flareId
						unsigned oldSeed = rand();
						srand(_flareId);

						for (unsigned i=0;i<_quality;i++)
						{
							rr::RRVec3 rayDir = (light->direction.normalized()+rr::RRVec3(rand()/(float)RAND_MAX-0.5f,rand()/(float)RAND_MAX-0.5f,rand()/(float)RAND_MAX-0.5f)*0.1f).normalized();
							ray->rayDirInv = rr::RRVec3(-1)/rayDir;
							_scene->getCollider()->intersect(ray);
							transparencySum += collisionHandlerTransparency->transparency;
							dirSum += rayDir*collisionHandlerTransparency->transparency.sum();
						}
						// cleanup
						srand(oldSeed);
					}
					float transparency = transparencySum.avg()/_quality;
					if (transparency>0.15f) // hide tiny flares, they jump randomly when looking through tree
					{
						// move flare a bit if light is half occluded
						lightPositionInWindow = _eye.getPositionInWindow(_eye.pos-dirSum*1e10f);

						renderLensFlare(_flareSize*transparency,_flareId,_textureRenderer,_eye.getAspect(),lightPositionInWindow);
					}
				}
			}
		}
	}
}

}; // namespace
