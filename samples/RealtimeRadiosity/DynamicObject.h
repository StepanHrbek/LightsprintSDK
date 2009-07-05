// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"

//#define OPTIMIZED_RDO // create multiobject and render it..(disabled, no improvement detected)
#ifdef OPTIMIZED_RDO
	#include "Lightsprint/GL/RendererOfRRObject.h"
#endif


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

//! 3ds object packed with material settings, environment maps and position in world.
class DynamicObject
{
public:
	//! Creates dynamic object from .3ds file.
	static DynamicObject* create(const char* filename,float scale,rr_gl::UberProgramSetup material,unsigned gatherCubeSize,unsigned specularCubeSize);

	//! Updates object's worldMatrix and some illumination properties according to worldFoot position and rotYZ rotation.
	//! To be called each time object moves/rotates. At least once at the beginning.
	//! No need to call it when object is not visible, but it's cheap.
	void updatePosition();

	//! Renders object.
	//! No need to call it when object is not visible, it's expensive.
	void render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,const rr::RRVector<rr_gl::RealtimeLight*>* lights,unsigned firstInstance,const rr_gl::Camera& eye, const rr::RRVec4* brightness, float gamma, float waterLevel);

	~DynamicObject();

	// Object's interface for movement, freely changeable from outside.
	// Your changes are registered when you call updatePosition().
	rr::RRVec3 worldFoot; // position of object's foot in world
	rr::RRVec2 rotYZ; // degrees, y is rotated first, then z
	rr::RRReal animationTime; // controls object's procedural animation, used when ANIMATION_WAVE=true
	bool visible;

	// updated by you, call solver->updateEnvironmentMap(illumination) each frame
	rr::RRObjectIllumination* illumination;

protected:
	DynamicObject();
	class Model_3DS* model;
	rr::RRObjects* singleObjects;
	rr::RRObject* multiObject;
	rr_gl::UberProgramSetup material;
#ifdef OPTIMIZED_RDO
	rr_gl::RendererOfRRObject* rendererWithoutCache;
#else
	rr_gl::Renderer* rendererWithoutCache;
#endif
	rr_gl::Renderer* rendererCached;

	// updated by updatePosition
	float worldMatrix[16];
};

#endif
