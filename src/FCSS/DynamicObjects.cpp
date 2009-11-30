#include "DynamicObjects.h"
#include "AnimationScene.h"
#include "Lightsprint/RRCollider.h"

using namespace rr;

// access to current scene
extern AnimationFrame currentFrame;
void reportEyeMovement();
void reportLightMovement();
const rr::RRCollider* getSceneCollider();

/////////////////////////////////////////////////////////////////////////////
//
// DynamicObjects

void DynamicObjects::addObject(DynamicObject* dynobj)
{
	if (dynobj)
	{
		dynaobject.push_back(dynobj);
	}
	else
	{
		RR_ASSERT(0);
	}
}

rr::RRVec3 DynamicObjects::getPos(unsigned objIndex) const
{
	return (objIndex<dynaobject.size()) ? dynaobject[objIndex]->worldFoot : rr::RRVec3(0);
}

void DynamicObjects::setPos(unsigned objIndex, rr::RRVec3 worldFoot)
{
	if (objIndex<dynaobject.size())
	{
		if (dynaobject[objIndex])
		{
			dynaobject[objIndex]->worldFoot = worldFoot;
			dynaobject[objIndex]->updatePosition();
			RR_ASSERT(dynaobject[objIndex]->visible);
		}
	}
	else
	{
		// v .ani je vic objektu nez v .cfg a tys sahnul na '1'..'9'
		RR_ASSERT(0);
	}
}

rr::RRVec2 DynamicObjects::getRot(unsigned objIndex) const
{
	return (objIndex<dynaobject.size()) ? dynaobject[objIndex]->rotYZ : RRVec2(0);
}

void DynamicObjects::setRot(unsigned objIndex, rr::RRVec2 rot)
{
	if (objIndex<dynaobject.size())
	{
		if (dynaobject[objIndex])
		{
			dynaobject[objIndex]->rotYZ = rot;
			dynaobject[objIndex]->updatePosition();
		}
	}
	else
	{
		RR_ASSERT(0);
	}
}

// copy animation data from frame to actual scene
// returns whether objects moved
bool DynamicObjects::copyAnimationFrameToScene(const LevelSetup* setup, const AnimationFrame& frame, bool lightsChanged)
{
	bool objMoved = false;
	if (lightsChanged)
	{
		currentFrame.light = frame.light;
		reportLightMovement();
	}
	currentFrame.eye = frame.eye;
	reportEyeMovement();
	currentFrame.brightness = frame.brightness;
	currentFrame.gamma = frame.gamma;
	currentFrame.projectorIndex = frame.projectorIndex;
	currentFrame.shadowType = frame.shadowType;
	currentFrame.indirectType = frame.indirectType;
	//for (AnimationFrame::DynaPosRot::const_iterator i=frame->dynaPosRot.begin();i!=frame->dynaPosRot.end();i++)
	for (unsigned i=0;i<dynaobject.size();i++)
	{
		if (dynaobject[i])
			dynaobject[i]->visible = false;
	}
	for (unsigned i=0;i<setup->objects.size();i++) // i = source object index in frame
	{
		unsigned j = setup->objects[i]; // j = destination object index in this
		if (i<frame.dynaPosRot.size() && j<dynaobject.size() && dynaobject[j])
		{
			dynaobject[j]->visible = true;
			if ((dynaobject[j]->worldFoot-frame.dynaPosRot[i].pos).length()>0.001f) objMoved = true; // treshold is necessary, rounding errors make position slightly off
			dynaobject[j]->worldFoot = frame.dynaPosRot[i].pos;
			if ((dynaobject[j]->rotYZ-frame.dynaPosRot[i].rot).length()>0.001f) objMoved = true;
			dynaobject[j]->rotYZ = frame.dynaPosRot[i].rot;
//static float globalRot = 0; globalRot += 0.2f; dynaobject[j]->rotYZ[0] = globalRot; //!!! automaticka rotace vsech objektu
			dynaobject[j]->updatePosition();
		}
	}
	return objMoved;
}

