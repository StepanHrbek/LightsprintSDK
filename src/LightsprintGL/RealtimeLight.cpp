// --------------------------------------------------------------------------
// RealtimeLight, provides multiple generated instances of light for area light simulation.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstring> // NULL
#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/RRDebug.h"
#include "Workaround.h"
#include <GL/glew.h>

namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// RealtimeLight

	RealtimeLight::RealtimeLight(rr::RRLight& _rrlight)
		: rrlight(_rrlight)
	{
		RR_ASSERT(&_rrlight);

		//smallMapGPU = Texture::create(NULL,w,h,false,Texture::TF_RGBA,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		smallMapCPU = NULL;
		numTriangles = 0;
		dirtyShadowmap = true;
		dirtyGI = true;
		dirtyRange = true;

		csmObserverPos = rr::RRVec3(0);
		csmObserverDir = rr::RRVec3(1,0,0);
		csmObserverNear = 0;
		csmSceneSize = rr::RRVec3(1);
		camera = new rr::RRCamera(_rrlight);
		deleteCamera = true;
		shadowOnly = false;
		areaType = LINE;
		areaSize = 0.2f;
		shadowTransparencyRequested = RGB_SHADOWS;
		shadowTransparencyActual = RGB_SHADOWS;
		numInstancesInArea = 1;
		positionOfLastDDI = rr::RRVec3(1e6);
		numSoftShadowSamples = 4;
		changesInProjectedTextureAffectGI = true;
	}

	RealtimeLight::~RealtimeLight()
	{
		delete[] smallMapCPU;
		if (deleteCamera) delete getCamera();
		for (unsigned c=0;c<2;c++)
			for (unsigned i=0;i<shadowmaps[c].size();i++)
				delete shadowmaps[c][i];
	}

	void RealtimeLight::configureCSM(const rr::RRCamera* observer, const rr::RRObject* scene)
	{
		if (rrlight.type==rr::RRLight::DIRECTIONAL)
		{
			if (observer)
			{
				csmObserverPos = observer->getPosition();
				csmObserverDir = observer->getDirection();
				csmObserverNear = observer->getNear();
			}
			if (scene)
			{
				rr::RRVec3 mini,maxi;
				scene->getCollider()->getMesh()->getAABB(&mini,&maxi,NULL);
				getCamera()->setPosition((maxi+mini)/2);
				getCamera()->setRange(-0.5f*(mini-maxi).length(),0.5f*(mini-maxi).length());
				getCamera()->setOrthoSize((maxi-mini).maxi());
				csmSceneSize = maxi-mini;
			}
		}
	}

	rr::RRVec3 RealtimeLight::getObserverPos() const
	{
		return csmObserverPos;
	}

	const Texture* RealtimeLight::getProjectedTexture()
	{
		// Ignore projected texture for point and dir lights.
		if (getRRLight().type!=rr::RRLight::SPOT)
		{
			return NULL;
		}
		// Automatically dirty GI if texture changes.
		rr::RRBuffer* buffer = getRRLight().rtProjectedTexture;
		if (changesInProjectedTextureAffectGI && buffer)
		{
			const Texture* oldTexture = (Texture*)buffer->customData;
			unsigned oldTextureVersion = oldTexture ? oldTexture->version : 369852;
			const Texture* newTexture = getTexture(buffer);
			unsigned newTextureVersion = newTexture ? newTexture->version : 258741;
			dirtyGI |= newTextureVersion!=oldTextureVersion;
			return newTexture;
		}
		return getTexture(buffer);
	}

	void RealtimeLight::updateAfterRRLightChanges()
	{
		if (!getCamera()->isOrthogonal() && rrlight.type==rr::RRLight::DIRECTIONAL)
		{
			// when setting directional, set orthogonal and disable distance attenuation
			getCamera()->setOrthogonal(true);
			rrlight.distanceAttenuationType = rr::RRLight::NONE;
		}
		else
		if (getCamera()->isOrthogonal() && rrlight.type!=rr::RRLight::DIRECTIONAL)
		{
			// when leaving directional, clear orthogonal and reasonable near/far
			getCamera()->setOrthogonal(false);
			getCamera()->setRange(0.1f,100);
		}
		// Dirty flags only if we are sure that light did change. Caller can always set dirty himself, if we don't.
		if (getCamera()->getPosition()!=rrlight.position
			|| getCamera()->getDirection()!=rrlight.direction
			|| (rrlight.type==rr::RRLight::SPOT && abs(rrlight.outerAngleRad*2-getCamera()->getFieldOfViewVerticalRad())>0.01f))
		{
			dirtyShadowmap = true;
			dirtyGI = true;
			dirtyRange = true;
		}
		// Copy position/direction.
		getCamera()->setPosition(rrlight.position);
		getCamera()->setDirection(rrlight.direction);
		// Copy outerAngle to FOV
		getCamera()->setAspect(1);
		getCamera()->setFieldOfViewVerticalDeg( (rrlight.type==rr::RRLight::SPOT) ? RR_RAD2DEG(rrlight.outerAngleRad)*2 : 90 ); // aspect must be already set
	}

	rr::RRCamera* RealtimeLight::getCamera() const
	{
		return camera;
	}

	rr::RRCamera* RealtimeLight::setCamera(rr::RRCamera* _camera)
	{
		rr::RRCamera* oldCamera = camera;
		camera = _camera;
		deleteCamera = false;
		return oldCamera;
	}

	unsigned RealtimeLight::getNumShadowmaps(bool color) const
	{
		if (!rrlight.castShadows) return 0;
		if (color && shadowTransparencyActual!=RGB_SHADOWS) return 0;
		switch (rrlight.type)
		{
			case rr::RRLight::POINT: return 6;
			case rr::RRLight::SPOT: return numInstancesInArea;
			case rr::RRLight::DIRECTIONAL: return RR_CLAMPED(rrlight.rtNumShadowmaps,1,3);
			default: RR_ASSERT(0); return 0;
		}
	}

	void RealtimeLight::setShadowmapSize(unsigned newSize)
	{
		if (newSize!=rrlight.rtShadowmapSize)
		{
			rrlight.rtShadowmapSize = newSize;
			dirtyShadowmap = true;
		}
	}

	Texture* RealtimeLight::getShadowmap(unsigned instance, bool color)
	{
		unsigned c = color?1:0;
		// check inputs
		unsigned numShadowmaps = getNumShadowmaps(color);
		if (instance>=numShadowmaps)
		{
			RR_ASSERT(0);
			return NULL;
		}
		// resize vector if necessary
		while (shadowmaps[c].size()<=instance)
			shadowmaps[c].push_back(NULL);
		// delete shadowmap if size does not match
		Texture*& shadowmap = shadowmaps[c][instance];
		if (shadowmap && (shadowmap->getBuffer()->getWidth()!=rrlight.rtShadowmapSize || shadowmap->getBuffer()->getHeight()!=rrlight.rtShadowmapSize))
			RR_SAFE_DELETE(shadowmap);
		// allocate shadowmap if it is NULL
		if (!shadowmap)
		{
			shadowmap = Texture::createShadowmap(rrlight.rtShadowmapSize,rrlight.rtShadowmapSize,color);
			dirtyShadowmap = true;

			static int i = 0;
			if (i++==3000)
				rr::RRReporter::report(rr::WARN,"3000th (re)allocation of shadowmap, either you change settings too often or something is wrong.\n");
		}
		// return shadowmap
		return shadowmap;
	}

	void RealtimeLight::setNumShadowSamples(unsigned _numSamples)
	{
		switch (_numSamples)
		{
			case 1:
			case 2:
			case 4:
			case 8:
				if ((numSoftShadowSamples>1) != (_numSamples>1))
					dirtyShadowmap = true; // SM needs rebuild with different polygon offset
				numSoftShadowSamples = _numSamples;
		}
	}

	unsigned RealtimeLight::getNumShadowSamples() const
	{
		if (!getRRLight().castShadows) return 0;
		if (Workaround::needsOneSampleShadowmaps(getRRLight()))
			return 1;
		switch(getRRLight().type)
		{
			case rr::RRLight::POINT: return Workaround::needsHardPointShadows() ? 1 : numSoftShadowSamples;
			case rr::RRLight::SPOT: return numSoftShadowSamples;
			case rr::RRLight::DIRECTIONAL: return numSoftShadowSamples;
			default: RR_ASSERT(0);
		}
		return 1;
	}

	unsigned RealtimeLight::getNumShadowSamples(unsigned instance) const
	{
		return getNumShadowSamples();
	}

	void RealtimeLight::setRangeDynamically(const rr::RRCollider* collider, const rr::RRObject* object)
	{
		if (!object)
			return;

		if (rrlight.type!=rr::RRLight::DIRECTIONAL)
		{
			rr::RRVec2 distanceMinMax(1e10f,0);
			if (rrlight.type==rr::RRLight::POINT)
			{
				// POINT
				object->getCollider()->getDistancesFromPoint(rrlight.position,object,distanceMinMax);
			}
			else
			{
				// SPOT
				rr::RRCamera shadowmapCamera;
				for (unsigned i=0;i<getNumShadowmaps();i++)
				{
					object->getCollider()->getDistancesFromCamera(getShadowmapCamera(i,shadowmapCamera),object,distanceMinMax);
				}
			}
			if (distanceMinMax[1]>=distanceMinMax[0]
				// better keep old range if detected distance is 0 (camera in wall?)
				&& distanceMinMax[1]>0)
			{
				// with *0.9f instead of 0.5*f, our shadow bias was sometimes insufficient for very close occluders,
				//  resulting in acne or full black square in close proximity of light
				// ways to make problem sufficiently rare:
				// a) use 5*fixedBias
				// b) use 0.5*near  <- implemented here
				getCamera()->setRange(distanceMinMax[0]*0.5f,distanceMinMax[1]*5);
				//rr::RRReporter::report(rr::INF2,"setRangeDynamically()\n");
				dirtyShadowmap = true;
			}
		}
		dirtyRange = false;
	}

	rr::RRCamera& RealtimeLight::getShadowmapCamera(unsigned instance, rr::RRCamera& light) const
	{
		light = *camera;
		unsigned numInstances = getNumShadowmaps();
		if (instance>=numInstances) 
		{
			rr::RRReporter::report(rr::WARN,"RealtimeLight: instance %d requested, but light has only %d instances.\n",instance,numInstances);
		}
		else
		if (numInstances==1)
		{
			// only 1 instance -> use unmodified camera
		}
		else
		switch (getRRLight().type)
		{
			case rr::RRLight::DIRECTIONAL:
				{
					// setup second map in cascade
					// DDI needs map0 big, so map0 is big, map1 is smaller
					if (instance==0)
					{
						// already set by configureCSM()
					}
					else
					{
						// keep constant pixel size in 3-level CSM in 1-15km scenes
						// base is 0.2 for ortho up to 1km, drops to 0.05 above 15km
						float base = sqrt(40/light.getOrthoSize());
						RR_CLAMP(base,0.05f,0.2f);
						// increase pixel size when looking from distance
						if (csmObserverNear>0) // may be negative for ortho observer
						{
							// if near is big, increase SM sizes, don't boosting waste SM space on tiny fraction of screen (or even in area cropped by near)
							// satisfy light.orthoSize * base^(numInstances-1) > csmObserverNear*10
							//         base^(numInstances-1) > csmObserverNear*10/light.orthoSize
							//         base > pow(csmObserverNear*10/light.orthoSize,1/(numInstances-1))
							// 10 works well in ex5.dae and sponza.dae
							float minBase = pow(csmObserverNear*10/light.getOrthoSize(),1.0f/(numInstances-1));
							if (_finite(minBase) && base<minBase)
								base = RR_MIN(minBase,0.5f);
						}
						float visibleArea = powf(base,(float)instance);
						light.setOrthoSize(light.getOrthoSize()*visibleArea);
						rr::RRVec3 lightPos = csmObserverPos
							// move SM center in view direction -> improves quality in front of camera, reduces quality behind camera (visible if camera turns back)
							+csmObserverDir*(light.getOrthoSize()*0.5f);
						if (Workaround::supportsDepthClamp())
							light.setRange(light.getNear()*visibleArea,light.getFar()*visibleArea);
						// prevent subpixel jitter, probably no longer working
						double r = lightPos.dot(light.getRight());
						double u = lightPos.dot(light.getUp());
						double tmp;
						double pixelSize = light.getOrthoSize()/rrlight.rtShadowmapSize*2;
						r = modf(r/pixelSize,&tmp)*pixelSize;
						u = modf(u/pixelSize,&tmp)*pixelSize;
						light.setPosition(lightPos - light.getRight()*(float)r+light.getUp()*(float)u);
					}
				}
				break;
			case rr::RRLight::POINT:
				{
					// edit output (view matrix) to avoid rounding errors, inputs stay unmodified
					RR_ASSERT(instance<6);
					rr::RRCamera::View views[6] = {rr::RRCamera::TOP,rr::RRCamera::BOTTOM,rr::RRCamera::FRONT,rr::RRCamera::BACK,rr::RRCamera::LEFT,rr::RRCamera::RIGHT};
					light.setView(views[instance],NULL);
					light.setOrthogonal(false);
				}
				break;
			case rr::RRLight::SPOT:
				{
					switch(areaType)
					{
						case LINE:
							// edit inputs, update outputs
							light.setPosition(light.getPosition() + light.getRight()*(areaSize*(instance/(numInstances-1.f)*2-1)));
							break;
						case RECTANGLE:
							// edit inputs, update outputs
							{int q=(int)sqrtf((float)(numInstances-1))+1;
							light.setPosition(light.getPosition() + light.getRight()*(areaSize*(instance/q/(q-1.f)-0.5f)) + light.getUp()*(areaSize*(instance%q/(q-1.f)-0.5f)));
							break;}
						case CIRCLE:
							// edit inputs, update outputs
							light.setPosition(light.getPosition() + light.getRight()*(areaSize*sin(instance*2*RR_PI/numInstances)) + light.getUp()*(areaSize*cos(instance*2*RR_PI/numInstances)));
							break;
					}
				}
				break;
		}

		return light;
	}

}; // namespace
