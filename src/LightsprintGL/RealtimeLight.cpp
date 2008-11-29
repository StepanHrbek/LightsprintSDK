// --------------------------------------------------------------------------
// RealtimeLight, provides multiple generated instances of light for area light simulation.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <cstring> // NULL
#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/RRDebug.h"
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

		parent = new Camera(_rrlight);
		deleteParent = true;
		shadowmapSize = (_rrlight.type==rr::RRLight::DIRECTIONAL)?2048:1024;
		areaType = LINE;
		areaSize = 0.2f;
		transparentMaterialShadows = ALPHA_KEYED_SHADOWS;
		lightDirectMap = NULL;
		numInstancesInArea = 1;
		positionOfLastDDI = rr::RRVec3(1e6);
		softShadowsAllowed = true;
	}

	RealtimeLight::~RealtimeLight()
	{
		delete[] smallMapCPU;
		if (deleteParent) delete getParent();
		for (unsigned i=0;i<shadowmaps.size();i++)
		{
			delete shadowmaps[i];
		}
	}

	void RealtimeLight::updateAfterRRLightChanges()
	{
		if (!getParent()->orthogonal && rrlight.type==rr::RRLight::DIRECTIONAL)
		{
			// when setting directional, set orthogonal and disable distance attenuation
			getParent()->orthogonal = true;
			rrlight.distanceAttenuationType = rr::RRLight::NONE;
		}
		else
		if (getParent()->orthogonal && rrlight.type!=rr::RRLight::DIRECTIONAL)
		{
			// when leaving directional, clear orthogonal and reasonable near/fat
			getParent()->orthogonal = false;
			getParent()->setRange(0.1f,100);
		}
		// Copy position/direction.
		getParent()->pos = rrlight.position;
		getParent()->setDirection(rrlight.direction);
		// Nearly all changes to RRLight create need for shadowmap and GI update.
		// At this point we don't know what was changed anyway, so let's update always.
		dirtyShadowmap = true;
		dirtyGI = true;
	}

	void RealtimeLight::updateAfterRealtimeLightChanges()
	{
		// Copy position/direction.
		rrlight.position = getParent()->pos;
		rrlight.direction = getParent()->dir;
	}

	Camera* RealtimeLight::getParent() const
	{
		return parent;
	}

	Camera* RealtimeLight::setParent(Camera* _parent)
	{
		Camera* oldParent = parent;
		parent = _parent;
		deleteParent = false;
		return oldParent;
	}

	unsigned RealtimeLight::getNumShadowmaps() const
	{
		if (!rrlight.castShadows) return 0;
		switch (rrlight.type)
		{
			case rr::RRLight::POINT: return 6;
			case rr::RRLight::SPOT: return numInstancesInArea;
			case rr::RRLight::DIRECTIONAL: return 2;
			default: RR_ASSERT(0); return 0;
		}
	}

	Camera* RealtimeLight::getShadowmapCamera(unsigned instance, bool jittered) const
	{
		if (!parent || instance>=getNumShadowmaps())
			return NULL;
		Camera* c = new Camera(*parent);
		if (!c)
			return NULL;
		instanceMakeup(*c,instance,jittered);
		return c;
	}

	void RealtimeLight::setShadowmapSize(unsigned newSize)
	{
		if (newSize!=shadowmapSize)
		{
			shadowmapSize = newSize;
			for (unsigned i=0;i<getNumShadowmaps();i++)
			{
				getShadowmap(i)->getBuffer()->reset(rr::BT_2D_TEXTURE,newSize,newSize,1,rr::BF_DEPTH,false,NULL);
				getShadowmap(i)->reset(false,false);
			}
			dirtyShadowmap = true;
		}
	}

	Texture* RealtimeLight::getShadowmap(unsigned instance)
	{
		// check inputs
		unsigned numShadowmaps = getNumShadowmaps();
		if (instance>=numShadowmaps)
		{
			assert(0);
			return NULL;
		}
		// free or allocate shadowmaps to match with current number
		if (shadowmaps.size()!=numShadowmaps)
		{
			for (unsigned i=0;i<shadowmaps.size();i++)
			{
				delete shadowmaps[i];
			}
			shadowmaps.clear();
			for (unsigned i=0;i<numShadowmaps;i++)
			{
				shadowmaps.push_back(Texture::createShadowmap(shadowmapSize,shadowmapSize));
			}
			dirtyShadowmap = true;
		}
		// return one
		return shadowmaps[instance];
	}

	unsigned RealtimeLight::getNumShadowSamples(unsigned instance) const
	{
		if (instance>=getNumShadowmaps()) return 0;
		if (!getRRLight().castShadows) return 0;
		if (!softShadowsAllowed) return 1;
		switch(getRRLight().type)
		{
			case rr::RRLight::POINT: return 1;
			case rr::RRLight::SPOT: return 4;
			case rr::RRLight::DIRECTIONAL: return (instance==getNumShadowmaps()-1)?4:1;
			default: RR_ASSERT(0);
		}
		return 1;
	}

	void RealtimeLight::instanceMakeup(Camera& light, unsigned instance, bool jittered) const
	{
		unsigned numInstances = getNumShadowmaps();
		if (instance>=numInstances) 
		{
			rr::RRReporter::report(rr::WARN,"RealtimeLight: instance %d requested, but light has only %d instances.\n",instance,numInstances);
			return;
		}
		if (numInstances==1)
		{
			light.update();
			return; // only 1 instance -> use unmodified parent
		}
		if (getRRLight().type==rr::RRLight::DIRECTIONAL)
		{
			// setup second map in cascade
			// DDI needs map0 big, so map0 is big, map1 is smaller
			RR_ASSERT(numInstances==2); // dir must have 1 or 2 instances
			if (instance==1)
			{
				light.orthoSize *= 0.2f; // cascade goes in 5x size steps
			}
			light.update();
			return;
		}
		if (getRRLight().type==rr::RRLight::POINT)
		{
			// edit output (view matrix) to avoid rounding errors, inputs stay unmodified
			RR_ASSERT(instance<6);
			light.update();
			light.rotateViewMatrix(instance%6);
			return;
		}
		//light.update(0.3f);

		switch(areaType)
		{
			case LINE:
				// edit inputs, update outputs
				light.pos[0] += light.right[0]*(areaSize*(instance/(numInstances-1.f)*2-1));
				light.pos[1] += light.right[1]*(areaSize*(instance/(numInstances-1.f)*2-1));
				light.pos[2] += light.right[2]*(areaSize*(instance/(numInstances-1.f)*2-1));
				break;
			case RECTANGLE:
				// edit inputs, update outputs
				{int q=(int)sqrtf((float)(numInstances-1))+1;
				light.pos[0] += light.right[0]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[0]*areaSize*(instance%q/(q-1.f)-0.5f);
				light.pos[1] += light.right[1]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[1]*areaSize*(instance%q/(q-1.f)-0.5f);
				light.pos[2] += light.right[2]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[2]*areaSize*(instance%q/(q-1.f)-0.5f);
				break;}
			case CIRCLE:
				// edit inputs, update outputs
				light.pos[0] += light.right[0]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[0]*areaSize*cos(instance*2*3.14159f/numInstances);
				light.pos[1] += light.right[1]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[1]*areaSize*cos(instance*2*3.14159f/numInstances);
				light.pos[2] += light.right[2]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[2]*areaSize*cos(instance*2*3.14159f/numInstances);
				break;
		}
		if (jittered)
		{
			static signed char jitterSample[10][2] = {{0,0},{3,-2},{-2,3},{1,2},{-2,-1},{3,4},{-4,-3},{2,-1},{-1,1},{-3,0}};
			light.angle += light.getFieldOfViewHorizontalRad()/shadowmapSize*jitterSample[instance%10][0]*0.22f;
			light.angleX += light.getFieldOfViewVerticalRad()/shadowmapSize*jitterSample[instance%10][1]*0.22f;
		}
		light.update();
	}

}; // namespace
