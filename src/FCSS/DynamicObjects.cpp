#include "DynamicObjects.h"
#include "AnimationScene.h"
#include "Lightsprint/RRCollider.h"

using namespace rr;

// access to current scene
extern de::Camera eye;
extern de::Camera light;
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
		if(!getSceneCollider()) return pos;
		RRVec3 eyepos = rr::RRVec3(eye.pos[0],eye.pos[1],eye.pos[2]);
		RRVec3 eyedir = rr::RRVec3(eye.dir[0],eye.dir[1],eye.dir[2]).normalized();
		// block movement of very close objects
		if((eyepos+eyedir*1.2f-(pos+RRVec3(0,1,0))).length()<1) return pos;
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
		if(getSceneCollider()->intersect(ray))
		{
			pos = ray->hitPoint3d;
		}
		else
		{
			// stick up
			ray->rayDirInv[1] = +1;
			ray->rayFlags = RRRay::FILL_POINT3D;
			if(getSceneCollider()->intersect(ray))
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
	de::UberProgramSetup material;

	// diffuse
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_VCOLOR = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 0;
	material.MATERIAL_SPECULAR_MAP = 0;
	material.MATERIAL_NORMAL_MAP = 0;
	//dynaobject[3] = DynamicObject::create("3ds\\characters\\armyman2003.3ds",0.006f,material); // 14k
	addObject(DynamicObject::create("3ds\\characters\\blackman1\\blackman.3ds",0.95f,material,0)); // 1k
	addObject(DynamicObject::create("3ds\\characters\\civil\\civil.3ds",0.01f,material,0)); // 2k
	//dynaobject[4] = DynamicObject::create("3ds\\characters\\3dm-female3\\3dm-female3.3ds",0.008f,material); // strasny vlasy
	//dynaobject[5] = DynamicObject::create("3ds\\characters\\Tifa\\Tifa.3ds",0.028f,material); // prilis lowpoly oblicej
	//dynaobject[5] = DynamicObject::create("3ds\\characters\\icop\\icop.3DS",0.04f,material);
	//dynaobject[6] = DynamicObject::create("3ds\\characters\\ct\\crono.3ds",0.01f,material);
	//dynaobject[7] = DynamicObject::create("3ds\\characters\\ct\\lucca.3ds",0.01f,material);

	// diff+specular map+normalmap
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_VCOLOR = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 1;
	material.MATERIAL_NORMAL_MAP = 1;
	addObject(DynamicObject::create("3ds\\characters\\sven\\sven.3ds",0.011f,material,8)); // 2k

	// diff+specular
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_VCOLOR = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 0;
	material.MATERIAL_NORMAL_MAP = 0;
#ifdef HIGH_DETAIL
	addObject(DynamicObject::create("3ds\\characters\\woman-statue HD.3ds",0.004f,material,4)); // 44k
	addObject(DynamicObject::create("3ds\\characters\\Jessie HD.3DS",0.022f,material,16));
#else
	addObject(DynamicObject::create("3ds\\characters\\woman-statue9.3ds",0.004f,material,4)); // 9k
	// 3
	addObject(DynamicObject::create("3ds\\characters\\Jessie16.3DS",0.022f,material,16)); // 16k
	// 5
#endif

	// diff+specular map
	material.MATERIAL_DIFFUSE = 1;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_VCOLOR = 0;
	material.MATERIAL_DIFFUSE_MAP = 1;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 1;
	material.MATERIAL_NORMAL_MAP = 0;
	addObject(DynamicObject::create("3ds\\characters\\potato\\potato01.3ds",0.004f,material,16)); // 13k
	// 4

	// specular
	material.MATERIAL_DIFFUSE = 0;
	material.MATERIAL_DIFFUSE_CONST = 0;
	material.MATERIAL_DIFFUSE_VCOLOR = 0;
	material.MATERIAL_DIFFUSE_MAP = 0;
	material.MATERIAL_SPECULAR = 1;
	material.MATERIAL_SPECULAR_MAP = 0;
	material.MATERIAL_NORMAL_MAP = 0;
#ifdef HIGH_DETAIL
	addObject(DynamicObject::create("3ds\\characters\\I Robot female HD.3ds",0.024f,material,16));
#else
	addObject(DynamicObject::create("3ds\\characters\\I Robot female.3ds",0.24f,material,16)); // 20k
#endif

	// static: quake = 28k

	// ok otexturovane
	//dynaobject = DynamicObject::create("3ds\\characters\\ct\\crono.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds\\characters\\ct\\lucca.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds\\characters\\sven\\sven.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds\\characters\\potato\\potato01.3ds",0.004f); // ok
	//dynaobject = DynamicObject::create("3ds\\objects\\head\\head.3DS",0.004f); // ok. vytvari zeleny facy po koupelne
	//dynaobject = DynamicObject::create("3ds\\characters\\swatfemale\\female.3ds",0.02f); // spatne normaly na bocich
	//dynaobject = DynamicObject::create("3ds\\characters\\icop\\icop.3DS",0.04f); // spatne normaly na zadech a chodidlech
	//dynaobject = DynamicObject::create("3ds\\characters\\GokuGT_3DS\\Goku_GT.3DS",0.004f); // ok, ale jen nekomercne

	// ok neotexturovane
	//dynaobject = DynamicObject::create("3ds\\characters\\armyman2003.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds\\characters\\i robot female.3ds",0.04f); // ok
	//dynaobject = DynamicObject::create("3ds\\objects\\knife.3ds",0.01f); // ok
	//dynaobject = DynamicObject::create("3ds\\characters\\snowman.3ds",1); // spatne normaly zezadu
	//dynaobject = DynamicObject::create("3ds\\objects\\gothchocker.3ds",0.2f); // spatne normaly zezadu
	//dynaobject = DynamicObject::create("3ds\\objects\\rubic_cube.3ds",0.01f); // spatne normaly, ale pouzitelne
	//dynaobject = DynamicObject::create("3ds\\objects\\polyhedrons_ts.3ds",0.1f); // sesmoothovane normaly, chybi hrany
	//dynaobjectAI[1]->shuffle();
	*/
}

