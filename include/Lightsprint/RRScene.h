#ifndef RRSCENE_H
#define RRSCENE_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRScene.h
//! \brief LightsprintCore | 3d scene and its import
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2010
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRDynamicSolver.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Interface of 3d scene and its load from file.

//! 3d scene loaded from file.
class RR_API RRScene : public RRUniformlyAllocatedNonCopyable
{
public:

	//////////////////////////////////////////////////////////////////////////////
	// Load
	//////////////////////////////////////////////////////////////////////////////

	//! Creates empty scene.
	RRScene();
	//! Loads 3d scene from file.
	//
	//! Scene load is attempted using loaders registered via registerLoader().
	//! Loaders for ~35 formats (\ref supported_formats)
	//! are implemented in LightsprintIO library,
	//! rr_io::registerLoaders() will register all of them for you.
	//!
	//! Our loaders try to load all textures from proper paths specified by scene file,
	//! but if it fails, second attempts are made in the same directory where scene file is.
	//!
	//! \param filename
	//!  Filename of scene. If it is NULL, scene will be empty.
	//! \param scale
	//!  Scale is size of scene unit in meters.
	//!  Scene is automatically converted to meters, i.e. scaled(multiplied) by scale.
	//!  Default 1 keeps original units.
	//!  This is format specific option, some formats may ignore it.
	//! \param aborting
	//!  Import may be asynchronously aborted by setting *aborting to true.
	//! \param emissiveMultiplier
	//!  Multiplies emittance in all materials. Default 1 keeps original values.
	RRScene(const char* filename, float scale = 1, bool* aborting = NULL, float emissiveMultiplier = 1);
	//! Deletes scene including all objects and lights.
	virtual ~RRScene();


	//////////////////////////////////////////////////////////////////////////////
	// Scene
	//////////////////////////////////////////////////////////////////////////////

	//! Collection of objects in scene. May be freely modified, does not delete objects in destructor.
	RRObjects objects;
	//! Collection of lights in scene. May be freely modified, does not delete lights in destructor.
	RRLights lights;
	//! Scene environment, skybox. May be modified, but carefully, scene owns environment and deletes it in destructor, so delete old environment before setting new one.
	RRBuffer* environment;


	//////////////////////////////////////////////////////////////////////////////
	// Loaders
	//////////////////////////////////////////////////////////////////////////////

	//! Template of custom scene loader.
	typedef RRScene* Loader(const char* filename, float scale, bool* aborting, float emissiveMultiplier);
	//! Registers scene loader so it can be used by RRScene constructor.
	//
	//! Extensions are case insensitive, in "*.dae;*.3ds;*.md5mesh" format.
	//!
	//! Several loaders are implemented in LightsprintIO library,
	//! rr_io::registerLoaders() will register all of them for you (by calling this function several times).
	//!
	//! Multiple loaders may be registered, even for the same extension.
	//! If first loader fails to load scene, second one is tried etc.
	static void registerLoader(const char* extensions, Loader* loader);
	//! Returns list of supported extensions in "*.dae;*.3ds;*.md5mesh" format.
	//
	//! All extensions of registered loaders are returned in one static string, don't free() it.
	//! NULL is returned if no loaders were registered.
	static const char* getSupportedExtensions();

protected:
	//! Protected collection of objects in scene, owns objects, deletes them in destructor. 
	RRObjects* protectedObjects;
	//! Protected collection of lights in scene, owns lights, deletes them in destructor. 
	RRLights* protectedLights;
private:
	RRScene* implementation;
};

} // namespace rr

#endif
