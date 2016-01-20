// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lens flare postprocess.
// --------------------------------------------------------------------------

#include <cstdlib> // rand()
#include "Lightsprint/GL/PluginLensFlare.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// CollisionHandlerTransparency

class CollisionHandlerTransparency : public rr::RRCollisionHandler
{
public:
	virtual void init(rr::RRRay& ray)
	{
		ray.rayFlags |= rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE|rr::RRRay::FILL_POINT2D;
		transparency = rr::RRVec3(1);
	}
	virtual bool collides(const rr::RRRay& ray)
	{
		RR_ASSERT(ray.rayFlags&rr::RRRay::FILL_POINT2D);
		RR_ASSERT(ray.rayFlags&rr::RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray.rayFlags&rr::RRRay::FILL_SIDE);

		rr::RRPointMaterial pointMaterial;
		ray.hitObject->getPointMaterial(ray.hitTriangle,ray.hitPoint2d,nullptr,true,pointMaterial); // custom scale transparency is identical to physical one (and even if not, it would be good enough here)
		if (pointMaterial.sideBits[ray.hitFrontSide?0:1].renderFrom)
			transparency *= pointMaterial.specularTransmittance.color;
		return transparency==rr::RRVec3(0);
	}
	virtual bool done()
	{
		return false;
	}
	rr::RRVec3 transparency;
};


//////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeLensFlare

class PluginRuntimeLensFlare : public PluginRuntime
{
	enum {NUM_PRIMARY_MAPS=3,NUM_SECONDARY_MAPS=3};
	rr::RRBuffer* primaryMap[NUM_PRIMARY_MAPS];
	rr::RRBuffer* secondaryMap[NUM_SECONDARY_MAPS];
	bool colorizeSecondaryMap[NUM_SECONDARY_MAPS];
	class CollisionHandlerTransparency* collisionHandlerTransparency;

public:

	PluginRuntimeLensFlare(const PluginCreateRuntimeParams& params)
	{
		for (unsigned i=0;i<NUM_PRIMARY_MAPS;i++)
			primaryMap[i] = rr::RRBuffer::load(rr::RRString(0,L"%lsflare_prim%d.png",params.pathToMaps.w_str(),i+1));
		for (unsigned i=0;i<NUM_SECONDARY_MAPS;i++)
		{
			secondaryMap[i] = rr::RRBuffer::load(rr::RRString(0,L"%lsflare_sec%d.png",params.pathToMaps.w_str(),i+1));
			// is it mostly grayscale? we will colorize it in renderLensFlare() only if it is mostly gray
			if (secondaryMap[i])
			{
				float sum = 0;
				unsigned numElements = secondaryMap[i]->getNumElements();
				for (unsigned j=0;j<numElements;j++)
				{
					rr::RRVec3 color = secondaryMap[i]->getElement(j,nullptr);
					sum += abs(color[0]-color[1])+abs(color[1]-color[2])+abs(color[2]-color[0]);
				}
				colorizeSecondaryMap[i] = sum/numElements<0.03f;
			}
		}
		collisionHandlerTransparency = new CollisionHandlerTransparency;
	}

	virtual ~PluginRuntimeLensFlare()
	{
		delete collisionHandlerTransparency;
		for (unsigned i=0;i<NUM_PRIMARY_MAPS;i++)
			delete primaryMap[i];
		for (unsigned i=0;i<NUM_SECONDARY_MAPS;i++)
			delete secondaryMap[i];
	}

	//! \param flareSize
	//!  Relative size of flare, 1 for typical size.
	//! \param flareId
	//!  Various flare parameters are generated from this number.
	//! \param textureRenderer
	//!  Pointer to caller's TextureRenderer instance, we save time by not creating local instance.
	//! \param aspect
	//!  Camera aspect.
	//! \param lightPositionInWindow
	//!  0,0 represents center of window, -1,-1 top left window corner, 1,1 bottom right window corner.
	void renderLensFlare(float _flareSize, unsigned _flareId, TextureRenderer* _textureRenderer, float _aspect, rr::RRVec2 _lightPositionInWindow)
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
		rr::RRBuffer* map = primaryMap[rand()%NUM_PRIMARY_MAPS];
		if (map)
			_textureRenderer->render2D(getTexture(map),nullptr,topleft.x*0.5f+0.5f,topleft.y*0.5f+0.5f,size.x*0.5f,size.y*0.5f);