void DynamicObjects::addObject(DynamicObject* dynobj)
{
	dynaobject.push_back(dynobj);
	dynaobjectAI.push_back(new DynamicObjectAI());
}

const rr::RRVec3 DynamicObjects::getPos(unsigned objIndex)
{
	return (objIndex<dynaobject.size()) ? dynaobject[objIndex]->worldFoot : rr::RRVec3(0);
}

const rr::RRReal DynamicObjects::getRot(unsigned objIndex)
{
	return (objIndex<dynaobject.size()) ? dynaobject[objIndex]->rot : 0;
}

void DynamicObjects::setPos(unsigned objIndex, rr::RRVec3 worldFoot)
{
	if(objIndex<dynaobject.size())
	{
		if(dynaobject[objIndex])
		{
			dynaobjectAI[objIndex]->pos = worldFoot;
			dynaobject[objIndex]->worldFoot = worldFoot;
			//dynaobject[objIndex]->rot += 2.1f;
			dynaobject[objIndex]->updatePosition();
			assert(dynaobject[objIndex]->visible);
		}
	}
	else
	{
		assert(0);
	}
}

// copy animation data from frame to actual scene
void DynamicObjects::copyAnimationFrameToScene(const LevelSetup* setup, const AnimationFrame& frame, bool lightsChanged)
{
	if(lightsChanged)
	{
		light = frame.eyeLight[1];
		reportLightMovement();
	}
	eye = frame.eyeLight[0];
	reportEyeMovement();
	//for(AnimationFrame::DynaPosRot::const_iterator i=frame->dynaPosRot.begin();i!=frame->dynaPosRot.end();i++)
	for(unsigned i=0;i<dynaobject.size();i++)
	{
		if(dynaobject[i])
			dynaobject[i]->visible = false;
	}
	for(unsigned i=0;i<setup->objects.size();i++) // i = source object index in frame
	{
		unsigned j = setup->objects[i]; // j = destination object index in this
		if(i<frame.dynaPosRot.size() && j<dynaobject.size() && dynaobject[j])
		{
			dynaobject[j]->visible = true;
			dynaobject[j]->worldFoot = RRVec3(frame.dynaPosRot[i][0],frame.dynaPosRot[i][1],frame.dynaPosRot[i][2]);
			dynaobject[j]->rot = frame.dynaPosRot[i][3];
			dynaobject[j]->updatePosition();
			// copy changes to AI
			dynaobjectAI[j]->pos = dynaobject[j]->worldFoot;
		}
	}
}

