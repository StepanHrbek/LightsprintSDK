//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RRSolver.h
//! \brief LightsprintCore | global illumination solver for dynamic scenes
//----------------------------------------------------------------------------

#ifndef RRSOLVER_H
#define RRSOLVER_H

#include <functional>
#include "RRVector.h"
#include "RRIllumination.h"
#include "RRObject.h"
#include "RRLight.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRSolver
	//! Global illumination solver for interactive applications.
	//
	//! Usage for interactive realtime visualizations and tools:
	//! Create one instance at the beginning of interactive session
	//! and load it with all static objects in scene.
	//! Call calculate() at each frame, it will spend some time
	//! by improving illumination.
	//! When scene changes, report it using report* methods.
	//!
	//! Usage for non interactive tools/precalculators is
	//! the same as for interactive applications, with these differences:
	//! - call updateLightmaps() or updateLightmap() with higher quality settings
	//!   for higher quality results
	//! - instead of rendering computed illumination,
	//!   save it to disk using RRBuffer::save()
	//!   or RRBuffer::save() functions.
	//!
	//! Custom access to GPU and your renderer is not implemented here.
	//! You may implement it in your RRSolver subclass
	//! or use RRRealtimeRadioosityGL, that implements GPU access using OpenGL 2.0.
	//!
	//! Sample RealtimeRadiosity shows both typical usage scenarios,
	//! rendering with realtime global illumination and precalculations.
	//!
	//! It is not allowed to create and use multiple instances at the same time.
	//!
	//! Thread safe: Partially, see functions for more details.
	//!  All updateXxxx() functions automatically use all cores.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRSolver: public RRUniformlyAllocatedNonCopyable
	{
	public:
		RRSolver();
		virtual ~RRSolver();


		//! Set colorSpace used by this scene i/o operations.
		//
		//! It switches solver inputs and outputs from physical scale to custom scale defined by colorSpace,
		//! e.g. screen colors. See RRColorSpace for details.
		//!
		//! In 99% of cases you want to set colorSpace before using solver as nearly all
		//! existing renderers and datasets use sRGB or other custom scale.
		//! Not setting colorSpace is allowed for future rendering pipelines working fully in physical scale.
		//! \param colorSpace
		//!  Color space for converting illumination from/to linear colors.
		//!  It will be used by all data input and output paths in RRSolver, if not specified otherwise.
		//!  Note that colorSpace is not adopted, you are still responsible for deleting it
		//!  when it's no longer needed.
		virtual void setColorSpace(const RRColorSpace* colorSpace);

		//! Returns colorSpace used by this scene i/o operations, set by setColorSpace().
		const RRColorSpace* getColorSpace() const;


		//! Sets environment around scene.
		//
		//! This is one of ways for light to enter solver; others are setLights(), setDirectIllumination(), emissive materials.
		//! Environment is rendered around scene and scene is illuminated by environment.
		//!
		//! By default, scene contains no environment, which is the same as black environment.
		//! \param environment0
		//!  LDR or HDR environment stored in cube map or in 2d texture with 360*180 degree panorama (equirectangular projection).
		//!  Buffer is not adopted, you are still responsible for deleting it when it's no longer needed.
		//! \param environment1
		//!  Optional second environment map, solver is able to work with blend of two environments, see setEnvironmentBlendFactor().
		//!  It's ok to mix LDR and HDR maps, 2d and cube maps, all combinations work.
		//!  Buffer is not adopted, you are still responsible for deleting it when it's no longer needed.
		//! \param angleRad0
		//! \param angleRad1
		void setEnvironment(RRBuffer* environment0, RRBuffer* environment1=nullptr, RRReal angleRad0=0, RRReal angleRad1=0);
		//! Returns scene environment set by setEnvironment().
		RRBuffer* getEnvironment(unsigned environmentIndex=0, RRReal* angleRad=nullptr) const;

		//! Sets environment blend factor, specifies how two environments are blended together.
		//
		//! \param blendFactor
		//!  Any value between 0 and 1, tells solver how to blend two environments.
		//!  0 = use only environment0, this is default.
		//!  1 = use only environment1.
		//!  Lightsprint realtime GI solvers and realtime renderer fully support blending,
		//!  offline GI solver works as if blendFactor is 0.
		void setEnvironmentBlendFactor(float blendFactor);
		//! Returns environment blend factor set by setEnvironment().
		float  getEnvironmentBlendFactor() const;


		//! Sets lights in scene, all at once.
		//
		//! This is one of ways how light enters solver, others are setEnvironment(), setDirectIllumination(), emissive materials.
		//!
		//! By default, scene contains no lights.
		//!
		//! Setting lights is important for offline GI and for direct illumination in realtime renderer.
		//! If you integrate Lightsprint realtime GI with third party renderer and you use solver only to calculate realtime indirect illumination,
		//! calling setLights() is not necessary; calling setDirectIllumination() instead gives solver necessary information.
		virtual void setLights(const RRLights& lights);

		//! Returns lights in scene, set by setLights().
		const RRLights& getLights() const;


		//! Sets custom irradiance for all static triangles in scene.
		//
		//! This is one of paths for light to enter solver, others are setLights(), setEnvironment(), emissive materials.
		//! Unlike the other paths, this one is usually called many times - whenever lighting changes.
		//!
		//! If you use rr_gl::RRSolverGL, setDirectIllumination() is called automatically from calculate().
		//!
		//! If you want to provide your own direct illumination data, for example when integrating Lightsprint realtime GI
		//! with third party renderer, use RRSolver and call setDirectIllumination() manually before calculate() whenever lighting changes.
		//! Note that functions that reset solver or change its type (setStaticObjects(), loadFireball() etc) might reset also direct illumination,
		//! so you need to set it again after such functions.
		//!
		//! \param perTriangleIrradianceRGBA
		//!  Array of average per-triangle direct-lighting irradiances in custom scale.
		//!  In other words, average triangle colors when direct lighting+shadows are applied,
		//!  but materials are not.
		//!  Format is RGBA8, i.e. first byte is red, second one is green, third one is blue, fourth one is ignored.
		//!
		//!  Length of array is getMultiObject()->getCollider()->getMesh()->getNumTriangles().
		//!  Order of values in array is defined by order of static triangles in scene,
		//!  first all triangles from static object 0, then all triangles from static object 1 etc.
		//!
		//!  Array must stay valid at least until next setDirectIllumination() call.
		//!  Array is not adopted+deleted, you are still responsible for deleting it.
		//!  You are free to set always the same array or each time different one,
		//!  performace is identical.
		//!
		//!  Setting nullptr is the same as setting array filled by zeroes, no custom irradiance.
		void setDirectIllumination(const unsigned* perTriangleIrradianceRGBA);

		//! Returns pointer previously passed to setDirectIllumination(), or nullptr if it was not set yet.
		const unsigned* getDirectIllumination();


		//! Illumination smoothing parameters.
		struct SmoothingParameters
		{
			//! Distance in world units. Vertices with lower or equal distance
			//! (and angle between vertex normals smaller than maxSmoothAngle) will be internally stitched into one vertex.
			//! Zero stitches only identical vertices, negative value generates no action.
			//! Non-stitched vertices at the same location create illumination discontinuity.
			//! \n Sideeffect: non-negative values protect solver against INF and NaN vertices.
			//! In ideal situation where geometry doesn't need stitching, doesn't contain NaNs and INFs
			//! and adapter doesn't split vertices (Collada adapter does),
			//! set negative value to make calculation faster.
			float vertexWeldDistance;
			//! Angle in radians, vertices are stitched and illumination smoothed only if difference
			//! between vertex normals is <=abs(maxSmoothAngle).
			float maxSmoothAngle;
			//! Makes needle-like triangles with equal or smaller angle (rad) ignored.
			//! Default 0 removes only completely degenerated triangles.
			//! 0.001 is a reasonable value to put extremely needle like triangles off calculation,
			//! which may help in some situations.
			float ignoreSmallerAngle;
			//! Makes smaller and equal size triangles ignored.
			//! Default 0 removes only completely degenerated triangles.
			//! For typical game interior scenes and world in 1m units,
			//! 1e-10 is a reasonable value to put extremely small triangles off calculation,
			//! which may help in some situations.
			float ignoreSmallerArea;
			//! Sets default values at creation time.
			SmoothingParameters()
			{
				vertexWeldDistance = 0; // weld/smooth if vertex positions are identical
				maxSmoothAngle = 0.01f; // weld/smooth if vertex normals differ <=0.01rad
				ignoreSmallerAngle = 0; // ignores degerated triangles
				ignoreSmallerArea = 0; // ignores degerated triangles
			}
		};

		//! Sets static contents of scene, all static objects at once. Only objects with !RRObject::isDynamic are taken, the rest is ignored.
		//
		//! Solver creates internal set of static objects, by taking only those of your objects with isDynamic=false,
		//! you can query accepted objects by getStaticObjects(). Solver preserves your order of static objects.
		//! Any reference to n-th object in documentation refers to getStaticObjects()[n]. If you pass only static objects,
		//! then getStaticObjects()[n]==objects[n].
		//! Solver reads isDynamic only in this function; if you change it later, it won't have any effect.
		//!
		//! Once set, triangles and vertices in static objects must not change (that's the difference from dynamic objects).
		//! If you modify static triangles or vertices anyway, call setStaticObjects() again otherwise solver might crash.
		//!
		//! Scene must always contain static objects.
		//! Major occluders (buildings, large furniture etc) should be part
		//! of static scene.
		//! Handling major occluders as dynamic objects is safe,
		//! but it reduces realism of realtime indirect illumination, so it is not recommended.
		//!
		//! If Fireball was enabled, setStaticObjects() disables it.
		//!
		//! \param objects
		//!  Static contents of your scene, set of static objects.
		//!  \n Objects are passed via RRObjects, collection of pointers to objects.
		//!     Solver creates its own copy of these pointers, but it does not copy actual objects.
		//!     So while your collection is no longer accessed after this call,
		//!     actual objects are, you must not deallocate or modify them
		//!     until next setStaticObjects() call or solver destruction.
		//!     Changing objects, meshes, positions, colors etc while they are set in solver could crash program.
		//!     Changing illumiation stored in objects is safe.
		//! \param smoothing
		//!  Static scene illumination smoothing.
		//!  Set nullptr for default values.
		//! \param cacheLocation
		//!  Whether and where to cache colliders (speeds up startup when opening the same geometry next time).
		//!  It is passed to RRCollider::create(), so
		//!  default nullptr caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		//! \param intersectTechnique
		//!  Intersection technique used by solver. Techniques differ by speed and memory requirements.
		//! \param copyFrom
		//!  Should stay nullptr (used by sceneViewer to reuse multiObject and smoothing from old solver).
		virtual void setStaticObjects(const RRObjects& objects, const SmoothingParameters* smoothing, const char* cacheLocation=nullptr, RRCollider::IntersectTechnique intersectTechnique=RRCollider::IT_BVH_FAST, RRSolver* copyFrom = nullptr);
		//! Returns static contents of scene, all static objects at once. It is very fast, returning reference to existing collection.
		const RRObjects& getStaticObjects() const;

		//! Sets dynamic contents of scene, all dynamic objects at once. Only objects with RRObject::isDynamic are taken, the rest is ignored.
		//
		//! Solver creates internal set of dynamic objects, by taking only those of your objects with isDynamic=true,
		//! you can query accepted objects by getDynamicObjects(). Solver preserves your order of dynamic objects.
		//! If you pass only dynamic objects, then getDynamicObjects()[n]==objects[n].
		//! Solver reads isDynamic only in this function; if you change it later, it won't have any effect.
		//!
		//! Unlike static objects, you may freely edit dynamic objects between frames.
		//! If you want to add or remove objects, call setDynamicObjects() again. If you only edit object properties,
		//! there's no need to call it again.
		//!
		//! Dynamic objects affect realtime lighting, they are ignored when building static lightmaps.
		//! API does not even let you build lightmaps for dynamic objects, as it does not make big sense,
		//! dynamic objects are better illuminated by diffuse and specular reflection maps, see updateEnvironmentMap().
		//! If you really have to bake lightmap for dynamic object, make the object static first; if you also need the object
		//! to have no effect on other static objects (no shadows etc), but still receive light/shadows from
		//! other static objects, make the object transparent during lightmap baking (via material->specularTransmittance).
		void setDynamicObjects(const RRObjects& objects);
		//! Returns dynamic contents of scene, all dynamic objects at once. It is very fast, returning reference to existing collection.
		const RRObjects& getDynamicObjects() const;

		//! Returns collection of all objects in scene. It is slower than getStaticObjects() and getDynamicObjects(), returning newly allocated collection.
		RRObjects getObjects() const;

		//! Returns given object or nullptr for out of range index. Objects are indexed from 0, static objects go first, then dynamic.
		RRObject* getObject(unsigned index) const;


		//! Returns collider for whole scene, both static and dynamic.
		//
		//! Never returns nullptr, even if scene is empty.
		//! Returned collider is valid until next getCollider() call. It is owned by solver, you don't delete it.
		//!
		//! This is slightly heavier operation as it updates acceleration structures.
		//! Therefore recommended usage is to call it once, use returned collider many times (ideally until scene changes)
		//! rather than getting collider for each ray.
		//!
		//! When a dynamic object moves, call reportDirectIlluminationChange(-1,...) to update illumination.
		//! As a sideeffect, solver marks collider dirty and builds new one next time you call getCollider().
		//! If you don't get correct intersections, verify that reportDirectIlluminationChange(-1,...) is called after object movement.
		//!
		//! Thread safe: no, you must not use solver or modify objects while getCollider() executes.
		//! Returned collider is thread safe as usual, many threads can calculate intersections at once.
		//!
		//! Returned collider does not necessarily know about objects, so ray.hitObjects is not necessarily set after intersection.
		//! If you need to know what single object was intersected, follow instructions
		//! - before calling intersect() on this collider, do ray.hitObject = solver->getMultiObject();
		//! - when intersect() on this collider returns true, do ray.convertHitFromMultiToSingleObject(solver);
		//!
		//! If you can afford to ignore dynamic objects, collide with static objects only, it's faster.
		//! To collide with static objects only, use getMultiObject()->getCollider().
		RRCollider* getCollider() const;

		//! Fills bounding box of all objects in solver.
		//
		//! \return Some planar dynamic objects (like ocean surface) would make box very large, so we exclude them from calculation.
		//!  In such case, pointer to one of excluded planes is returned.
		const RRObject* getAABB(RRVec3* _mini, RRVec3* _maxi, RRVec3* _center) const;


		//! Does something once for each buffer in solver's materials, lights, environment and illumination layers.
		//
		//! Can be used to gather all texture filenames, to pause all videos etc.
		//! \param layers
		//!  Illumination from given layers will be processed too.
		//! \param func
		//!  Code to run for each buffer once.
		void processBuffers(const RRVector<unsigned>* layers, std::function<void (RRBuffer*)> func) const;


		//! Multipliers for tweaking contribution of various light sources. They work in physical scale.
		struct RR_API Multipliers
		{
			//! Multiplies illumination from RRLights.
			RRReal lightMultiplier;
			//! Multiplies environment(skybox) illumination.
			RRReal environmentMultiplier;
			//! Multiplies emittance of emissive materials.
			RRReal materialEmittanceMultiplier;

			Multipliers()
			{
				lightMultiplier = 1;
				environmentMultiplier = 1;
				materialEmittanceMultiplier = 1;
			}
			bool operator ==(const Multipliers& a) const;
			bool operator !=(const Multipliers& a) const;
			Multipliers operator *(const Multipliers& a) const;
		};

		//! Optional parameters of calculate().
		struct RR_API CalculateParameters : public Multipliers
		{
			//! Only for Fireball solver:
			//! Specifies what to do when emissive texture changes.
			//!  - 0 disables updates, existing lighting stays unchanged.
			//!  - 1 = max speed, flat emittance colors stored in materials are used.
			//!    All triangles that share the same material emit the same average color.
			//!    Eventual changes in textures and non-uniform distribution of colors in texture are ignored.
			//!  - 2 or more = higher precision, emissive textures are sampled,
			//!    quality specifies number of samples per triangle.
			//!    Triangles emit their average emissive colors and changes in textures are detected.
			unsigned materialEmittanceStaticQuality;
			//! Only for Fireball solver:
			//! Like materialEmittanceStaticQuality, applied when there is video in emissive texture.
			unsigned materialEmittanceVideoQuality;

			//! Only for Fireball solver:
			//! For materialEmittanceQuality=0, this parameter is ignored.
			//! For materialEmittanceQuality>0, two paths exist
			//!  - false = Fast direct access to material's emissive texture. Recommended.
			//!    At quality 16, it is roughly 5x slower than quality 0. If you haven't overloaded getPointMaterial(),
			//!    both paths produce identical results, but this one is faster.
			//!  - true = Slow access via customizable virtual function getPointMaterial().
			//!    At quality 16, it is roughly 100x slower than quality 0.
			//!    Use it only if you need your overloaded getPointMaterial() to be used.
			bool materialEmittanceUsePointMaterials;

			//! Only for Fireball and Architect solvers:
			//! Specifies what to do when transmittance texture changes.
			//!  - 0 = no updates, existing lighting stays unchanged.
			//!  - 1 = update only shadows.
			//!  - 2 = update full GI.
			unsigned materialTransmittanceStaticQuality;
			//! Only for Fireball and Architect solvers:
			//! Like materialTransmittanceStaticQuality, applied when there is video in transmittance texture.
			unsigned materialTransmittanceVideoQuality;

			//! Only for Fireball solver:
			//! Specifies what to do when environment changes.
			//! Quality of lighting from environment, number of samples.
			//! 0 disables updates, existing lighting stays unchanged; so if you always pass 0, environment will not illuminate scene.
			unsigned environmentStaticQuality;
			//! Only for Fireball solver:
			//! Like environmentStaticQuality, applied when there is video in environment texture.
			unsigned environmentVideoQuality;

			//! Only for Fireball solver:
			//! Quality of indirect lighting when direct lighting changes.
			//! 1..20, default is 3.
			//! Higher quality makes calculate() take longer.
			unsigned qualityIndirectDynamic;

			//! Only for Fireball solver:
			//! Target quality of indirect lighting when direct lighting doesn't change.
			//! 1..1000, default is 3.
			//! Higher quality doesn't make calculate() take longer, but indirect lighting
			//! improves in several consecutive calculate()s when direct lighting doesn't change.
			unsigned qualityIndirectStatic;

			//! Only for RRSolverGL:
			//! For how many frames indirect illumination can trail behind direct illumination.
			//! 0 = indirect synchronized with direct, highest quality.
			//! 3 = indirect slightly delayed behind direct, faster.
			unsigned delayDDI;

			//! Only for RRSolverGL:
			//! For how many seconds indirect illumination can stay unchanged.
			//! 0 = update in each frame, highest quality.
			//! 0.05 = update less frequently, faster.
			float secondsBetweenDDI;

			//! Experimental, RRSolverGL::calculate() does not call RRSolver::calculate().
			bool skipRRSolver;

			//! Sets default parameters. This is used if you send nullptr instead of parameters.
			CalculateParameters()
			{
				materialEmittanceStaticQuality = 17;
				materialEmittanceVideoQuality = 5;
				materialEmittanceUsePointMaterials = false;
				materialTransmittanceStaticQuality = 2;
				materialTransmittanceVideoQuality = 2;
				environmentStaticQuality = 6000;
				environmentVideoQuality = 300;
				qualityIndirectDynamic = 3;
				qualityIndirectStatic = 3;
				delayDDI = 0;
				secondsBetweenDDI = 0;
				skipRRSolver = false;
			}
			bool operator ==(const CalculateParameters& a) const;
		};

		//! Calculates and improves indirect illumination on static objects.
		//
		//! Call this function once per frame while rendering realtime GI. You can call it even when
		//! not rendering, to improve GI solution inside solver (so that you have it ready when you render scene later).
		//! On the other hand, don't call it when you don't need realtime GI,
		//! for example when rendering offline baked lightmaps, it would still spend time calculating realtime GI even if you don't need it.
		//!
		//! It helps to improve performance if
		//! you don't repeatedly render scene that doesn't change, for example in game editor.
		//! Note that user can't see any difference, you only save time and power 
		//! by not rendering the same image again.
		//! calculate() uses these periods for more intense calculations,
		//! creating higher quality illumination.
		//! It detects these periods from you not calling reportDirectIlluminationChange() and reportInteraction().
		//!
		//! For games, it is recommended to call loadFireball() before first calculate().
		//! Without fireball, first calculate() takes longer, solver internals are initialized.
		//!
		//! \param params
		//!  Optional calculation parameters. Currently used only by Fireball.
		virtual void calculate(const CalculateParameters* params = nullptr);

		//! Returns version of global illumination solution.
		//
		//! You may use this number to avoid unnecessary updates of illumination buffers.
		//! Store version together with your illumination buffers and don't update them
		//! (updateLightmaps()) until this number changes.
		//!
		//! Version may be incremented only by calculate().
		unsigned getSolutionVersion() const;


		//! Parameters for updateLightmap(), updateLightmaps().
		//
		//! See two constructors for default realtime and default offline parameters.
		//!
		//! While some light types can be disabled here, light from emissive materials always enters calculation.
		//!
		//! If you use \ref calc_fireball, only default realtime parameters are supported,
		//! use nullptr for default parameters.
		struct RR_API UpdateParameters
		{
			//! Multiplies direct illumination from sources.
			Multipliers direct;
			//! Multiplies indirect illumination from sources. Set both direct and indirect identical for realistic results.
			Multipliers indirect;

			//! Reuse indirect illumination already calculated and stored in solver?
			//
			//! Calls to updateLightmaps() or calculate() leave calculated indirect illumination stored in solver.
			//! Following calls to updateLightmaps() can save time by reusing current solution instead of calculating it from scratch.
			//! Note that when reusing current solution, majority of indirect light is already mixed in solver,
			//! using indirect multipliers sent to previous calls, so resuing solution with different multipliers has only limited effect.
			bool useCurrentSolution;

			//! Quality of computed illumination, 0 for realtime.
			//
			//! Relates to number of rays per texel or triangle,
			//! time taken grows mostly linearly with this number.
			//!
			//! Higher number = higher quality.
			//! 1000 is usually sufficient for production, with small per pixel details
			//! and precise antialiasing computed.
			//! Lower quality is good for tests, with per pixel details, but with artifacts
			//! and aliasing.
			unsigned quality;

			//! Adjusts quality in radiosity step, small part of whole calculation.
			//
			//! Time spent in all steps, including radiosity, is set automatically based on single 'quality' parameter.
			//! However, it's possible to tweak time spent in radiosity step,
			//! value x makes radiosity step x times longer. Example: 0.5 makes it 2x faster.
			float qualityFactorRadiosity;

			//! Use bump maps, when available. It makes lightmaps more detailed, but calculation is bit slower.
			bool useBumpMaps;

			//! Higher value makes indirect illumination in corners darker, 0=disabled/lighter, default 1=normal, 2=darker.
			RRReal aoIntensity;
			//! Indirect illumination gets darker in this distance (in world units, usually meters) from corners.
			//! Default 0=disabled.
			//! If set too high, indirect illumination becomes black.
			RRReal aoSize;

			//! Deprecated. Only partially supported since 2007.08.21.
			//
			//! 0..1 ratio, texels with greater fraction of hemisphere 
			//! seeing inside objects (or below rug, see rugDistance)
			//! are masked away.
			//! Default value 1 disables any correction.
			RRReal insideObjectsThreshold;

			//! Deprecated. Only partially supported since 2007.08.21.
			//
			//! Distance in world space, illumination coming from closer surfaces is masked away.
			//! Set it slightly above distance of rug and ground, to prevent darkness
			//! under the rug leaking half texel outside (instead, light around rug will
			//! leak under the rug). Set it to zero to disable any corrections.
			RRReal rugDistance;

			//! Distance in world space; reflected or emited light never comes from greater distance.
			//
			//! As long as it is bigger than scene size, results are realistic.
			//! Setting it below scene size makes results less realistic, illumination
			//! gets increasingly influenced by outer environment/sky instead of scene.
			//! (Rays are shot from texel into scene. When scene is not intersected
			//! in this or lower distance from texel, illumination is read from 
			//! outer environment/sky.)
			RRReal locality;

			//! For internal use only, don't change default RM_IRRADIANCE_CUSTOM_INDIRECT value.
			RRRadiometricMeasure measure_internal;

			//! For debugging only, to be described later.
			unsigned debugObject;
			//! For debugging only, to be described later.
			unsigned debugTexel;
			//! For debugging only, to be described later. (multiObjPostImport)
			unsigned debugTriangle;
			//! For debugging only, to be described later.
			void (*debugRay)(const RRRay& ray, bool hit);


			//! Sets default parameters for fast realtime update, using current solution from solver.
			UpdateParameters()
			{
				direct.lightMultiplier = 0;
				direct.environmentMultiplier = 0;
				direct.materialEmittanceMultiplier = 0;
				indirect.lightMultiplier = 0;
				indirect.environmentMultiplier = 0;
				indirect.materialEmittanceMultiplier = 0;
				useCurrentSolution = true;
				quality = 0;
				qualityFactorRadiosity = 1;
				useBumpMaps = true;
				aoIntensity = 1;
				aoSize = 0;
				insideObjectsThreshold = 1;
				rugDistance = 0.001f;
				locality = 100000;
				measure_internal = RM_IRRADIANCE_CUSTOM_INDIRECT;
				debugObject = UINT_MAX;
				debugTexel = UINT_MAX;
				debugTriangle = UINT_MAX;
				debugRay = nullptr;
			}
			//! Sets default parameters for complete baking, with all light sources enabled.
			UpdateParameters(unsigned _quality)
			{
				useCurrentSolution = false;
				quality = _quality;
				qualityFactorRadiosity = 1;
				useBumpMaps = true;
				insideObjectsThreshold = 1;
				rugDistance = 0.001f;
				locality = 100000;
				aoIntensity = 1;
				aoSize = 0;
				measure_internal = RM_IRRADIANCE_CUSTOM_INDIRECT;
				debugObject = UINT_MAX;
				debugTexel = UINT_MAX;
				debugTriangle = UINT_MAX;
				debugRay = nullptr;
			}
			bool operator ==(const UpdateParameters& a) const;
		};

		//! Parameters of filtering in updateLightmap()/updateLightmaps().
		struct RR_API FilteringParameters
		{
			//! Amount of smoothing applied when baking lightmaps.
			//! Hides unwrap seams, makes edges smoother, reduces noise, but washes out tiny details.
			//! Reasonable values are around 1. 0=off.
			float smoothingAmount;
			//! Distance in pixels, how deep foreground (used) colors spread into background (unused) regions.
			//! Zero is theoretically ok for lightmapping with bilinear interpolation, GPU should never sample from unused pixels.
			//! However, some GPU antialiasing modes read samples from unused regions anyway, so in order to hide this problem,
			//! it's good to fill lightmap background with nearby foreground colors (which is done by default).
			unsigned spreadForegroundColor;
			//! Color of unused background pixels. In color space of destination buffer.
			RRVec4 backgroundColor;
			//! Smooth colors between opposite borders.
			//! Generally, enable wrap if lightmap is to be later applied with wrapping enabled.
			bool wrap;
			//! Sets default parameters.
			FilteringParameters()
			{
				smoothingAmount = 1;
				spreadForegroundColor = 1000;
				backgroundColor = RRVec4(0);
				wrap = false;
			}
			bool operator ==(const FilteringParameters& a) const;
		};

		//! For single static object, calculates and updates lightmap and/or bent normals; in per-pixel or per-vertex; with direct, indirect or global illumination.
		//
		//! This is universal update for both per-pixel and per-vertex buffers.
		//! Type and format of data produced depends only on type and format of buffer you provide.
		//! Combinations like per-pixel colors, per-vertex bent normals are supported too.
		//! Format of buffer is preserved.
		//!
		//! For 2d texture buffer (lightmap, bentNormalMap), uv channel material->lightmap.texcoord is used.
		//! All uv coordinates must be in 0..1 range and two triangles
		//! must not overlap in texture space.
		//! If it's not satisfied, contents of created lightmap is undefined.
		//!
		//! Thread safe: no, but there's no need to run it from multiple threads at the same time,
		//!   all cores are used automatically.
		//!
		//! \param objectNumber
		//!  Number of object in this scene.
		//!  Object numbers are defined by order in which you pass objects to setStaticObjects().
		//!  In case of vertex buffer, -1 is allowed for multiobject with whole static scene.
		//! \param lightmap
		//!  Buffer for storing calculated illumination.
		//!  \n May be nullptr.
		//!  \n Types supported: BT_VERTEX_BUFFER, BT_2D_TEXTURE, however with \ref calc_fireball,
		//!     only vertex buffer is supported.
		//!  \n Formats supported: All color formats, RGB, RGBA, bytes, floats.
		//!     Data are converted to format of buffer.
		//!  \n Lightmap could contain direct, indirect or global illumination, depending on
		//!     parameters you set in params.
		//! \param directionalLightmap
		//!  Pointer to array of three lightmaps for storing calculated directional illumination.
		//!  \n Compatible with Unreal Engine 3 directional lightmaps.
		//!  \n May be nullptr.
		//!  \n Supports the same types and formats as parameter 'lightmap'.
		//! \param bentNormals
		//!  Buffer for storing calculated bent normals, compact representation of directional information.
		//!  \n May be nullptr.
		//!  \n RGB values (range 0..1) are calculated from XYZ worldspace normalized normals
		//!     (range -1..1) by this formula: (XYZ+1)/2.
		//!  \n Supports the same types and formats as parameter 'lightmap'.
		//! \param params
		//!  Parameters of the update process. Set nullptr for default parameters that
		//!  specify very fast realtime/preview update.
		//! \param filtering
		//!  Parameters of lightmap filtering, set nullptr for default ones.
		//! \return
		//!  Number of lightmaps and bent normal maps updated, 0 1 or 2.
		//!  If it's lower than you expect, read system messages (RRReporter) for more details on possible failure.
		//! \remarks
		//!  In comparison with more general updateLightmaps() function, this one
		//!  lacks paramsIndirect. However, you can still include indirect illumination
		//!  while updating single lightmap, see updateLightmaps() remarks.
		virtual unsigned updateLightmap(int objectNumber, RRBuffer* lightmap, RRBuffer* directionalLightmap[3], RRBuffer* bentNormals, const UpdateParameters* params, const FilteringParameters* filtering=nullptr);

		//! For all static objects, calculates and updates lightmap and/or bent normal; in per-pixel or per-vertex; with direct, indirect or global illumination.
		//
		//! This is more powerful full scene version of single object's updateLightmap().
		//!
		//! Usage:
		//! -# create buffers of arbitrary types and formats
		//!   (per-pixel, per-vertex, bytes, floats, rgb, rgba, physical scale, custom scale)
		//!   and store them to getStaticObjects()[objectNumber]->illumination->getLayer(layerIndex)
		//! -# call updatelightmaps()
		//! -# enjoy buffers with computed lighting, you can do buffer->save(), buffer->lock(), renderer->render()...
		//!
		//! For 2d texture buffer (lightmap, bentNormalMap), uv channel material->lightmap.texcoord is used.
		//! All uv coordinates must be in 0..1 range and two triangles
		//! must not overlap in texture space.
		//! If it's not satisfied, contents of created lightmap is undefined.
		//!
		//! Lightmap update is one of the most demanding functions in Lightsprint SDK.
		//! Its \ref of_speed "time complexity" can be roughly predicted.
		//!
		//! Thread safe: no, but there's no need to run it from multiple threads at the same time,
		//!   all cores are used automatically.
		//!
		//! \param layerLightmap
		//!  1 lightmap per object will be computed into existing buffers in this layer,
		//!  getStaticObjects()[objectNumber]->illumination->getLayer(layerNumber).
		//!  \n Negative number disables update of lightmaps.
		//! \param layerDirectionalLightmap
		//!  3 directional lightmaps per object will be computed into existing buffers in this layer and two successive layers,
		//!  getStaticObjects()[objectNumber]->illumination->getLayer(layerNumber).
		//!  \n Negative number disables update of directional lightmaps.
		//! \param layerBentNormals
		//!  Bent normals will be computed into existing buffers in this layer,
		//!  getStaticObjects()[objectNumber]->illumination->getLayer(layerBentNormals).
		//!  \n Negative number disables update of bent normals.
		//! \param params
		//!  Parameters of the update process.
		//!  With nullptr or quality=0, update is realtime, but options are limited, 
		//!  only vertex buffers are filed with indirect illumination in physical scale, read from current solution in solver.
		//!  With quality>0, you get much more flexibility, but update is non-realtime.
		//! \param filtering
		//!  Parameters of lightmap filtering, set nullptr for default ones.
		//! \return
		//!  Number of lightmaps updated.
		//!  If it's lower than you expect, read system messages (RRReporter) for more details on possible failure.
		//! \remarks
		//!  As a byproduct of calculation with indirect ilumination, internal state of solver (current solution)
		//!  is updated, so that it holds computed indirect illumination for sources
		//!  and quality specified in params.
		//!  Internal state is properly updated even when buffers don't exist (so no other output is produced).
		//!  Following updateLightmap() will include this indirect lighting into computed buffer
		//!  if you call it with params->useCurrentSolution=true.
		//! \remarks
		//!  Update of selected objects (rather than all objects) is supported in multiple ways, use one of them.
		//!  All three ways produce the same quality, but first one may be faster in some cases.
		//!  - create buffers for selected objects, make sure other buffers are nullptr and call updateLightmaps()
		//!  - if you don't need indirect illumination, simply call updateLightmap() for all selected objects
		//!  - call updateLightmaps(-1,-1,params,nullptr) once to update current solution,
		//!    call updateLightmap(params with useCurrentSolution=true) for all selected objects
		//! \remarks
		//!  Sharing one lightmap by multiple objects is not supported out of the box. Please consult us for possible solutions.
		virtual unsigned updateLightmaps(int layerLightmap, int layerDirectionalLightmap, int layerBentNormals, const UpdateParameters* params, const FilteringParameters* filtering);
		
		//! Makes other solver functions abort, returning quickly with bogus results.
		//
		//! You may set/unset it asynchronously, from other threads.
		//! Solver only reads it, never modifies it, so don't forget to clear it after abort.
		bool aborting;

		//! Calculates and updates object's environment map, stored in given layer of given illumination.
		//
		//! Function updates existing buffer, it does nothing if buffer does not exist.
		//! You can allocate buffers
		//! - automatically, by allocateBuffersForRealtimeGI()
		//! - or manually by illumination->getLayer() = RRBuffer::create()
		//!
		//! Generated environment can be used for illumination of both static and dynamic objects.
		//!
		//! Function is fast for low resolution cubemaps, suitable for use in realtime applications.
		//! It is safe to call it often, it returns quickly if it detects that illumination is already up to date.
		//! For high resolution (more than approximately 32x32x6) envmaps,
		//! consider calling RRSolverGL::updateEnvironmentMap() instead, it is faster.
		//!
		//! Function reads static scene illumination, so don't call it before calculate(), otherwise
		//! dynamic objects will reflect light from previous frame, or no light at all.
		//! If you have to update envmaps before calculate(), use RRSolverGL::updateEnvironmentMap().
		//!
		//! Thread safe: yes, may be called from multiple threads at the same time
		//!  (but there's no need as it uses all cores internally)
		//!
		//! \param illumination
		//!  Object's illumination to be updated.
		//!  (It's not necessary to have RRObject adapter for dynamic object, but its RRObjectIllumination must exist.)
		//! \param layerEnvironment
		//!  Number of layer with environment maps, they are addressed by illumination->getLayer(layerEnvironment).
		//! \param layerLightmap
		//!  Number of layer with lightmaps, they are addressed by illumination->getLayer(layerLightmap).
		//!  Unused by default implementation.
		//! \param layerAmbientMap
		//!  Number of layer with ambient maps, they are addressed by illumination->getLayer(layerAmbientMap).
		//!  Unused by default implementation.
		//!  While default implementation reads illumination directly from solver,
		//!  access to lightmaps and/or ambient maps allows other implementations (rr_gl::RRSolverGL::updateEnvironmentMap())
		//!  to read illumination from lights and these maps instead.
		//! \return
		//!  Number of environment maps updated, 0 or 1.
		virtual unsigned updateEnvironmentMap(RRObjectIllumination* illumination, unsigned layerEnvironment, unsigned layerLightmap, unsigned layerAmbientMap);


		//! Reads illumination of triangle's vertex in units given by measure.
		//
		//! Reads results in format suitable for fast vertex based rendering without subdivision.
		//! \param triangle Index of triangle in multiobject you want to get results for.
		//! \param vertex Index of triangle's vertex you want to get results for. Valid vertices are 0, 1, 2.
		//!  For invalid vertex number, average value for whole triangle is taken instead of smoothed value in vertex.
		//! \param measure
		//!  Specifies what to measure, using what units.
		//! \param out
		//!  For valid inputs, illumination level is stored here. For invalid inputs, nothing is changed.
		//! \return
		//!  True if out was successfully filled. False may be caused by invalid inputs.
		bool getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, RRVec3& out) const;


		//! Reports that appearance of one or more materials in static objects has changed.
		//
		//! Call this when you changed material properties of static objects.
		//! It is not necessary to report change in dynamic objects (it would only cost time, without affecting GI).
		//!
		//! Note that solver creates physically validated copy of all materials in static objects,
		//! you can edit both original materials (in custom scale, used for realtime rendering)
		//! and validated copies (in physical scale, used for indirect illumination),
		//! but you are responsible for keeping them in sync.
		//! Complete code sequence to edit original material, synchronize copy in solver and report change could look like
		//! \code
		//! ... here you edit material's color
		//! material->convertToLinear(solver->getColorSpace()); // converts material's color to colorLinear
		//! solver->reallocateBuffersForRealtimeGI(); // allocates specular reflection cubes if you add specular
		//! solver->reportMaterialChange();
		//! \endcode
		//! If you make e.g. red color and blue colorLinear, realtime renderer will
		//! render red material, but reflected light will be blue.
		//! \param dirtyShadows
		//!  Set this if you want shadows updated. Shadows may need update after change in material transparency.
		//! \param dirtyGI
		//!  Set this if you want GI updated, or keep it false to save time.
		//!  If you use \ref calc_fireball, changes in emissive textures are
		//!  recognized automatically, you don't have to report them, however, some other material changes
		//!  won't affect GI until you rebuild fireball with buildFireball().
		virtual void reportMaterialChange(bool dirtyShadows, bool dirtyGI);

		//! Reports that scene has changed and direct or global illumination should be updated.
		//
		//! Call this function when light moves, changes color etc.. or when geometry changes,
		//! so that shadows and/or GI should be updated.
		//! Note that direct illumination changes also when dynamic objects move,
		//! call it with lightIndex -1 in such case.
		//! \param lightIndex
		//!  Light number in list of lights, see setLights(). If geometry changes, pass -1, change will be reported to all lights in solver.
		//! \param dirtyShadows
		//!  Tells that direct shadows should be updated.
		//!  Generic RRSolver uses it only to invalidate collider returned by getCollider() (because when shadows need update, it's most likely because geometry did change);
		//!  subclasses (e.g. rr_gl::RRSolverGL) use it also to update light's shadowmaps.
		//! \param dirtyGI
		//!  Tells that global illumination should be updated.
		//!  Generic RRSolver ignores it, but subclasses (e.g. rr_gl::RRSolverGL)
		//!  use it to redetect appropriate direct irradiances and call setDirectIllumination().
		//!  You can save time by setting false when changes in scene were so small, that
		//!  change in GI would be hardly visible. This is usually case when objects move,
		//!  but lights stay static or nearly static - moving objects have much weaker global effects
		//!  than moving lights.
		//! \param dirtyRange
		//!  Tells that shadowmapping near/far range should be updated.
		//!  Near should be small enough to not clip objects, but not smaller
		//!  (it would increase shadow bias). Solver recalculates it on demand from distance of nearby objects.
		virtual void reportDirectIlluminationChange(int lightIndex, bool dirtyShadows, bool dirtyGI, bool dirtyRange);

		//! Reports interaction between user and application.
		//
		//! This is useful for better CPU utilization in non-interactive periods of life
		//! of your application.
		//! Call it each time user hits key, moves mouse etc, or animation frame is played.
		//! \n\n When called often,
		//! solver calculates illumination in small batches to preserve realtime performance.
		//! This is typical mode for games.
		//! \n\n When not called at all,
		//! there's no interaction between user and application
		//! and solver increases calculation batches up to 100ms at once to better utilize CPU.
		//! This happens for example in game editor, when level designer stops moving mouse.
		virtual void reportInteraction();


		//! Build and start Fireball. Optionally save it to file.
		//
		//! Builds Fireball from scratch, starts it and optionally saves it to file.
		//! Later you can load saved file and start Fireball faster, see loadFireball().
		//! This function must be called after setStaticObjects(), because it depends on
		//! static objects in scene. It doesn't depend on lights end environment.
		//!
		//! Build complexity is O( avgRaysPerTriangle * t * log(t) )
		//! where t is number of triangles in static objects.
		//!
		//! \ref calc_fireball is faster, higher quality, smaller, realtime only solver;
		//! it is highly recommended for games.
		//! When used, non-realtime functions like updateLightmaps(some params..) are not supported,
		//! but realtime functions like updateLightmaps(other params..) and updateEnvironmentMap() are faster
		//! and produce better results using less memory.
		//!
		//! \param avgRaysPerTriangle
		//!  Average number of rays per triangle used to compute form factors.
		//!  Higher number = longer calculation, higher quality results, bigger file.
		//! \param filename
		//!  Data precomputed for current static scene will be saved to this file.
		//!  If empty, automatically generated name (in temp directory) is used.
		//! \return
		//!  True if successful.
		//!  For better consistency, if save (disk operation) fails, Fireball is not started.
		bool buildFireball(unsigned avgRaysPerTriangle, const RRString& filename);

		//! Load and start Fireball.
		//
		//! Loads and starts Fireball previously saved by buildFireball().
		//! This function should be called before calculate() to avoid unnecessary operations.
		//!
		//! \ref calc_fireball is faster, higher quality, smaller, realtime only solver;
		//! it is highly recommended for games.
		//! When used, non-realtime functions like updateLightmaps(some params..) are not supported,
		//! but realtime functions like updateLightmaps(other params..) and updateEnvironmentMap() are faster
		//! and produce better results using less memory.
		//!
		//! \param filename
		//!  File with data to load, previously created by buildFireball().
		//!  If empty, automatically generated name is used (in your temp directory).
		//! \param onlyPerfectMatch
		//!  Loads file only if it was built for exactly the same scene on the same CPU architecture.
		//!  Applications that build fireball on end-user machines should use true.
		//!  Applications that ship with fireball pre-built by developer should use true
		//!  during development (so that any change in scene results in rebuild)
		//!  and false on end-user machines (so that supplied fireball is always loaded even if scene seems different;
		//!  difference may be caused by different floation point precision on end-user's system).
		//! \return
		//!  True if successful.
		bool loadFireball(const RRString& filename, bool onlyPerfectMatch);

		//! Switch to non-Fireball solver that supports offline calculations.
		void leaveFireball();

		//! Type of internal solver. For debugging purposes.
		enum InternalSolverType
		{
			NONE,
			ARCHITECT, // default solver type
			FIREBALL, // must be activated by loadFireball() or buildFireball()
			BOTH
		};
		//! Returns type of active internal solver.
		InternalSolverType getInternalSolverType() const;


		//! Checks consistency of data in solver and reports problems found using RRReporter.
		//
		//! While all precomputed lighting and cheap-to-detect realtime lighting problems
		//! are reported immediately even without checkConsistency(),
		//! more expensive realtime lighting checks are done only on this request.
		void checkConsistency();


		//! Returns true if solver contains at least one light source.
		//
		//! This could be enabled light, emissive material or environment/skybox.
		//! Simple presence of light source is tested, it is not guaranteed
		//! that light source actually affects objects in scene.
		virtual bool containsLightSource() const;
		//! Returns true if solver contains at least one realtime GI capable light source.
		//
		//! All light sources are realtime GI capable with Fireball,
		//! the only light source ignored by Architect solver is environment/skybox.
		virtual bool containsRealtimeGILightSource() const;

		//! Allocates buffers for realtime GI illumination of objects in solver.
		//
		//! See RRObjects::allocateBuffersForRealtimeGI() for parameters
		//! and additional information.
		virtual void allocateBuffersForRealtimeGI(int layerLightmap, int layerEnvironment, unsigned diffuseEnvMapSize = 4, unsigned specularEnvMapSize = 16, unsigned refractEnvMapSize = 16, bool allocateNewBuffers = true, bool changeExistingBuffers = true, float specularThreshold = 0.2f, float depthThreshold = 0.1f) const;

		//! Returns multiObject created by merging all static objects in scene, see setStaticObjects().
		RRObject* getMultiObject() const;

		struct RR_API PathTracingParameters
		{
			//! Multiplies direct illumination from sources.
			Multipliers direct;
			//! Multiplies indirect illumination from sources. Set both direct and indirect identical for realistic results.
			Multipliers indirect;

			//! Use only given types of materials.
			RRMaterial::BrdfType brdfTypes;

			// how many bounces before given optimization kicks in?
			unsigned useFlatNormalsSinceDepth; // unbiased = UINT_MAX
			unsigned useSolverDirectSinceDepth; // unbiased = UINT_MAX
			unsigned useSolverIndirectSinceDepth; // unbiased = UINT_MAX

			// termination criteria
			unsigned stopAtDepth; // unbiased = UINT_MAX
			RRReal   stopAtVisibility; // unbiased = 0

			PathTracingParameters()
			{
				brdfTypes = RRMaterial::BRDF_ALL;
				useFlatNormalsSinceDepth = UINT_MAX;
				useSolverDirectSinceDepth = UINT_MAX;
				useSolverIndirectSinceDepth = UINT_MAX;
				stopAtDepth = 20; // without stopAtDepth limit, Lightmaps sample with refractive sphere runs forever, single ray bounces inside sphere. it hits only pixels with r=1, so it does not fade away. material clamping does not help here, point materials are used for quality>=18. in this case, stopAtDepth 20 is not visibly slower than 2
				stopAtVisibility = 0.001f;
			}
		};

		//! Renders scene image into given frame, using pathtracer.
		//
		//! Pathtracing is slower and noisier than rasterization, but it offers higher quality if you give it enough time.
		//! Pathtraced images contain random noise, so if you accumulate multiple noisy images, you get smoother one.
		//! \param camera Camera to view scene from.
		//! \param frame Framebuffer to render to. If you accumulate multiple frames into single buffer, format should be BF_RGBF or BF_RGBAF, to avoid color banding.
		//! \param accumulate Number of frames already accumulated in frame. Increase this number each time you call pathTraceFrame, zero it only when camera or scene change.
		//! \param parameters Additional parameters.
		void pathTraceFrame(const RRCamera& camera, RRBuffer* frame, unsigned accumulate, const PathTracingParameters& parameters);

	protected:

		//! Optional extension of calculate(), sets dirty flags in lights when it detects change in transparency texture or video.
		//
		//! Realtime GI implementations (like rr_gl::RRSolverGL)
		//! call it from calculate(), before updating shadowmaps.
		void calculateDirtyLights(const CalculateParameters* params = nullptr);

	private:

		void optimizeMultipliers(UpdateParameters& params, bool testEnvForBlackness) const;
		//! Detects direct illumination on all faces in scene and sends it to the solver.
		//
		//! This is more general version of detectDirectIllumination(),
		//! used for non-realtime calculations.
		//! It supports all light sources: current solver, environment, lights.
		//! \param paramsDirect
		//!  Parameters of update process.
		bool updateSolverDirectIllumination(const UpdateParameters* paramsDirect);

		//! Detects direct illumination, feeds solver and calculates until indirect illumination values are available.
		bool updateSolverIndirectIllumination(const UpdateParameters* paramsIndirect);

		bool gatherPerTrianglePhysical(const UpdateParameters* aparams, const class GatheredPerTriangleData* resultsPhysical, unsigned numResultSlots);
		unsigned updateVertexBufferFromPerTriangleDataPhysical(unsigned objectHandle, RRBuffer* vertexBuffer, RRVec3* perTriangleDataPhysical, unsigned stride, bool allowScaling) const;
		void calculateCore(float improveStep, const CalculateParameters* params=nullptr);
		unsigned updateVertexBufferFromSolver(int objectNumber, RRBuffer* vertexBuffer, const UpdateParameters* params);
		void updateVertexLookupTableDynamicSolver();
		void updateVertexLookupTablePackedSolver();
		bool cubeMapGather(RRObjectIllumination* illumination, unsigned layerEnvironment);
		struct Private;
		Private* priv;
		friend class PathtracerWorker;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// Checks API version at runtime, detects some configuration mismatches
	//
	// If you want to catch the most common configuration problems at runtime,
	// make this first step of your application:
	//   if (!RR_INTERFACE_OK) warn_or_terminate_with_message(RR_INTERFACE_MISMATCH_MSG);
	//
	// Note that
	//  1) this does not detect all incompatibilities, mismatches in compiler options can still cause havoc
	//  2) detected incompatibility does not have to be fatal, you can try running anyway
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Returns if interface matches. False = dll mismatch, app should be terminated.
	#define RR_INTERFACE_OK (rr::RR_INTERFACE_ID_LIB()==RR_INTERFACE_ID_APP() && !strcmp(RR_INTERFACE_DESC_APP(),rr::RR_INTERFACE_DESC_LIB()))
	//! Returns id of interface offered by library.
	RR_API unsigned RR_INTERFACE_ID_LIB();
	// Returns id of interface expected by app.
	#define RR_INTERFACE_ID_APP() unsigned( sizeof(rr::RRSolver) + 6 )
	//! Returns description of interface offered by library.
	RR_API const char* RR_INTERFACE_DESC_LIB();
	// Returns description of interface expected by app.
	#if defined(_WIN32)
		#define RR_INTERFACE_OS "win"
	#elif defined(__APPLE__)
		#define RR_INTERFACE_OS "osx"
	#else
		#define RR_INTERFACE_OS "linux"
	#endif
	#if defined(_M_X64) || defined(_LP64)
		#define RR_INTERFACE_BITS "64"
	#else
		#define RR_INTERFACE_BITS "32"
	#endif
	#if defined(NDEBUG)
		#define RR_INTERFACE_RLSDBG "release"
	#else
		#define RR_INTERFACE_RLSDBG "debug"
	#endif
	#if defined(RR_STATIC)
		#define RR_INTERFACE_DLLSTATIC "static"
	#else
		#define RR_INTERFACE_DLLSTATIC "dll"
	#endif
	#define RR_STRINGIZE1(x) #x
	#define RR_STRINGIZE(x) RR_STRINGIZE1(x)
	#if defined(_MSC_VER)
	#define RR_INTERFACE_COMPILER "Visual C++ " RR_STRINGIZE(_MSC_VER)
	#elif defined(__clang__)
		#define RR_INTERFACE_COMPILER "clang " RR_STRINGIZE(__clang_major__) "." RR_STRINGIZE(__clang_minor__)
	#elif defined(__GNUC__)
		#define RR_INTERFACE_COMPILER "gcc " RR_STRINGIZE(__GNUC__) "." RR_STRINGIZE(__GNUC_MINOR__)
	#else
		#define RR_INTERFACE_COMPILER "unknown"
	#endif
	#if defined(_CPPRTTI)
		#define RR_INTERFACE_RTTI "rtti"
	#else
		#define RR_INTERFACE_RTTI ""
	#endif
	#define RR_INTERFACE_DESC_APP() ( RR_INTERFACE_COMPILER " " RR_INTERFACE_OS RR_INTERFACE_BITS " " RR_INTERFACE_RLSDBG " " RR_INTERFACE_DLLSTATIC " " RR_INTERFACE_RTTI )
	// Returns description of version mismatch.
	#define RR_INTERFACE_MISMATCH_MSG "LightsprintCore version mismatch.\nLibrary has interface: %s %d\nApplication expects  : %s %d\n",rr::RR_INTERFACE_DESC_LIB(),rr::RR_INTERFACE_ID_LIB(),RR_INTERFACE_DESC_APP(),RR_INTERFACE_ID_APP()

} // namespace

#endif