		unsigned numSecondaryFlares = rand()%10;
		for (unsigned i=0;i<numSecondaryFlares;i++)
		{
			rr::RRVec4 color(0);
			color[rand()%3] = rand()*1.2f/RAND_MAX;
			size = baseSize * (float)(1+(rand()%5));
			center -= _lightPositionInWindow*(rand()*2.0f/RAND_MAX);
			topleft = center-size/2;
			unsigned mapIndex = rand()%NUM_SECONDARY_MAPS;
			if (secondaryMap[mapIndex])
			{
				ToneParameters tp;
				if (colorizeSecondaryMap[mapIndex])
					tp.color = color;
				_textureRenderer->render2D(getTexture(secondaryMap[mapIndex]),&tp,topleft.x*0.5f+0.5f,topleft.y*0.5f+0.5f,size.x*0.5f,size.y*0.5f);
			}
		}

		// cleanup
		srand(oldSeed);
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsLensFlare& pp = *dynamic_cast<const PluginParamsLensFlare*>(&_pp);
		
		for (unsigned i=0;i<pp.lights->size();i++)
		{
			// is it sun above horizon?
			rr::RRLight* light = (*pp.lights)[i];
			if (light && light->enabled && light->type==rr::RRLight::DIRECTIONAL && light->direction.y<=0)
			{
				// is it on screen?
				rr::RRVec2 lightPositionInWindow = _sp.camera->getPositionInViewport(_sp.camera->getPosition()-light->direction*1e10f);
				if (_sp.camera->getDirection().dot(light->direction)<0 && lightPositionInWindow.x>-1 && lightPositionInWindow.x<1 && lightPositionInWindow.y>-1 && lightPositionInWindow.y<1)
				{
					if (!pp.collider)
					{
						renderLensFlare(pp.flareSize,pp.flareId,_renderer.getTextureRenderer(),_sp.camera->getAspect(),lightPositionInWindow);
					}
					else
					{
						// is it visible, not occluded?
						rr::RRVec3 transparencySum(0);
						rr::RRVec3 dirSum(0);
						rr::RRVec3 unused;
						rr::RRRay ray;
						ray.collisionHandler = collisionHandlerTransparency;
						_sp.camera->getRay(lightPositionInWindow,ray.rayOrigin,unused);
						ray.rayLengthMin = 0;
						ray.rayLengthMax = 1e10f;
						ray.rayFlags = 0;

						// generate everything from _flareId
						unsigned oldSeed = rand();
						srand(pp.flareId);

						for (unsigned i=0;i<RR_MAX(pp.quality,1);i++)
						{
							ray.rayDir = ( rr::RRVec3(rand()/(float)RAND_MAX-0.5f,rand()/(float)RAND_MAX-0.5f,rand()/(float)RAND_MAX-0.5f)*0.02f - light->direction.normalized() ).normalized();
							ray.hitObject = pp.scene; // we set hitObject for colliders that don't set it
							pp.collider->intersect(ray);
							transparencySum += collisionHandlerTransparency->transparency;
							dirSum -= ray.rayDir*collisionHandlerTransparency->transparency.sum();
						}
						// cleanup
						srand(oldSeed);
						float transparency = transparencySum.avg()/RR_MAX(pp.quality,1);
						if (transparency>0.15f) // hide tiny flares, they jump randomly when looking through tree
						{
							// move flare a bit if light is half occluded
							lightPositionInWindow = _sp.camera->getPositionInViewport(_sp.camera->getPosition()-dirSum*1e10f);

							renderLensFlare(pp.flareSize*transparency,pp.flareId,_renderer.getTextureRenderer(),_sp.camera->getAspect(),lightPositionInWindow);
						}
					}
				}
			}
		}
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PluginParamsLensFlare

PluginRuntime* PluginParamsLensFlare::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeLensFlare(params);
}

}; // namespace
