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
// DynamicObjectAI

class DynamicObjectAI
{
public:
	DynamicObjectAI()
	{
		speed = 0.1f;
		shuffle();
		pos = shift;
	}
	void shuffle()
	{
		shift = RRVec3((rand()/(float)RAND_MAX-0.5f),0,(rand()/(float)RAND_MAX-0.5f))*1.8f;
	}
	RRVec3 updatePosition(RRReal seconds)
	{
		if (!getSceneCollider()) return pos;
		RRVec3 eyepos = rr::RRVec3(currentFrame.eye.pos[0],currentFrame.eye.pos[1],currentFrame.eye.pos[2]);
		RRVec3 eyedir = rr::RRVec3(currentFrame.eye.dir[0],currentFrame.eye.dir[1],currentFrame.eye.dir[2]).normalized();
		// block movement of very close objects
		if ((eyepos+eyedir*1.2f-(pos+RRVec3(0,1,0))).length()<1) return pos;
		// calculate new position
		RRReal dist = 30*speed;
		RRVec3 destination = eyepos + eyedir*dist + shift*dist;
		RRVec3 dir = destination-pos;
		pos += dir*speed*seconds;
		// stick to ground, down
		RRRay* ray = RRRay::create();
		ray->rayOrigin = pos+RRVec3(0,0.5f,0);
		ray->rayDirInv[0] = 1e10;
		ray->rayDirInv[1] = -1;
		ray->rayDirInv[2] = 1e10;
		ray->rayLengthMin = 0;
		ray->rayLengthMax = 5;
		ray->rayFlags = RRRay::FILL_POINT3D;
		if (getSceneCollider()->intersect(ray))
		{
			pos = ray->hitPoint3d;
		}
		else
		{
			// stick up
			ray->rayDirInv[1] = +1;
			ray->rayFlags = RRRay::FILL_POINT3D;
			if (getSceneCollider()->intersect(ray))
			{
				pos = ray->hitPoint3d;
			}
		}
		delete ray;
		return pos;
	}
	RRReal speed;
	RRVec3 pos;
private:
	RRVec3 shift;
};


/////////////////////////////////////////////////////////////////////////////
//
// DynamicObjects

