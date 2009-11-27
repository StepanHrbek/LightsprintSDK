#ifndef RRSCENE_H
#define RRSCENE_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRScene.h
//! \brief LightsprintCore | 3d scene and its import
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
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
	//! Loads 3d scene from file.
	//
	//! Scene load is attempted using loaders registered via registerLoader().
	//! Several loaders are implemented in LightsprintIO library,
	//! rr_io::registerLoaders() will register all of them for you.
	//! See rr_io::registerLoaders() for details on formats/features supported.
	//!
	//! Known loaders try to load all textures from proper paths specified by scene file,
	//! but if it fails, second attempts are made in the same directory where scene file is.
	//!
	//! \param filename
	//!  Filename of scene. If it is NULL, scene will be empty.
	//! \param scale
	//!  If it is .gsa/.3ds/.obj, geometry is scaled(multiplied) by scale.
	//!  These formats don't contain information about units or it is optional,
	//!  so different files may need different scale to convert to meters.
	//!  Scale is size of scene unit in meters.
	//! \param aborting
	//!  Import may be asynchronously aborted by setting *aborting to true.
	//! \param emissiveMultiplier
	//!  Multiplies emittance in all materials. Default 1 keeps original values.
	RRScene(const char* filename, float scale = 1, bool* aborting = NULL, float emissiveMultiplier = 1);
	//! Creates empty scene.
	RRScene();
	//! Deletes scene including all objects and lights.
	virtual ~RRScene();

	//! Returns collection of objects in scene.
	virtual const RRObjects& getObjects();
	//! Returns collection of lights in scene.
	virtual const RRLights& getLights();
	//! Returns scene environment, skybox.
	virtual const RRBuffer* getEnvironment();

	//! Template of custom scene loader.
	typedef RRScene* Loader(const char* filename, float scale, bool* aborting, float emissiveMultiplier);
	//! Registers scene loader so it can be used by RRScene constructor.
	//
	//! Extension is case insensitive, without dot, e.g. "3ds".
	//!
	//! Several loaders are implemented in LightsprintIO library,
	//! rr_io::registerLoaders() will register all of them for you (by calling this function several times).
	//!
	//! Multiple loaders may be registered, even for the same extension.
	//! If first loader fails to load scene, second one is tried etc.
	static void registerLoader(const char* extension, Loader* loader);
	//! Returns list of supported extensions, e.g. "*.dae;*.3ds".
	//
	//! All extensions of registered loaders are returned in one static string, don't free() it.
	//! NULL is returned if no loaders were registered.
	static const char* getSupportedExtensions();

protected:
	//! Created in RRScene subclass, deleted automatically in base RRScene destructor.
	RRObjects* objects;
	//! Created in RRScene subclass, deleted automatically in base RRScene destructor.
	RRLights* lights;
private:
	RRScene* implementation;
};

} // namespace rr

#endif
