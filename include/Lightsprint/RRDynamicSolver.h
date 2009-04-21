#ifndef RRDYNAMICSOLVER_H
#define RRDYNAMICSOLVER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRDynamicSolver.h
//! \brief LightsprintCore | global illumination solver for dynamic scenes
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2000-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include "RRVector.h"
#include "RRIllumination.h"
#include "RRObject.h"
#include "RRLight.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRDynamicSolver
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
	//! You may implement it in your RRDynamicSolver subclass
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

	class RR_API RRDynamicSolver: public RRUniformlyAllocatedNonCopyable
	{
	public:
		RRDynamicSolver();
		virtual ~RRDynamicSolver();


		//! Set scaler used by this scene i/o operations.
		//
		//! It switches solver inputs and outputs from physical scale to custom scale defined by scaler,
		//! e.g. screen colors. See RRScaler for details.
		//!
		//! In 99% of cases you want to set scaler before using solver as nearly all
		//! existing renderers and datasets use sRGB or other custom scale.
		//! Not setting scaler is allowed for future rendering pipelines working fully in physical scale.
		//! Note that if scaler is not set, internal material corrections are disabled,
		//! so if you provide solver with invalid materials (for example material that reflects
		//! more light than it receives), solution will diverge to infinite values,
		//! debug version will assert.
		//! \param scaler
		//!  Scaler for converting illumination between physical and custom scale.
		//!  It will be used by all data input and output paths in RRDynamicSolver, if not specified otherwise.
		//!  Note that scaler is not adopted, you are still responsible for deleting it
		//!  when it's no longer needed.
		virtual void setScaler(const RRScaler* scaler);

		//! Returns scaler used by this scene i/o operations, set by setScaler().
		const RRScaler* getScaler() const;


		//! Sets environment around scene.
		//
		//! This is one of ways how light enters solver, others are setLights(), setDirectIllumination(), emissive materials.
		//! Environment is rendered around scene and scene is lit by environment (but only in offline solver).
		//!
		//! By default, scene contains no environment, which is the same as black environment.
		//! \param environment
		//!  HDR map of environment around scene.
		//!  Its RRBuffer::getValue() should return values in physical scale.
		//!  Note that environment is not adopted, you are still responsible for deleting it
		//!  when it's no longer needed.
		void setEnvironment(const RRBuffer* environment);

		//! Returns environment around scene, set by setEnvironment().
		const RRBuffer* getEnvironment() const;


		//! Sets lights in scene, all at once.
		//
		//! This is one of ways how light enters solver, others are setEnvironment(), setDirectIllumination(), emissive materials.
		//!
		//! By default, scene contains no lights.
		virtual void setLights(const RRLights& lights);

		//! Returns lights in scene, set by setLights().
		const RRLights& getLights() const;


		//! Sets custom irradiance for all triangles in scene.
		//
		//! This is one of ways how light enters solver, others are setLights(), setEnvironment(), emissive materials.
		//!
		//! \param perTriangleIrradiance
		//!  Array of average per-triangle direct-lighting irradiances in custom scale.
		//!  In other words, average triangle colors when direct lighting+shadows are applied,
		//!  but materials are not. So result for fully shadowed triangle is 0, fully lit
		//!  0xffffff00 (00 is alpha, it is ignored).
		//!
		//!  Order of values in array is defined by order of triangles in solver's multiobject.
		//!  By default, order can't be guessed without asking getMultiObjectCustom().
		//!  However, if you disable all optimizations in setStaticObjects(), order is guaranteed to be
		//!  the most simple one: first all triangles from object 0, then all triangles from object 1 etc.
		//!
		//!  Array must stay valid at least until next setDirectIllumination() call.
		//!  Array is not adopted+deleted, you are still responsible for deleting it.
		//!  You are free to set always the same array or each time different one,
		//!  performace is identical.
		//!
		//!  Setting NULL is the same as setting array full of zeroes, no custom irradiance.
		void setDirectIllumination(const unsigned* perTriangleIrradiance);

		//! Sets factor that multiplies intensity of direct illumination set by setDirectIllumination().
		//
		//! Default value is 1 = realistic.
		//! 2 makes direct illumination in setDirectIllumination()
		//! 2x stronger, so also computed indirect illumination is 2x stronger.
		//! You can get the same results if you stay with default 1
		//! and multiply computed indirect lighting in shader,
		//! but this function does it for free, without slowing down shader.
		void setDirectIlluminationBoost(RRReal boost);


		//! Illumination smoothing parameters.
		//
		//! Default values are reasonable for both realtime and offline rendering, but
		//! - if needle artifacts appear in realtime illumination,
		//!   try increasing minFeatureSize (maxSmoothAngle must be positive)
		//! - if seams between scene segments appear,
		//!   try increasing minFeatureSize (maxSmoothAngle must be positive) or vertexWeldDistance
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
			//! Angle in radians, controls smoothing mode and intensity.
			//
			//! Zero or negative value makes illumination smooth only where
			//! difference between vertex normals is <=abs(maxSmoothAngle), ignoring minFeatureSize.
			//! This mode preserves smoothing created by 3d artist.
			//! It is enabled by default.
			//!
			//! Positive value makes illumination smooth where angle between face normals is smaller than maxSmoothAngle,
			//! with additional blur specified by minFeatureSize.
			//! Optimal positive value depends on your geometry, but reasonable value could be 0.33 (approx 19 degrees).
			//! This mode generates smoothing automatically, ignoring artist's work (normals).
			//! It is suitable for models with errors in smoothing.
			float maxSmoothAngle;
			//! Distance in world units. Smaller indirect light features will be smoothed. This could be imagined as a kind of blur.
			//! Use default 0 for no blur and watch for possible artifacts in areas with small geometry details
			//! and 'needle' triangles. Increase until artifacts disappear.
			//! 0.15 could be good for typical interior game with 1m units.
			//! Only indirect lighting is affected, so even 15cm blur is mostly invisible.
			//! \n Note: minFeatureSize is ignored if maxSmoothAngle is <=0.
			float minFeatureSize;
			//! Makes needle-like triangles with equal or smaller angle (rad) ignored.
			//! Default 0 removes only completely degenerated triangles.
			//! 0.001 is a reasonable value to put extremely needle like triangles off calculation,
			//! which may help in some situations.
			//! \n Note: if you see needle-like artifacts in realtime rendering,
			//! try increase minFeatureSize.
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
				vertexWeldDistance = 0; // weld enabled for identical vertices
				maxSmoothAngle = -0.01f; // smooth illumination where vertex normals differ <=0.01rad
				minFeatureSize = 0; // disabled
				ignoreSmallerAngle = 0; // ignores degerated triangles
				ignoreSmallerArea = 0; // ignores degerated triangles
			}
		};

		//! Sets static contents of scene, all static objects at once.
		//
		//! Order of objects passed in first parameter is used for object numbering,
		//! any further references to n-th object refer to objects[n].
		//!
		//! Scene must always contain static objects.
		//! Major occluders (buildings, large furniture etc) should be part
		//! of static scene.
		//! Handling major occluders as dynamic objects is safe,
		//! but it introduces errors in lighting, so it is not recommended.
		//!
		//! setStaticObjects() removes effects of previous loadFireball() or calculate().
		//!
		//! \param objects
		//!  Static contents of your scene, set of static objects.
		//!  Objects should not move (in 3d space) during our lifetime.
		//!  Object's getTriangleMaterial() and getPointMaterial() should return values
		//!  in custom scale (usually screen colors).
		//! \param smoothing
		//!  Static scene illumination smoothing.
		//!  Set NULL for default values.
		//! \param cacheLocation
		//!  Whether and where to cache colliders (speeds up startup when opening the same geometry next time).
		//!  It is passed to RRCollider::create(), so
		//!  default NULL caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		//! \param intersectTechnique
		//!  Intersection technique used by solver. Techniques differ by speed and memory requirements.
		//! \param copyFrom
		//!  Should stay NULL (used by sceneViewer to reuse multiObjectCustom and smoothing from old solver).
		virtual void setStaticObjects(const RRObjects& objects, const SmoothingParameters* smoothing, const char* cacheLocation=NULL, RRCollider::IntersectTechnique intersectTechnique=RRCollider::IT_BSP_FASTER, RRDynamicSolver* copyFrom = NULL);
		//! Returns static contents of scene, all static objects at once.
		const RRObjects& getStaticObjects() const;

		//! Returns number of static objects in scene. Shortcut for getStaticObjects().size().
		unsigned getNumObjects() const;

		//! Returns i-th static object in scene. Shortcut for getStaticObjects()[i].object.
		RRObject* getObject(unsigned i);

		//! Returns illumination of i-th static object in scene. Shortcut for getStaticObjects()[i].illumination.
		RRObjectIllumination* getIllumination(unsigned i);
		//! Returns illumination of i-th static object in scene. Shortcut for getStaticObjects()[i].illumination.
		const RRObjectIllumination* getIllumination(unsigned i) const;

		//! Optional parameters of calculate(). Currently used only by Fireball.
		struct CalculateParameters
		{
			//! Quality of indirect lighting when direct lighting changes.
			//! 1..20, default is 3.
			//! Higher quality makes calculate() take longer.
			unsigned qualityIndirectDynamic;
			//! Possibly higher quality of indirect lighting when direct lighting doesn't change.
			//! 1..1000, default is 3.
			//! Higher quality doesn't make calculate() take longer, but indirect lighting
			//! improves in several consecutive frames when direct lighting doesn't change.
			unsigned qualityIndirectStatic;
			//! For how long time indirect illumination may stay unchanged.
			//! 0 = update in each frame, highest quality.
			//! 0.05 = update less frequently, faster.
			//! Used by RRDynamicSolverGL, RRDynamicSolver ignores it.
			float secondsBetweenDDI;
			//! Sets default parameters. This is used if you send NULL instead of parameters.
			CalculateParameters()
			{
				qualityIndirectDynamic = 3;
				qualityIndirectStatic = 3;
				secondsBetweenDDI = 0;
			}
		};

		//! Calculates and improves indirect illumination on static objects.
		//
		//! To be called once per frame while rendering. To be called even when
		//! rendering is paused, calculation runs on background.
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
		virtual void calculate(CalculateParameters* params = NULL);

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
		//! use NULL for default parameters.
		struct UpdateParameters
		{
			//! Include lights set by setLights() as a source of illumination.
			//! True makes calculation non-realtime.
			bool applyLights;

			//! Include environment set by setEnvironment() as a source of illumination.
			//! True makes calculation non-realtime.
			bool applyEnvironment;

			//! Include current solution as a source of indirect illumination.
			//
			//! Current solution in solver is updated by calculate()
			//! and possibly also by updateLightmaps() (depends on parameters).
			//! \n Note that some functions restrict use of applyCurrentSolution
			//! and applyLights/applyEnvironment at the same time.
			bool applyCurrentSolution;

			//! Quality of computed illumination.
			//
			//! Relates to number of rays per texel or triangle,
			//! time taken grows mostly linearly with this number.
			//! (When !applyCurrentSolution and applyLights and !applyEnvironment,
			//! faster path is used.)
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

			//! Deprecated. Only partially supported since 2007.08.21.
			//
			//! 0..1 ratio, texels with greater fraction of hemisphere 
			//! seeing inside objects (or below rug, see rugDistance)
			//! are masked away.
			//! Default value 1 disables any correction.
			RRReal insideObjectsTreshold;

			//! Deprecated. Only partially supported since 2007.08.21.
			//
			//! Distance in world space, illumination coming from closer surfaces is masked away.
			//! Set it slightly above distance of rug and ground, to prevent darkness
			//! under the rug leaking half texel outside (instead, light around rug will
			//! leak under the rug). Set it to zero to disable any corrections.
			RRReal rugDistance;

			//! Distance in world space; illumination never comes from greater distance.
			//
			//! As long as it is bigger than scene size, results are realistic.
			//! Setting it below scene size makes results less realistic, illumination
			//! gets increasingly influenced by outer environment/sky instead of scene.
			//! (Rays are shot from texel into scene. When scene is not intersected
			//! in this or lower distance from texel, illumination is read from 
			//! outer environment/sky.)
			RRReal locality;

			//! For internal use only, don't change default NULL value.
			RRBuffer* lowDetailForLightDetailMap;

			//! For internal use only, don't change default RM_IRRADIANCE_CUSTOM_INDIRECT value.
			RRRadiometricMeasure measure_internal;

			//! For debugging only, to be described later.
			unsigned debugObject;
			//! For debugging only, to be described later.
			unsigned debugTexel;
			//! For debugging only, to be described later. (multiObjPostImport)
			unsigned debugTriangle;
			//! For debugging only, to be described later.
			void (*debugRay)(const RRRay* ray, bool hit);


			//! Sets default parameters for fast realtime update. Only direct lighting from RRDynamicSolver::setDirectIllumination() enters calculation.
			UpdateParameters()
			{
				applyLights = false;
				applyEnvironment = false;
				applyCurrentSolution = true;
				quality = 0;
				qualityFactorRadiosity = 1;
				insideObjectsTreshold = 1;
				rugDistance = 0.001f;
				locality = 100000;
				lowDetailForLightDetailMap = NULL;
				measure_internal = RM_IRRADIANCE_CUSTOM_INDIRECT;
				debugObject = UINT_MAX;
				debugTexel = UINT_MAX;
				debugTriangle = UINT_MAX;
				debugRay = NULL;
			}
			//! Sets default parameters for offline update. All lightsources in scene enter calculation.
			UpdateParameters(unsigned _quality)
			{
				applyLights = true;
				applyEnvironment = true;
				applyCurrentSolution = false;
				quality = _quality;
				qualityFactorRadiosity = 1;
				insideObjectsTreshold = 1;
				rugDistance = 0.001f;
				locality = 100000;
				lowDetailForLightDetailMap = NULL;
				measure_internal = RM_IRRADIANCE_CUSTOM_INDIRECT;
				debugObject = UINT_MAX;
				debugTexel = UINT_MAX;
				debugTriangle = UINT_MAX;
				debugRay = NULL;
			}
		};

		//! Parameters of filtering in updateLightmap()/updateLightmaps().
		struct FilteringParameters
		{
			//! How far foreground (used) colors spread into background (unused) regions.
			//! For lightmaps that are bilinearly filtered at application time, set 2 or higher
			//! to prevent background color leaking into foreground.
			//! For lightmaps that are unfiltered at application time, set 1 or higher.
			//! Set high enough (e.g. 1000) to fill whole background by nearest foreground color.
			unsigned spreadForegroundColor;
			//! Color of unused background pixels.
			RRVec4 backgroundColor;
			//! Smooth foreground-background transition.
			bool smoothBackground;
			//! Smooth colors between opposite borders.
			//! Some mappings need it to prevent seams, e.g. one kind of spherical mapping.
			//! Generally, enable wrap if lightmap is to be later applied with wrapping enabled.
			bool wrap;
			//! Sets default parameters.
			FilteringParameters()
			{
				spreadForegroundColor = 2;
				backgroundColor = RRVec4(0);
				smoothBackground = false;
				wrap = true;
			}
		};

		//! For single static object, calculates and updates lightmap and/or bent normals; in per-pixel or per-vertex; with direct, indirect or global illumination.
		//
		//! This is universal update for both per-pixel and per-vertex buffers.
		//! Type and format of data produced depends only on type and format of buffer you provide.
		//! Combinations like per-pixel colors, per-vertex bent normals are supported too.
		//! Format of buffer is preserved.
		//!
		//! For 2d texture buffer (lightmap, bentNormalMap), uv coordinates provided by RRMesh::getTriangleMapping() are used.
		//! All uv coordinates must be in 0..1 range and two triangles
		//! may not overlap in texture space.
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
		//!  \n May be NULL.
		//!  \n Types supported: BT_VERTEX_BUFFER, BT_2D_TEXTURE, however with \ref calc_fireball,
		//!     only vertex buffer is supported.
		//!  \n Formats supported: All color formats, RGB, RGBA, bytes, floats.
		//!     Data are converted to format of buffer.
		//!  \n Lightmap could contain direct, indirect or global illumination, depending on
		//!     parameters you set in params.
		//! \param directionalLightmap
		//!  Pointer to array of three lightmaps for storing calculated directional illumination.
		//!  \n Compatible with Unreal Engine 3 directional lightmaps.
		//!  \n May be NULL.
		//!  \n Supports the same types and formats as parameter 'lightmap'.
		//! \param bentNormals
		//!  Buffer for storing calculated bent normals, compact representation of directional information.
		//!  \n May be NULL.
		//!  \n RGB values (range 0..1) are calculated from XYZ worldspace normalized normals
		//!     (range -1..1) by this formula: (XYZ+1)/2.
		//!  \n Supports the same types and formats as parameter 'lightmap'.
		//! \param params
		//!  Parameters of the update process. Set NULL for default parameters that
		//!  specify very fast realtime/preview update.
		//! \param filtering
		//!  Parameters of lightmap filtering, set NULL for default ones.
		//! \return
		//!  Number of lightmaps and bent normal maps updated, 0 1 or 2.
		//!  If it's lower than you expect, read system messages (RRReporter) for more details on possible failure.
		//! \remarks
		//!  In comparison with more general updateLightmaps() function, this one
		//!  lacks paramsIndirect. However, you can still include indirect illumination
		//!  while updating single lightmap, see updateLightmaps() remarks.
		virtual unsigned updateLightmap(int objectNumber, RRBuffer* lightmap, RRBuffer* directionalLightmap[3], RRBuffer* bentNormals, const UpdateParameters* params, const FilteringParameters* filtering=NULL);

		//! For all static objects, calculates and updates lightmap and/or bent normal; in per-pixel or per-vertex; with direct, indirect or global illumination.
		//
		//! This is more powerful full scene version of single object's updateLightmap().
		//!
		//! Usage:
		//! -# create buffers of arbitrary types and formats
		//!   (per-pixel, per-vertex, bytes, floats, rgb, rgba, physical scale, custom scale)
		//!   and store them to getIllumination(objectIndex)->getLayer(layerIndex)
		//! -# call updatelightmaps()
		//! -# enjoy buffers with computed lighting, you can do buffer->save(), buffer->lock(), rendererOfScene->render()...
		//!
		//! For 2d texture buffer (lightmap, bentNormalMap), uv coordinates provided by RRMesh::getTriangleMapping() are used.
		//! All uv coordinates must be in 0..1 range and two triangles
		//! may not overlap in texture space.
		//! If it's not satisfied, contents of created lightmap is undefined.
		//!
		//! Lightmap update is one of the most demanding functions in Lightsprint SDK.
		//! Its \ref of_speed "time complexity" can be roughly predicted.
		//!
		//! Thread safe: no, but there's no need to run it from multiple threads at the same time,
		//!   all cores are used automatically.
		//!
		//! \param layerNumberLighting
		//!  1 lightmap per object will be computed into existing buffers in this layer,
		//!  getIllumination(objectNumber)->getLayer(layerNumber).
		//!  \n Negative number disables update of lightmaps.
		//! \param layerNumberDirectionalLighting
		//!  3 directional lightmaps per object will be computed into existing buffers in this layer and two successive layers,
		//!  getIllumination(objectNumber)->getLayer(layerNumber).
		//!  \n Negative number disables update of directional lightmaps.
		//! \param layerNumberBentNormals
		//!  Bent normals will be computed into existing buffers in this layer,
		//!  getIllumination(objectNumber)->getLayer(layerNumberBentNormals).
		//!  \n Negative number disables update of bent normals.
		//! \param paramsDirect
		//!  Parameters of the update process specific for direct illumination component of final color.
		//!  With e.g. paramsDirect->applyLights, direct illumination created by lights 
		//!  set by setLights() is added to the final value stored into lightmap.
		//!  \n Set both paramsDirect and paramsIndirect NULL for very fast/realtime update
		//!  that fills vertex buffers with indirect illumination in physical scale, read from current solution in solver.
		//!  Set only paramsDirect NULL for no direct illumination.
		//! \param paramsIndirect
		//!  Parameters of the update process specific for indirect illumination component of final color.
		//!  With e.g. paramsIndirect->applyLights, indirect illumination created by lights
		//!  set by setLights() is added to the final value stored into buffer.
		//!  For global illumination created by e.g. lights,
		//!  set both paramsDirect->applyLights and paramsIndirect->applyLights.
		//!  \n paramsIndirect->quality is ignored, only paramsDirect->quality matters.
		//!  \n Set both paramsDirect and paramsIndirect NULL for very fast/realtime update
		//!  that fills vertex buffers with indirect illumination in physical scale, read from current solution in solver.
		//!  Set only paramsIndirect NULL for no indirect illumination.
		//! \param filtering
		//!  Parameters of lightmap filtering, set NULL for default ones.
		//! \return
		//!  Number of lightmaps updated.
		//!  If it's lower than you expect, read system messages (RRReporter) for more details on possible failure.
		//! \remarks
		//!  As a byproduct of calculation with indirect ilumination, internal state of solver (current solution)
		//!  is updated, so that it holds computed indirect illumination for sources
		//!  and quality specified in paramsIndirect.
		//!  Internal state is properly updated even when buffers don't exist (so no other output is produced).
		//!  Following updateLightmap() will include this indirect lighting into computed buffer
		//!  if you call it with params->applyCurrentSolution=true and params->measure_internal=RM_IRRADIANCE_CUSTOM.
		//! \remarks
		//!  Update of selected objects (rather than all objects) is supported in multiple ways, use one of them.
		//!  All three ways produce the same quality, but first one may be faster in some cases.
		//!  - create buffers for selected objects, make sure other buffers are NULL and call updateLightmaps()
		//!  - if you don't need indirect illumination, simply call updateLightmap() for all selected objects
		//!  - call updateLightmaps(-1,-1,NULL,paramsIndirect,NULL) once to update current solution,
		//!    call updateLightmap(params with applyCurrentSolution=true and measure_internal=RM_IRRADIANCE_CUSTOM) for all selected objects
		//! \remarks
		//!  Sharing one lightmap by multiple objects is not supported out of the box. Please consult us for possible solutions.
		virtual unsigned updateLightmaps(int layerNumberLighting, int layerNumberDirectionalLighting, int layerNumberBentNormals, const UpdateParameters* paramsDirect, const UpdateParameters* paramsIndirect, const FilteringParameters* filtering);
		
		//! Makes other solver functions abort, returning quickly with bogus results.
		//
		//! You may set/unset it asynchronously, from other threads.
		//! Solver only reads it, never modifies it, so don't forget to clear it after abort.
		bool aborting;


		//! Optional update of illumination cache, makes updateEnvironmentMap() faster.
		//
		//! Depends on geometry but not on lighting, may be executed before static scene lighting is computed.
		//! Call it when CPU is not fully loaded (on background or while waiting) to use otherwise wasted cycles.
		//! If you don't have such cycles, don't call it at all.
		//!
		//! Reads RRObjectIllumination variables with 'envMap' in name. If you modify them on the fly
		//! (e.g. sizes of buffers), modify them before calling this function.
		//!
		//! Thread safe: yes
		void updateEnvironmentMapCache(RRObjectIllumination* illumination);
		//! Calculates and updates diffuse and/or specular environment map in object's illumination.
		//
		//! Generates specular and diffuse environment/reflection maps with object's indirect illumination
		//! \n- specular map is to be sampled (by reflected view direction) in object's glossy pixels
		//! \n- diffuse map is to be sampled (by surface normal) in object's rough pixels
		//!
		//! This function is all you need for indirect illumination of dynamic objects,
		//! you don't have to import them into solver.
		//! (but you still need to import static objects and call calculate() regularly)
		//!
		//! Reads static scene illumination, so don't call it before calculate(), otherwise
		//! dynamic objects will reflect light from previous frame.
		//!
		//! Reads RRObjectIllumination variables with 'envMap' in name,
		//! update them (e.g. buffer sizes) before calling this function.
		//!
		//! Thread safe: yes, may be called from multiple threads at the same time
		//!  (but there's no need as it uses all cores internally)
		//!
		//! \param illumination
		//!  Object's illumination to be updated.
		//!  (It's not necessary to have RRObject adapter for dynamic object, but its RRObjectIllumination must exist.)
		//!  RRObjectIllumination variables with 'envMap' in name are used, update them before calling
		//!  this function, e.g. create illumination->diffuseEnvMap if you want it to be updated here.
		//! \return
		//!  Number of environment maps updated. May be 0, 1 or 2 (optional diffuse and specular reflection map).
		virtual unsigned updateEnvironmentMap(RRObjectIllumination* illumination);


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


		//! Reports that appearance of one or more materials has changed.
		//
		//! Call this when you changed material properties of static objects
		//! (and RRObject::getTriangleMaterial() returns new materials).
		//! If you use \ref calc_fireball, material changes don't apply
		//! until you rebuild it (see buildFireball()).
		virtual void reportMaterialChange();

		//! Reports that scene has changed and direct or global illumination should be updated.
		//
		//! Call this function when light moves, changes color etc.. or when shadow caster moves,
		//! so that shadows and/or GI should be updated.
		//! \param lightIndex
		//!  Light number in list of lights, see setLights().
		//! \param dirtyShadowmap
		//!  Tells us that direct shadows should be updated.
		//!  Generic RRDynamicSolver ignores it, but subclasses (e.g. rr_gl::RRDynamicSolverGL)
		//!  use it to update light's shadowmaps.
		//! \param dirtyGI
		//!  Tells us that global illumination should be updated.
		//!  Generic RRDynamicSolver ignores it, but subclasses (e.g. rr_gl::RRDynamicSolverGL)
		//!  use it to redetect appropriate direct irradiances and call setDirectIllumination().
		//!  You can save time by setting false when changes in scene were so small, that
		//!  change in GI would be hardly visible. This is usually case when objects move,
		//!  but lights stay static or nearly static - moving objects have much weaker global effects
		//!  than moving lights.
		virtual void reportDirectIlluminationChange(unsigned lightIndex, bool dirtyShadowmap, bool dirtyGI);

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
		//! Build complexity is O( avgRaysPerTriangle * triangles * log(triangles) ).
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
		//!  If zero, current factors are used, without recalculating.
		//! \param filename
		//!  Data precomputed for current static scene will be saved to this file.
		//!  Set NULL for automatically generated name in temp directory.
		//!  Set "" for no saving.
		//! \return
		//!  True if successful.
		//!  For better consistency, if save (disk operation) fails, Fireball is not started.
		bool buildFireball(unsigned avgRaysPerTriangle, const char* filename);

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
		//!  Set NULL to use automatically generated name (in your temp directory).
		//! \return
		//!  True if successful.
		bool loadFireball(const char* filename);

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
		//! This could be light, emissive material or environment/skybox.
		//! Simple presence of light source is tested, it is not guaranteed
		//! that light source actually affects objects in scene.
		virtual bool containsLightSource() const;
		//! Returns true if solver contains at least one realtime GI capable light source.
		//
		//! All light sources are realtime GI capable with Fireball,
		//! the only light source ignored by Architect solver is environment/skybox.
		virtual bool containsRealtimeGILightSource() const;

		//! Returns multiObject created by merging all static objects in scene, see setStaticObjects().
		RRObject* getMultiObjectCustom() const;

		//! As getMultiObjectCustom, but with materials converted to physical space.
		const RRObjectWithPhysicalMaterials* getMultiObjectPhysical() const;

	protected:


	private:

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

		bool gatherPerTrianglePhysical(const UpdateParameters* aparams, const class GatheredPerTriangleData* resultsPhysical, unsigned numResultSlots, bool gatherEmissiveMaterials);
		unsigned updateVertexBufferFromPerTriangleDataPhysical(unsigned objectHandle, RRBuffer* vertexBuffer, RRVec3* perTriangleDataPhysical, unsigned stride, bool allowScaling) const;
		void calculateCore(float improveStep,CalculateParameters* params=NULL);
		unsigned updateVertexBufferFromSolver(int objectNumber, RRBuffer* vertexBuffer, const UpdateParameters* params);
		void updateVertexLookupTableDynamicSolver();
		void updateVertexLookupTablePackedSolver();
		struct Private;
		Private* priv;
		friend class GatheredIrradianceHemisphere;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// Checks dll at runtime, detects wrong dll configuration or version
	//
	// First step of your application should be
	//   if (!RR_INTERFACE_OK) terminate_with_message(RR_INTERFACE_MISMATCH_MSG);
	//////////////////////////////////////////////////////////////////////////////

	//! Returns id of interface offered by library.
	RR_API unsigned RR_INTERFACE_ID_LIB();
	// Returns id of interface expected by app.
	#define RR_INTERFACE_ID_APP() unsigned( sizeof(rr::RRDynamicSolver) + 5 )
	//! Returns if interface matches. False = dll mismatch, app should be terminated.
	#define RR_INTERFACE_OK (RR_INTERFACE_ID_APP()==rr::RR_INTERFACE_ID_LIB())
	//! Returns description of interface offered by library + compile date.
	RR_API const char* RR_INTERFACE_DESC_LIB();
	// Returns description of interface expected by app + compile date.
	#if defined(NDEBUG) && defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "RELEASE_STATIC (" __DATE__ " " __TIME__ ")"
	#elif defined(NDEBUG) && !defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "RELEASE_DLL (" __DATE__ " " __TIME__ ")"
	#elif !defined(NDEBUG) && defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "DEBUG_STATIC (" __DATE__ " " __TIME__ ")"
	#elif !defined(NDEBUG) && !defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "DEBUG_DLL (" __DATE__ " " __TIME__ ")"
	#endif
	// Returns description of version mismatch.
	#define RR_INTERFACE_MISMATCH_MSG "LightsprintCore dll version mismatch.\nLibrary has interface: %d %s\nApplication expects  : %d %s\n",rr::RR_INTERFACE_ID_LIB(),rr::RR_INTERFACE_DESC_LIB(),RR_INTERFACE_ID_APP(),RR_INTERFACE_DESC_APP()

} // namespace

#endif