// copy animation data from frame to actual scene
void DynamicObjects::copySceneToAnimationFrame_ignoreThumbnail(AnimationFrame& frame, const LevelSetup* setup)
{
	frame.eyeLight[0] = eye;
	frame.eyeLight[1] = light;
	frame.dynaPosRot.clear();
	for(unsigned sceneIndex=0;sceneIndex<setup->objects.size();sceneIndex++) // scene has few objects
	{
		unsigned demoIndex = setup->objects[sceneIndex]; // demo has more objects
		rr::RRVec4 tmp = rr::RRVec4(0);
		if(dynaobject[demoIndex])
			tmp = rr::RRVec4(dynaobject[demoIndex]->worldFoot,dynaobject[demoIndex]->rot);
		frame.dynaPosRot.push_back(tmp);
	}
	frame.validate(setup->objects.size());
}

// nastavi dynamickou scenu do daneho casu od zacatku animace
// pokud je cas mimo rozsah animace, neudela nic a vrati false
bool DynamicObjects::setupSceneDynamicForPartTime(LevelSetup* setup, float secondsFromPartStart)
{
	if(!setup)
	{
		return false; // no scene loaded
	}
	static AnimationFrame prevFrame;
	const AnimationFrame* frame = setup->getFrameByTime(secondsFromPartStart);
	if(!frame)
		return false;
	copyAnimationFrameToScene(setup,*frame,memcmp(&frame->eyeLight[1],&prevFrame.eyeLight[1],sizeof(de::Camera))!=0);
	prevFrame = *frame;
	return true;
}
/*
void DynamicObjects::updateSceneDynamic(LevelSetup* setup, float seconds, unsigned onlyDynaObjectNumber)
{
	// increment rotation
	float rot = seconds*70;

	// move objects
	bool lightsChanged = false;
	const AnimationFrame* frame = level ? level->pilot.autopilot(seconds,&lightsChanged) : NULL;
	if(frame)
	{
		// autopilot movement
		assert(onlyDynaObjectNumber==1000);
		copyAnimationFrameToScene(setup,*frame,lightsChanged);
	}
	else
	{
		// AI movement
		for(unsigned i=0;i<dynaobject.size();i++)
		{
			if(dynaobject[i] && (i==onlyDynaObjectNumber || onlyDynaObjectNumber>999))
			{
				dynaobject[i]->worldFoot = dynaobjectAI[i]->updatePosition(seconds);
				dynaobject[i]->rot += rot * ((i%2)?1:-1);
				dynaobject[i]->updatePosition();
			}
		}
	}
}
*/
void DynamicObjects::renderSceneDynamic(rr::RRRealtimeRadiosity* solver, de::UberProgram* uberProgram, de::UberProgramSetup uberProgramSetup, de::AreaLight* areaLight, unsigned firstInstance, de::Texture* lightDirectMap) const
{
	// use object space
	uberProgramSetup.OBJECT_SPACE = true;
	// use environment maps
	if(uberProgramSetup.LIGHT_INDIRECT_VCOLOR || uberProgramSetup.LIGHT_INDIRECT_MAP)
	{
		// indirect from envmap
		uberProgramSetup.SHADOW_MAPS = 1; // always use 1 shadowmap, detectMaxShadowmaps expects it and ignores dynaobjects
		//uberProgramSetup.SHADOW_SAMPLES = 1;
		uberProgramSetup.LIGHT_INDIRECT_CONST = 0;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 0;
		uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
		uberProgramSetup.LIGHT_INDIRECT_ENV = 1;
	}
	//dynaobject[4]->worldFoot = rr::RRVec3(2.2f*sin(d*0.005f),1.0f,2.2f);

	for(unsigned i=0;i<dynaobject.size();i++)
	{
		if(dynaobject[i] && dynaobject[i]->visible)
		{
			if(uberProgramSetup.LIGHT_INDIRECT_ENV)
				dynaobject[i]->updateIllumination(solver);
			dynaobject[i]->render(uberProgram,uberProgramSetup,areaLight,firstInstance,lightDirectMap,eye);
		}
	}
}

DynamicObject* DynamicObjects::getObject(unsigned objectIndex)
{
	return (objectIndex<dynaobject.size())?dynaobject[objectIndex]:NULL;
}

DynamicObjects::~DynamicObjects()
{
	for(unsigned i=0;i<dynaobject.size();i++)
	{
		delete dynaobject[i];
		delete dynaobjectAI[i];
	}
}