// copy animation data from frame to actual scene
void DynamicObjects::copySceneToAnimationFrame_ignoreThumbnail(AnimationFrame& frame, const LevelSetup* setup)
{
	frame.eye = currentFrame.eye;
	frame.light = currentFrame.light;
	frame.brightness = currentFrame.brightness;
	frame.gamma = currentFrame.gamma;
	frame.dynaPosRot.clear();
	frame.projectorIndex = currentFrame.projectorIndex;
	frame.shadowType = currentFrame.shadowType;
	frame.indirectType = currentFrame.indirectType;
	for (unsigned sceneIndex=0;sceneIndex<setup->objects.size();sceneIndex++) // scene has few objects
	{
		unsigned demoIndex = setup->objects[sceneIndex]; // demo has more objects
		AnimationFrame::DynaObjectPosRot tmp;
		if (demoIndex<dynaobject.size() // but hand edit of .cfg may remove objects referenced from .ani, lets check it
			&& dynaobject[demoIndex])
		{
			tmp.pos = dynaobject[demoIndex]->worldFoot;
			tmp.rot = dynaobject[demoIndex]->rotYZ;
		}
		frame.dynaPosRot.push_back(tmp);
	}
	frame.validate((unsigned)setup->objects.size());
}

void DynamicObjects::updateSceneDynamic(rr::RRDynamicSolver* solver)
{
	for (unsigned i=0;i<dynaobject.size();i++)
	{
		if (dynaobject[i] && dynaobject[i]->visible)
		{
			solver->updateEnvironmentMapCache(dynaobject[i]->illumination);
		}
	}
}

void DynamicObjects::renderSceneDynamic(rr::RRDynamicSolver* solver, rr_gl::UberProgram* uberProgram, rr_gl::UberProgramSetup uberProgramSetup, rr_gl::Camera* camera, const rr::RRVector<rr_gl::RealtimeLight*>* lights, unsigned firstInstance, const rr::RRVec4* brightness, float gamma, float waterLevel) const
{
	// use object space
	uberProgramSetup.OBJECT_SPACE = true;
	// use environment maps
	if (uberProgramSetup.LIGHT_INDIRECT_VCOLOR || uberProgramSetup.LIGHT_INDIRECT_MAP)
	{
		// indirect from envmap
		uberProgramSetup.SHADOW_MAPS = 1; // decrease shadow quality on dynobjects
		uberProgramSetup.LIGHT_INDIRECT_CONST = 0;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 0;
		uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
		uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = 0;
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = 1;
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = 1;
	}
	//dynaobject[4]->worldFoot = rr::RRVec3(2.2f*sin(d*0.005f),1.0f,2.2f);

	RRReportInterval report(uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE?INF3:INF9,"Updating dynamic objects...\n");

	for (unsigned i=0;i<dynaobject.size();i++)
	{
		if (dynaobject[i] && dynaobject[i]->visible && dynaobject[i]->worldFoot!=rr::RRVec3(0))
		{
			if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
			{
				solver->updateEnvironmentMap(dynaobject[i]->illumination);
			}
			dynaobject[i]->render(uberProgram,uberProgramSetup,lights,firstInstance,currentFrame.eye,brightness,gamma,waterLevel);
		}
	}
}

DynamicObject* DynamicObjects::getObject(unsigned objectIndex)
{
	return (objectIndex<dynaobject.size())?dynaobject[objectIndex]:NULL;
}

void DynamicObjects::advanceRot(float seconds)
{
	for (unsigned i=0;i<dynaobject.size();i++)
	{
		RR_ASSERT(dynaobject[i]);
		dynaobject[i]->rotYZ[0] += 90*seconds;
		dynaobject[i]->updatePosition();
	}
}

DynamicObjects::~DynamicObjects()
{
	for (unsigned i=0;i<dynaobject.size();i++)
	{
		delete dynaobject[i];
	}
}
