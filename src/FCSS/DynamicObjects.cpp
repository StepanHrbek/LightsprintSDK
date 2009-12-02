#include "DynamicObjects.h"
#include "AnimationScene.h"
#include "Lightsprint/RRCollider.h"

using namespace rr;

// access to current scene
extern AnimationFrame currentFrame;
void reportEyeMovement();
void reportLightMovement();
const rr::RRCollider* getSceneCollider();

// estimates where character foot is, moves it to given world coordinate, then rotates character around Y and Z axes
static void transformObject(rr::RRObject* object, rr::RRVec3 worldFoot, rr::RRVec2 rotYZ)
{
	if (!object)
		return;
	rr::RRVec3 mini,center;
	object->getCollider()->getMesh()->getAABB(&mini,NULL,&center);
	float sz = sin(RR_DEG2RAD(rotYZ[1]));
	float cz = cos(RR_DEG2RAD(rotYZ[1]));
	float sy = sin(RR_DEG2RAD(rotYZ[0]));
	float cy = cos(RR_DEG2RAD(rotYZ[0]));
	float mx = -center.x;
	float my = -mini.y;
	float mz = -center.z;
	rr::RRMatrix3x4 worldMatrix;
	worldMatrix.m[0][0] = cz*cy;
	worldMatrix.m[1][0] = sz*cy;
	worldMatrix.m[2][0] = -sy;
	worldMatrix.m[0][1] = -sz;
	worldMatrix.m[1][1] = cz;
	worldMatrix.m[2][1] = 0;
	worldMatrix.m[0][2] = cz*sy;
	worldMatrix.m[1][2] = sz*sy;
	worldMatrix.m[2][2] = cy;
	worldMatrix.m[0][3] = cz*cy*mx-sz*my+cz*sy*mz+worldFoot[0];
	worldMatrix.m[1][3] = sz*cy*mx+cz*my+sz*sy*mz+worldFoot[1];
	worldMatrix.m[2][3] = -sy*mx+cy*mz+worldFoot[2];
	object->setWorldMatrix(&worldMatrix);
	object->illumination.envMapWorldCenter = rr::RRVec3(
		center.x*worldMatrix.m[0][0]+center.y*worldMatrix.m[0][1]+center.z*worldMatrix.m[0][2]+worldMatrix.m[0][3],
		center.x*worldMatrix.m[1][0]+center.y*worldMatrix.m[1][1]+center.z*worldMatrix.m[1][2]+worldMatrix.m[1][3],
		center.x*worldMatrix.m[2][0]+center.y*worldMatrix.m[2][1]+center.z*worldMatrix.m[2][2]+worldMatrix.m[2][3]);
}

/////////////////////////////////////////////////////////////////////////////
//
// DynamicObjects

bool DynamicObjects::addObject(const char* filename, float scale)
{
	rr::RRScene* scene = new rr::RRScene(filename,scale);
	if (!scene->getObjects().size())
	{
		delete scene;
		return false;
	}
	bool aborting = false;
	push_back(rr::RRObject::createMultiObject(&scene->getObjects(),rr::RRCollider::IT_LINEAR,aborting,-1,-1,false,0,NULL));
	scenesToBeDeleted.push_back(scene);
	return true;
}

rr::RRVec3 DynamicObjects::getPos(unsigned objIndex) const
{
	if (objIndex>=size())
		return rr::RRVec3(0);
	RRObject* object = (*this)[objIndex];
	rr::RRVec3 mini,center;
	object->getCollider()->getMesh()->getAABB(&mini,NULL,&center);
	return object->getWorldMatrixRef().getTranslation() - rr::RRVec3(center.x,mini.y,center.z);
}

void DynamicObjects::setPos(unsigned objIndex, rr::RRVec3 worldFoot)
{
	if (objIndex<size())
	{
		RRObject* object = (*this)[objIndex];
		rr::RRVec3 mini,center;
		object->getCollider()->getMesh()->getAABB(&mini,NULL,&center);
		rr::RRMatrix3x4 worldMatrix = object->getWorldMatrixRef();
		worldMatrix.setTranslation(worldFoot - rr::RRVec3(center.x,mini.y,center.z));
		object->setWorldMatrix(&worldMatrix);
	}
	else
	{
		// v .ani je vic objektu nez v .cfg a tys sahnul na '1'..'9'
		RR_ASSERT(0);
	}
}
/*
rr::RRVec2 DynamicObjects::getRot(unsigned objIndex) const
{
	return (objIndex<size()) ? dynaobject[objIndex]->rotYZ : RRVec2(0);
}

void DynamicObjects::setRot(unsigned objIndex, rr::RRVec2 rot)
{
	if (objIndex<size())
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
*/

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
	for (unsigned i=0;i<setup->objects.size();i++) // i = source object index in frame
	{
		unsigned j = setup->objects[i]; // j = destination object index in this
		if (i<frame.dynaPosRot.size() && j<size() && (*this)[j])
		{
			rr::RRObject* dynaobjectj = (*this)[j];
			rr::RRMatrix3x4 wm1 = dynaobjectj->getWorldMatrixRef();
			transformObject(dynaobjectj, frame.dynaPosRot[i].pos, frame.dynaPosRot[i].rot);
			rr::RRMatrix3x4 wm2 = dynaobjectj->getWorldMatrixRef();
			float delta = 0;
			for(unsigned i=0;i<12;i++) delta += fabs(wm1.m[0][i]-wm2.m[0][i]);
			if (delta>0.001f) objMoved = true; // treshold is necessary, rounding errors make position slightly off
			//!!!however, this tresholding is dangerous
			//   make sure 10ms of slow motion is still >treshold, otherwise we just caused stuttering
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
		if (demoIndex<size() // but hand edit of .cfg may remove objects referenced from .ani, lets check it
			&& (*this)[demoIndex])
		{
			rr::RRObject* object = (*this)[demoIndex];
			rr::RRVec3 mini,center;
			object->getCollider()->getMesh()->getAABB(&mini,NULL,&center);
			rr::RRMatrix3x4 wm = object->getWorldMatrixRef();

			tmp.pos = wm.getTranslation()-rr::RRVec3(center.x,mini.y,center.z);
			tmp.rot = rr::RRVec2(RR_RAD2DEG(acos(wm.m[2][2])),RR_RAD2DEG(acos(wm.m[1][1])));

		}
		frame.dynaPosRot.push_back(tmp);
	}
	frame.validate((unsigned)setup->objects.size());
}
/*
void DynamicObjects::advanceRot(float seconds)
{
	for (unsigned i=0;i<size();i++)
	{
		RR_ASSERT(dynaobject[i]);
		dynaobject[i]->rotYZ[0] += 90*seconds;
		dynaobject[i]->updatePosition();
	}
}
*/
DynamicObjects::~DynamicObjects()
{
	for (unsigned i=0;i<size();i++)
	{
		delete (*this)[i];
	}
	for (unsigned i=0;i<scenesToBeDeleted.size();i++)
	{
		delete scenesToBeDeleted[i];
	}
	
}
