#ifndef DYNAMICOBJECTS_H
#define DYNAMICOBJECTS_H

#include "Lightsprint/RRMath.h"
#include "Lightsprint/RRIllumination.h"
#include "AnimationFrame.h"
#include "DynamicObject.h"

/////////////////////////////////////////////////////////////////////////////
//
// Dynamic objects

class DynamicObjects
{
public:
	~DynamicObjects();
	
	// fills pool with objects
	void addObject(DynamicObject* dynobj);
	DynamicObject* getObject(unsigned objIndex);

	// actual positions in scene
	rr::RRVec3 getPos(unsigned objIndex) const;
	void setPos(unsigned objIndex, rr::RRVec3 worldFoot);
	rr::RRVec2 getRot(unsigned objIndex) const;
	void setRot(unsigned objIndex, rr::RRVec2 rot);

	// copy positions from animation frame to actual scene
	// returns true when object in scene changed position or rotation
	bool copyAnimationFrameToScene(const class LevelSetup* setup, const AnimationFrame& frame, bool lightsChanged);

	// copy positions from actual scene to animation frame
	void copySceneToAnimationFrame_ignoreThumbnail(AnimationFrame& frame, const LevelSetup* setup);

	// automaticky rotuje objekty, pouzij kdyz je benchmark pauznuty
	void advanceRot(float seconds);

	// muze byt zavolano po zahybani objekty a pred renderSceneDynamic()
	// aktualizuje triangleNumbers v dynobjektech, osvetleni jeste nemusi byt spoctene
	// zrychli prubeh nasledujiciho renderSceneDynamic
	// neni povinne
	void updateSceneDynamic(rr::RRDynamicSolver* solver);

	// dokonci update envmap a posle je do GPU
	// probehne rychleji pokud bylo predtim zavolano updateSceneDynamic, ale nutne to neni
	// camera must be already set in OpenGL, this one is passed only for frustum culling
	void renderSceneDynamic(rr::RRDynamicSolver* solver, rr_gl::UberProgram* uberProgram, rr_gl::UberProgramSetup uberProgramSetup, rr_gl::Camera* camera, const rr::RRVector<rr_gl::RealtimeLight*>* lights, unsigned firstInstance, const rr::RRVec4* brightness, float gamma, float waterLevel) const;

private:
	std::vector<DynamicObject*> dynaobject;
};

#endif