DynamicObjects::DynamicObjects()
{
	/*
	rr_gl::UberProgramSetup material;

	// diffuse
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 0;
	material.MATERIAL_SPECULAR_MAP = 0;
	material.MATERIAL_NORMAL_MAP = 0;
	//dynaobject[3] = DynamicObject::create("3ds/characters/armyman2003.3ds",0.006f,material); // 14k
	addObject(DynamicObject::create("3ds/characters/blackman1/blackman.3ds",0.95f,material,0)); // 1k
	addObject(DynamicObject::create("3ds/characters/civil/civil.3ds",0.01f,material,0)); // 2k
	//dynaobject[4] = DynamicObject::create("3ds/characters/3dm-female3/3dm-female3.3ds",0.008f,material); // strasny vlasy
	//dynaobject[5] = DynamicObject::create("3ds/characters/Tifa/Tifa.3ds",0.028f,material); // prilis lowpoly oblicej
	//dynaobject[5] = DynamicObject::create("3ds/characters/icop/icop.3DS",0.04f,material);
	//dynaobject[6] = DynamicObject::create("3ds/characters/ct/crono.3ds",0.01f,material);
	//dynaobject[7] = DynamicObject::create("3ds/characters/ct/lucca.3ds",0.01f,material);

	// diff+specular map+normalmap
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 1;
	material.MATERIAL_NORMAL_MAP = 1;
	addObject(DynamicObject::create("3ds/characters/sven/sven.3ds",0.011f,material,8)); // 2k

	// diff+specular
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 0;
	material.MATERIAL_NORMAL_MAP = 0;
#ifdef HIGH_DETAIL
	addObject(DynamicObject::create("3ds/characters/woman-statue HD.3ds",0.004f,material,4)); // 44k
	addObject(DynamicObject::create("3ds/characters/Jessie HD.3DS",0.022f,material,16));
#else
	addObject(DynamicObject::create("3ds/characters/woman-statue9.3ds",0.004f,material,4)); // 9k
	// 3
	addObject(DynamicObject::create("3ds/characters/Jessie16.3DS",0.022f,material,16)); // 16k
	// 5
#endif

	// diff+specular map
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 1;
	material.MATERIAL_NORMAL_MAP = 0;
	addObject(DynamicObject::create("3ds/characters/potato/potato01.3ds",0.004f,material,16)); // 13k
	// 4

	// specular
	material.MATERIAL_DIFFUSE = 0;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_MAP = 0;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 0;
	material.MATERIAL_NORMAL_MAP = 0;
#ifdef HIGH_DETAIL
	addObject(DynamicObject::create("3ds/characters/I Robot female HD.3ds",0.024f,material,16));
#else
	addObject(DynamicObject::create("3ds/characters/I Robot female.3ds",0.24f,material,16)); // 20k
#endif

	// static: quake = 28k

	// ok otexturovane
	//dynaobject = DynamicObject::create("3ds/characters/ct/crono.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds/characters/ct/lucca.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds/characters/sven/sven.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds/characters/potato/potato01.3ds",0.004f); // ok
	//dynaobject = DynamicObject::create("3ds/objects/head/head.3DS",0.004f); // ok. vytvari zeleny facy po koupelne
	//dynaobject = DynamicObject::create("3ds/characters/swatfemale/female.3ds",0.02f); // spatne normaly na bocich
	//dynaobject = DynamicObject::create("3ds/characters/icop/icop.3DS",0.04f); // spatne normaly na zadech a chodidlech
	//dynaobject = DynamicObject::create("3ds/characters/GokuGT_3DS/Goku_GT.3DS",0.004f); // ok, ale jen nekomercne

	// ok neotexturovane
	//dynaobject = DynamicObject::create("3ds/characters/armyman2003.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds/characters/i robot female.3ds",0.04f); // ok
	//dynaobject = DynamicObject::create("3ds/objects/knife.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds/characters/snowman.3ds",1); // spatne normaly zezadu
	//dynaobject = DynamicObject::create("3ds/objects/gothchocker.3ds",0.2f); // spatne normaly zezadu
	//dynaobject = DynamicObject::create("3ds/objects/rubic_cube.3ds",0.01f); // spatne normaly, ale pouzitelne
	//dynaobject = DynamicObject::create("3ds/objects/polyhedrons_ts.3ds",0.1f); // sesmoothovane normaly, chybi hrany
	//dynaobjectAI[1]->shuffle();
	*/
}

void DynamicObjects::addObject(DynamicObject* dynobj)
{
	if (dynobj)
	{
		dynaobject.push_back(dynobj);
		dynaobjectAI.push_back(new DynamicObjectAI());
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
			dynaobjectAI[objIndex]->pos = worldFoot;
			dynaobject[objIndex]->worldFoot = worldFoot;
			dynaobject[objIndex]->updatePosition();
			assert(dynaobject[objIndex]->visible);
		}
	}
	else
	{
		// v .ani je vic objektu nez v .cfg a tys sahnul na '1'..'9'
		assert(0);
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
		assert(0);
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
			// copy changes to AI
			dynaobjectAI[j]->pos = dynaobject[j]->worldFoot;
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

void DynamicObjects::renderSceneDynamic(rr::RRDynamicSolver* solver, rr_gl::UberProgram* uberProgram, rr_gl::UberProgramSetup uberProgramSetup, rr_gl::Camera* camera, const rr::RRVector<rr_gl::RealtimeLight*>* lights, unsigned firstInstance, const rr::RRVec4* brightness, float gamma) const
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
			dynaobject[i]->render(uberProgram,uberProgramSetup,lights,firstInstance,currentFrame.eye,brightness,gamma);
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
		delete dynaobjectAI[i];
	}
}
