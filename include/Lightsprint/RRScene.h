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
	//! but if it fails, second attempts are made in the same directory where scene file is.
	//!
	//! If RScefile contains information on units or up direction,
	//! our importers convert units to meters and up to Y.
	//! \param filename
	//!  Filename of scene. If it is NULL, scene will be empty.
	//! \param aborting
	//!  Import may be asynchronously aborted by setting *aborting to true.
	//! \param emissiveMultiplier
	//!  Multiplies emittance in all materials. Default 1 keeps original values.
	RRScene(const char* filename, bool* aborting = NULL, float emissiveMultiplier = 1);
	//! Saves 3d scene to file.
	//
	//! Scene save is attempted using savers registered via registerSaver().
	//! One saver is implemented in LightsprintIO library,
	//! rr_io::registerSavers() will register it for you.
	//! See rr_io::registerSavers() for details on formats/features supported.
	bool save(const char* filename);
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
	// Tools
	//////////////////////////////////////////////////////////////////////////////

	//! Transforms scene by given matrix (i.e. transforms all object matrices and lights).
	void transform(const RRMatrix3x4& transformation);
	//! Changes units to meters.
	//
	//! Importers already try to change units to meters, so use this only if automatic conversion fails.
	//! Using meters is not strictly necessary, global illumination works on all scales.
	void normalizeUnits(float currentUnitLengthInMeters);
	//! Changes up axist to Y.
	//
	//! Importers already try to change up to Y, so use this only if automatic conversion fails.
	//! Using up Y is not strictly necessary, global illumination works in all directions.
	//! \param currentUpAxis
	//!  0=X, 1=Y, 2=Z
	void normalizeUpAxis(unsigned currentUpAxis);


	//////////////////////////////////////////////////////////////////////////////
	// Loaders/Savers
	//////////////////////////////////////////////////////////////////////////////

	//! Template of custom scene loader.
	typedef RRScene* Loader(const char* filename, bool* aborting, float emissiveMultiplier);
	//! Template of custom scene saver.
	typedef bool Saver(const RRScene* scene, const char* filename);
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
	//! NULL is returned if no loaders were registered.
	static const char* getSupportedLoaderExtensions();
	//! Returns list of supported saver extensions in "*.dae;*.3ds;*.md5mesh" format.
	//
	//! All extensions of registered savers are returned in one static string, don't free() it.
	//! NULL is returned if no savers were registered.
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
