//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RRScene.h
//! \brief LightsprintCore | 3d scene, collection of objects and lights
//----------------------------------------------------------------------------

#ifndef RRSCENE_H
#define RRSCENE_H

#include "RRSolver.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Interface of 3d scene and its load/save.

//! 3d scene.
class RR_API RRScene : public RRUniformlyAllocatedNonCopyable
{
public:

	//////////////////////////////////////////////////////////////////////////////
	// Load/Save
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
	//! but if it fails, additional attempts are made as specified by textureLocator.
	//! textureLocator tells also what to do when texture is not found at all
	//! (create stub texture or create material without texture?)
	//!
	//! If file contains information on units or up direction,
	//! our importers convert units to meters and up to Y.
	//! \param filename
	//!  Filename of scene. If it is empty, scene will be empty.
	//! \param textureLocator
	//!  Optional custom file locator, for finding texture files in unusual location.
	//!  nullptr = default locator will be used.
	//! \param aborting
	//!  Import may be asynchronously aborted by setting *aborting to true.
	RRScene(const RRString& filename, RRFileLocator* textureLocator = nullptr, bool* aborting = nullptr);
	//! Saves 3d scene to file.
	//
	//! Scene save is attempted using savers registered via registerSaver().
	//! One saver is implemented in LightsprintIO library,
	//! rr_io::registerSavers() will register it for you.
	//! See rr_io::registerSavers() for details on formats/features supported.
	//! \param filename
	//!  Name of file to be created.
	bool save(const RRString& filename) const;
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
	//! Collection of cameras in scene. May be freely modified.
	RRCameras cameras;


	//////////////////////////////////////////////////////////////////////////////
	// Tools
	//////////////////////////////////////////////////////////////////////////////

	//! Transforms scene by given matrix (i.e. transforms all object matrices and lights).
	//
	//! This function reads contents of scene from objects and lights, not protectedObjects and protectedLights,
	//! so if you call it from your RRScene constructor, fill objects and lights first.
	void transform(const RRMatrix3x4& transformation);
	//! Changes units to meters.
	//
	//! Using meters is not strictly necessary, global illumination works on all scales.
	//! However, it is recommended to import scenes in meters.
	//! This function reads contents of scene from objects and lights, not protectedObjects and protectedLights,
	//! so if you call it from your RRScene constructor, fill objects and lights first.
	void normalizeUnits(float currentUnitLengthInMeters);
	//! Changes up axist to Y.
	//
	//! Using up Y is not strictly necessary, global illumination works in all directions.
	//! However, it is recommended to import scenes so that Y is up.
	//! This function reads contents of scene from objects and lights, not protectedObjects and protectedLights,
	//! so if you call it from your RRScene constructor, fill objects and lights first.
	//! \param currentUpAxis
	//!  0=X, 1=Y, 2=Z
	void normalizeUpAxis(unsigned currentUpAxis);
	//! Inserts all buffers found in scene's materials, lights, environment and illumination layers into collection.
	//
	//! Can be used to gather all texture filenames, to pause all videos etc.
	//! \param buffers
	//!  In-out collection of buffers.
	//!  Unique input buffers are preserved, new buffers from scene are added to collection.
	//!  Ordering of input buffers may change and duplicates and nullptr buffers are removed.
	//! \param layers
	//!  Illumination from given layers will be gathered too.
	void getAllBuffers(RRVector<RRBuffer*>& buffers, const RRVector<unsigned>* layers) const;


	//////////////////////////////////////////////////////////////////////////////
	// Loaders/Savers
	//////////////////////////////////////////////////////////////////////////////

	//! Template of custom scene loader.
	typedef RRScene* Loader(const RRString& filename, RRFileLocator* textureLocator, bool* aborting);
	//! Template of custom scene saver.
	typedef bool Saver(const RRScene* scene, const RRString& filename);
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
	//! Similar to registerLoader().
	static void registerSaver(const char* extensions, Saver* saver);
	//! Returns list of supported loader extensions in "*.dae;*.3ds;*.md5mesh" format.
	//
	//! All extensions of registered loaders are returned in one static string, don't free() it.
	//! nullptr is returned if no loaders were registered.
	static const char* getSupportedLoaderExtensions();
	//! Returns list of supported saver extensions in "*.dae;*.3ds;*.md5mesh" format.
	//
	//! All extensions of registered savers are returned in one static string, don't free() it.
	//! nullptr is returned if no savers were registered.
	static const char* getSupportedSaverExtensions();

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
