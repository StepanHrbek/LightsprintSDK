#ifndef RRDYNAMICSOLVER_H
#define RRDYNAMICSOLVER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRDynamicSolver.h
//! \brief LightsprintCore | global illumination solver for dynamic scenes
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2000-2010
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

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
		void setEnvironment(RRBuffer* environment0, RRBuffer* environment1=NULL);
		//! Returns scene environment set by setEnvironment().
		RRBuffer* getEnvironment(unsigned environmentIndex=0) const;

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
		virtual void setLights(const RRLights& lights);

		//! Returns lights in scene, set by setLights().
		const RRLights& getLights() const;


		//! Sets custom irradiance for all triangles in scene.
		//
		//! This is one of paths for light to enter solver, others are setLights(), setEnvironment(), emissive materials.
		//!
		//! If you use rr_gl::RRDynamicSolverGL, setDirectIllumination() is called automatically
		//! from calculate(). If you want to provide your own direct illumination data, use RRDynamicSolver
		//! and call setDirectIllumination() manually before calculate().
		//!
		//! \param perTriangleIrradiance
		//!  Array of average per-triangle direct-lighting irradiances in custom scale.
		//!  In other words, average triangle colors when direct lighting+shadows are applied,
		//!  but materials are not. So result for fully shadowed triangle is 0, fully lit
		//!  0xffffff00 (00 is alpha, it is ignored).
		//!
		//!  Order of values in array is defined by order of triangles in scene,
		//!  first all triangles from object 0, then all triangles from object 1 etc.
		//!
		//!  Array must stay valid at least until next setDirectIllumination() call.
		//!  Array is not adopted+deleted, you are still responsible for deleting it.
		//!  You are free to set always the same array or each time different one,
		//!  performace is identical.
		//!
		//!  Setting NULL is the same as setting array filled by zeroes, no custom irradiance.
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
		//! For all static objects with RRObject::illumination NULL, illumination is allocated.
		//!
		//! setStaticObjects() removes effects of previous loadFireball() or calculate().
		//!
		//! \param objects
		//!  Static contents of your scene, set of static objects.
		//!  Object's getTriangleMaterial() and getPointMaterial() should return values
		//!  in custom scale (usually screen colors).
		//!  \n You guarantee that these objects will stay completely static(constant)
		//!  until next setStaticObjects() call or solver destruction.
		//!  Changing their meshes, positions, colors etc could lead to crash.
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

		//! Sets dynamic contents of scene, all dynamic objects at once.
		//
		//! Unlike static objects, you may freely edit dynamic objects between frames.
		//! If you add or remove objects, call setDynamicObjects() again. If you only edit objects,
		//! there's no need to call it again.
		//!
		//! Dynamic objects affect realtime lighting, they are ignored when building static lightmaps.
		void setDynamicObjects(const RRObjects& objects);
		//! Returns dynamic contents of scene, all dynamic objects at once.
		const RRObjects& getDynamicObjects() const;


		//! Inserts all buffers found in solver's materials, lights and environment into collection.
		//
		//! Can be used to gather all texture filenames, to pause all videos etc.
		//! Note that illumination buffers (lightmaps) are not inserted.
		//! May shuffle elements that were already present in collection.
		//! Removes duplicates, the same buffer is never listed twice.
		void getAllBuffers(RRVector<RRBuffer*>& buffers) const;


		//! Optional parameters of calculate(). Currently used only by Fireball.
		struct CalculateParameters
		{
			//! Only for Fireball solver:
			//! Multiplies emittance values in solver, but not emissive materials itself.
			//! So when realtime rendering scene, emissive materials are not affected,
			//! but indirect illumination they create is multiplied.
			float materialEmittanceMultiplier;

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
			//! 0 disables updates, existing lighting stays unchanged.
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

			//! Only for RRDynamicSolverGL:
			//! For how long time indirect illumination may stay unchanged.
			//! 0 = update in each frame, highest quality.
			//! 0.05 = update less frequently, faster.
			float secondsBetweenDDI;

			//! Sets default parameters. This is used if you send NULL instead of parameters.
			CalculateParameters()
			{
				materialEmittanceMultiplier = 1;
				materialEmittanceStaticQuality = 17;
				materialEmittanceVideoQuality = 5;
				materialEmittanceUsePointMaterials = false;
				materialTransmittanceStaticQuality = 2;
				materialTransmittanceVideoQuality = 2;
				environmentStaticQuality = 6000;
				environmentVideoQuality = 300;
				qualityIndirectDynamic = 3;
				qualityIndirectStatic = 3;
				secondsBetweenDDI = 0;
			}
			bool operator ==(const CalculateParameters& a) const;
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
			bool operator ==(const UpdateParameters& a) const;
		};

		//! Parameters of filtering in updateLightmap()/updateLightmaps().
		struct RR_API FilteringParameters
		{
			//! Amount of smoothing applied when baking lightmaps.
			//! Makes edges smoother, reduces noise, but washes out tiny details.
			//! Reasonable values are around 1. 0=off.
			float smoothingAmount;
			//! Distance in pixels, how deep foreground (used) colors spread into background (unused) regions.
			//! It is necessary only when creating mipmaps from lightmaps.
			unsigned spreadForegroundColor;
			//! Color of unused background pixels.
			RRVec4 backgroundColor;
			//! Smooth colors between opposite borders.
			//! Some mappings need it to prevent seams, e.g. one kind of spherical mapping.
			//! Generally, enable wrap if lightmap is to be later applied with wrapping enabled.
			bool wrap;
			//! Sets default parameters.
			FilteringParameters()
			{
				smoothingAmount = 0;
				spreadForegroundColor = 0;
				backgroundColor = RRVec4(0);
				wrap = true;
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
		//! For 2d texture buffer (lightmap, bentNormalMap), uv channel material->lightmapTexcoord is used.
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
		//!   and store them to getStaticObjects()[objectNumber]->illumination->getLayer(layerIndex)
		//! -# call updatelightmaps()
		//! -# enjoy buffers with computed lighting, you can do buffer->save(), buffer->lock(), rendererOfScene->render()...
		//!
		//! For 2d texture buffer (lightmap, bentNormalMap), uv channel material->lightmapTexcoord is used.
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
		//! \param layerNumberLighting
		//!  1 lightmap per object will be computed into existing buffers in this layer,
		//!  getStaticObjects()[objectNumber]->illumination->getLayer(layerNumber).
		//!  \n Negative number disables update of lightmaps.
		//! \param layerNumberDirectionalLighting
		//!  3 directional lightmaps per object will be computed into existing buffers in this layer and two successive layers,
		//!  getStaticObjects()[objectNumber]->illumination->getLayer(layerNumber).
		//!  \n Negative number disables update of directional lightmaps.
		//! \param layerNumberBentNormals
		//!  Bent normals will be computed into existing buffers in this layer,
		//!  getStaticObjects()[objectNumber]->illumination->getLayer(layerNumberBentNormals).
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
		//! \n- diffuse map is to be sampled (by surface normal) in object's rough pixels
		//! \n- specular map is to be sampled (by reflected view direction) in object's glossy pixels
		//!
		//! This function generates indirect illumination for dynamic object.
		//! It is called automatically for all objects passed to setDynamicObjects().
		//! If you render dynamic objects manually, without passing them to solver,
		//! call at least this solver function to update their illumination.
		//! It is safe to call it often, it returns quickly if it detects that illumination is already up to date.
		//!
		//! It reads static scene illumination, so don't call it before calculate(), otherwise
		//! dynamic objects will reflect light from previous frame.
		//!
		//! It reads RRObjectIllumination variables with 'envMap' in name,
		//! update them (e.g. buffer sizes) before calling this function.
		//! allocateBuffersForRealtimeGI() can do part of this work for you.
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


		//! Reports that appearance of one or more materials in static objects has changed.
		//
		//! Call this when you changed material properties of static objects
		//! (and RRObject::getTriangleMaterial() returns new materials).
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
		//! Call this function when light moves, changes color etc.. or when shadow caster moves,
		//! so that shadows and/or GI should be updated.
		//! \param lightIndex
		//!  Light number in list of lights, see setLights(). If it is -1, change is reported for all lights in solver.
		//! \param dirtyShadows
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
		virtual void reportDirectIlluminationChange(int lightIndex, bool dirtyShadows, bool dirtyGI);

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
		//! \param onlyPerfectMatch
		//!  Loads file only if it was built for exactly the same scene on the same CPU architecture.
		//!  Applications that build fireball on end-user machines should use true.
		//!  Applications that ship with fireball pre-built by developer should use true
		//!  during development (so that any change in scene results in rebuild)
		//!  and false on end-user machines (so that supplied fireball is always loaded even if scene seems different;
		//!  difference may be caused by different floation point precision on end-user's system).
		//! \return
		//!  True if successful.
		bool loadFireball(const char* filename, bool onlyPerfectMatch);

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

		//! Allocates illumination buffers for realtime GI.
		//
		//! This function allocates empty buffers in RRObject::illumination of all objects.
		//! \param lightmapLayerNumber
		//!  Arbitrary layer number for storing per-vertex indirect illumination.
		//!  You should pass the same layer number to renderer, so it can use buffers you just allocated.
		//!  Use any negative number for no allocation.
		//! \param diffuseCubeSize
		//!  Size of diffuse cube maps used for indirect illumination of dynamic objects.
		//!  Warning: GI calculation time is O(cubeSize^2)
		//! \param specularCubeSize
		//!  Size of specular cube maps used for indirect illumination of shiny objects.
		//!  Warning: GI calculation time is O(cubeSize^2)
		//! \param gatherCubeSize
		//!  If it is not negative, value is copied into RRObjectIllumination::gatherEnvMapSize of all objects.
		//! \param allocateNewBuffers
		//!  If buffer does not exist yet, true = it will be allocated, false = no action.
		//! \param changeExistingBuffers
		//!  If buffer already exists, true = it will be resized or deleted accordingly, false = no action.
		virtual void allocateBuffersForRealtimeGI(int lightmapLayerNumber, int diffuseCubeSize = 4, int specularCubeSize = 16, int gatherCubeSize = -1, bool allocateNewBuffers = true, bool changeExistingBuffers = true) const;

		//! Returns multiObject created by merging all static objects in scene, see setStaticObjects().
		RRObject* getMultiObjectCustom() const;

		//! As getMultiObjectCustom, but with materials converted to physical space.
		const RRObject* getMultiObjectPhysical() const;

	protected:

		//! Optional extension of calculate(), sets dirty flags in lights when it detects change in transparency texture or video.
		//
		//! Realtime GI implementations (like rr_gl::RRDynamicSolverGL)
		//! call it from calculate(), before updating shadowmaps.
		void calculateDirtyLights(CalculateParameters* params = NULL);

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
		bool cubeMapGather(RRObjectIllumination* illumination, RRVec3* exitanceHdr);
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
	#define RR_INTERFACE_ID_APP() unsigned( sizeof(rr::RRDynamicSolver) + 6 )
	//! Returns if interface matches. False = dll mismatch, app should be terminated.
	#define RR_INTERFACE_OK (RR_INTERFACE_ID_APP()==rr::RR_INTERFACE_ID_LIB())
	//! Returns description of interface offered by library + compile date.
	RR_API const char* RR_INTERFACE_DESC_LIB();
	// Returns description of interface expected by app + compile date.
	#if defined(NDEBUG) && defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "release static (" __DATE__ " " __TIME__ ")"
	#elif defined(NDEBUG) && !defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "release dll (" __DATE__ " " __TIME__ ")"
	#elif !defined(NDEBUG) && defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "debug static (" __DATE__ " " __TIME__ ")"
	#elif !defined(NDEBUG) && !defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "debug dll (" __DATE__ " " __TIME__ ")"
	#endif
	// Returns description of version mismatch.
	#define RR_INTERFACE_MISMATCH_MSG "LightsprintCore dll version mismatch.\nLibrary has interface: %d %s\nApplication expects  : %d %s\n",rr::RR_INTERFACE_ID_LIB(),rr::RR_INTERFACE_DESC_LIB(),RR_INTERFACE_ID_APP(),RR_INTERFACE_DESC_APP()

} // namespace

#endif
