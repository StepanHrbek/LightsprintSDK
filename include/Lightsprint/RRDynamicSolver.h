#ifndef RRDYNAMICSOLVER_H
#define RRDYNAMICSOLVER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRDynamicSolver.h
//! \brief LightsprintCore | global illumination solver for dynamic scenes
//! \author Copyright (C) Stepan Hrbek, Lightsprint
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
	//  RRIlluminatedObject
	//! 3d object with storage space for calculated illumination.
	//
	//////////////////////////////////////////////////////////////////////////////

	struct RRIlluminatedObject
	{
		RRObject* object;
		RRObjectIllumination* illumination;
		RRIlluminatedObject(RRObject* o, RRObjectIllumination* i) : object(o), illumination(i) {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObjects
	//! Set of illuminated objects with interface similar to std::vector.
	//
	//! This is usual product of adapter that creates Lightsprint interface for external 3d scene.
	//! You may use it for example to
	//! - send it to RRDynamicSolver and calculate global illumination
	//! - manipulate this set before sending it to RRDynamicSolver, e.g. remove moving objects
	//! - render it immediately, without calculating global illumination
	//! - render it when global illumination is calculated
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRObjects : public RRVector<RRIlluminatedObject>
	{
	public:
		virtual ~RRObjects() {};
	};


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
	//! Thread safe: Partially.
	//!  All updateXxxx() functions may be called from multiple threads at the same time.
	//!  See updateVertexBuffer(), updateVertexBuffers(), updateLightmap(),
	//!  updateLightmaps() and updateEnvironmentMaps() for important details.
	//!  Other function may be called from multiple threads, but not at the same time.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRDynamicSolver
	{
	public:
		RRDynamicSolver();
		virtual ~RRDynamicSolver();


		//! Set scaler used by this scene i/o operations.
		//
		//! It allows you to switch from physical scale to custom scale, e.g. screen colors.
		//! See RRScaler for details.
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
		//! By default, scene contains no environment, which is the same as black environment.
		//! Environment matters only for updateLightmap() and updateLightmaps(), it is not used by realtime GI solver.
		//! \param environment
		//!  HDR map of environment around scene.
		//!  Its RRBuffer::getValue() should return
		//!  values in physical scale.
		//!  Note that environment is not adopted, you are still responsible for deleting it
		//!  when it's no longer needed.
		void setEnvironment(const RRBuffer* environment);

		//! Returns environment around scene, set by setEnvironment().
		const RRBuffer* getEnvironment() const;


		//! Sets lights in scene, all at once.
		//
		//! Lights are used by both offline and realtime renderer.
		//! By default, scene contains no lights.
		//!
		//! Note that even without lights, offline rendered scene may be still lit, by 
		//! - emissive materials used by static objects
		//! - environment, see setEnvironment()
		virtual void setLights(const RRLights& lights);

		//! Returns lights in scene, set by setLights().
		const RRLights& getLights() const;


		//! Illumination smoothing parameters.
		//
		//! Has reasonable default values for both realtime and offline rendering, but:
		//! - if needle artifacts appear, consider increased minFeatureSize
		//! - if seams between scene segments appear, consider increased minFeatureSize or vertexWeldDistance
		struct SmoothingParameters
		{
			//! Speed of surface subdivision, 0=no subdivision, 0.3=slow, 1=standard, 3=fast.
			//! \n Set 0 for the fastest results and realtime responsiveness. Illumination will be available in scene vertices.
			//!    0 is necessary for realtime calculation.
			//! \n Set 1 for higher quality, precalculations. Illumination will be available in adaptively subdivided surfaces.
			//!    You can set slower subdivision for higher quality results with less noise, calculation will be slower.
			//!    If you set higher speed, calculation will be faster, but results will contain high frequency noise.
			float subdivisionSpeed;
			//! Distance in world units. Vertices with lower or equal distance will be internally stitched into one vertex.
			//! Zero stitches only identical vertices, negative value generates no action.
			//! Non-stitched vertices at the same location create illumination discontinuity.
			//! \n If your geometry doesn't need stitching and adapter doesn't split vertices (Collada adapter does),
			//! make sure to set negative value, calculation will be faster.
			float vertexWeldDistance;
			//! Distance in world units. Smaller indirect light features will be smoothed. This could be imagined as a kind of blur.
			//! Use default 0 for no blur and watch for possible artifacts in areas with small geometry details
			//! and 'needle' triangles. Increase until artifacts disappear.
			//! 15cm (0.15 for game with 1m units) could be good for typical interior game.
			//! Only indirect lighting is affected, so even 15cm blur is mostly invisible.
			float minFeatureSize;
			//! Angle in radians, controls automatic smoothgrouping.
			//! Edges with smaller angle between face normals are smoothed out.
			//! Optimal value depends on your geometry, but reasonable value could be 0.33.
			float maxSmoothAngle;
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
			//! Intersection technique used for smoothed object.
			//! Techniques differ by speed and memory requirements.
			RRCollider::IntersectTechnique intersectTechnique;
			//! Sets default values at creation time.
			SmoothingParameters()
			{
				subdivisionSpeed = 0; // disabled
				maxSmoothAngle = 0.33f; // default angle for automatic smoothing
				vertexWeldDistance = 0; // weld enabled for identical vertices
				minFeatureSize = 0; // disabled
				ignoreSmallerAngle = 0; // ignores degerated triangles
				ignoreSmallerArea = 0; // ignores degerated triangles
				intersectTechnique = RRCollider::IT_BSP_FASTER;
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
		//!  If objects provide custom data channels (see RRChanneledData), all objects must have the same channels.
		//! \param smoothing
		//!  Static scene illumination smoothing.
		//!  Set NULL for default values.
		void setStaticObjects(const RRObjects& objects, const SmoothingParameters* smoothing);
		//! Returns static contents of scene, all static objects at once.
		const RRObjects& getStaticObjects();

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
			//! Sets default parameters. This is used if you send NULL instead of parameters.
			CalculateParameters()
			{
				qualityIndirectDynamic = 3;
				qualityIndirectStatic = 3;
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
		//! loadFireball() before first calculate() is recommended for games.
		//! If you haven't called it, first calculate() takes longer, creates internal solver.
		//!
		//! \param params
		//!  Optional calculation parameters. Currently used only by Fireball.
		virtual void calculate(CalculateParameters* params = NULL);

		//! Returns version of global illumination solution.
		//
		//! You may use this number to avoid unnecessary updates of illumination buffers.
		//! Store version together with your illumination buffers and don't update them
		//! (updateVertexBuffers(), updateLightmaps()) until this number changes.
		//!
		//! Version may be incremented only by calculate().
		unsigned getSolutionVersion() const;


		//! Parameters for updateVertexBuffer(), updateVertexBuffers(), updateLightmap(), updateLightmaps().
		//
		//! If you use \ref calc_fireball, only default parameters are supported,
		//! use NULL for default parameters.
		struct UpdateParameters
		{
			//! Requested type of data stored into updated (external) buffers. Doesn't affect data stored inside solver.
			//
			//! Attribute scaled enables conversion to custom scale before data are stored to buffer.
			//! It's usually set to 1 for 8bit sRGB lightmaps, 0 for float vertex buffers with cheap scaling in shader.
			//! \n Attributes direct/indirect specify what data are read from solver when applyCurrentSolution=true.
			//! \n Some attributes may be ignored by some updateXxx() functions.
			RRRadiometricMeasure measure;

			//! Include lights set by setLights() as a source of illumination.
			//! True makes calculation non-realtime.
			bool applyLights;

			//! Include environment set by setEnvironment() as a source of illumination.
			//! True makes calculation non-realtime.
			bool applyEnvironment;

			//! Include current solution as a source of indirect illumination.
			//
			//! Current solution in solver is updated by calculate()
			//! and possibly also by updateVertexBuffers() and updateLightmaps() (depends on parameters).
			//! \n Note that some functions restrict use of applyCurrentSolution
			//! and applyLights/applyEnvironment at the same time.
			bool applyCurrentSolution;

			//! Quality of computed illumination.
			//
			//! Time taken grows mostly linearly with this number.
			//! (When !applyCurrentSolution and applyLights and !applyEnvironment,
			//! faster path is used.)
			//!
			//! Higher number = higher quality.
			//! 1000 is usually sufficient for production, with small per pixel details
			//! and precise antialiasing computed.
			//! Lower quality is good for tests, with per pixel details, but with artifacts
			//! and aliasing.
			unsigned quality;

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

			//! Sets default parameters for fast realtime update.
			UpdateParameters()
			{
				measure = RM_IRRADIANCE_PHYSICAL_INDIRECT;
				applyLights = false;
				applyEnvironment = false;
				applyCurrentSolution = true;
				quality = 0;
				insideObjectsTreshold = 1;
				rugDistance = 0.001f;
				locality = 100000;
			}
		};

		//! Updates vertex buffer with direct, indirect or global illumination on single static object's surface.
		//
		//! \param objectNumber
		//!  Number of object in this scene.
		//!  Object numbers are defined by order in which you pass objects to setStaticObjects().
		//! \param vertexBuffer
		//!  Destination vertex buffer for indirect illumination.
		//! \param params
		//!  Parameters of the update process, NULL for the default parameters that
		//!  specify fast update (takes milliseconds) of RM_IRRADIANCE_PHYSICAL_INDIRECT data.
		//!  \n Only subset of all parameters is supported, see remarks.
		//!  \n params->measure specifies type of information stored in vertex buffer.
		//!  For typical scenario with per pixel direct illumination and per vertex indirect illumination,
		//!  use RM_IRRADIANCE_PHYSICAL_INDIRECT (faster) or RM_IRRADIANCE_CUSTOM_INDIRECT.
		//! \return
		//!  Number of vertex buffers updated, 0 or 1.
		//! \remarks
		//!  In comparison with more general updateVertexBuffers() function, this one
		//!  lacks paramsIndirect. However, you can still include indirect illumination
		//!  while updating single vertex buffer, see updateVertexBuffers() remarks.
		//! \remarks
		//!  In comparison with updateLightmap(),
		//!  updateVertexBuffer() is very fast but less general, it always reads lighting from current solver,
		//!  without final gather. In other words, it assumes that
		//!  params.applyCurrentSolution=1; applyLights=0; applyEnvironment=0.
		//!  For higher quality final gathered results, use updateVertexBuffers().
		virtual unsigned updateVertexBuffer(int objectNumber, RRBuffer* vertexBuffer, const UpdateParameters* params);

		//! Updates vertex buffers with direct, indirect or global illumination and bent normals on whole static scene's surface.
		//
		//! \param layerNumberLighting
		//!  Vertex colors for individual objects are stored into
		//!  getIllumination(objectNumber)->getLayer(layerNumberLighting)->vertexBuffer.
		//!  \n Negative number disables update of buffers.
		//! \param layerNumberBentNormals
		//!  Bent normals for individual objects are stored into
		//!  getIllumination(objectNumber)->getLayer(layerNumberBentNormals)->vertexBuffer.
		//!  \n Negative number disables update of buffers.
		//!  \n Bent normals are intentionally stored in separated layer, so you can save memory
		//!  by sharing single bent normal layer with multiple lighting layers.
		//! \param paramsDirect
		//!  Parameters of the update process specific for direct illumination component of final color.
		//!  With e.g. paramsDirect->applyLights, direct illumination created by lights 
		//!  set by setLights() is added to the final value stored into buffer.
		//!  \n params->measure specifies type of information stored in vertex buffer.
		//!  Use RM_IRRADIANCE_PHYSICAL_INDIRECT (faster, returns indirect lighting in physical scale,
		//!  can be cheaply converted to custom scale by shader)
		//!  or RM_IRRADIANCE_CUSTOM_INDIRECT (slower, returns indirect lighting in custom scale).
		//!  \n Set both paramsDirect and paramsIndirect to NULL for 'realtime' update
		//!  that fills buffers with indirect illumination in physical scale, read from current solution.
		//! \param paramsIndirect
		//!  Parameters of the update process specific for indirect illumination component of final color.
		//!  With e.g. paramsIndirect->applyLights, indirect illumination created by lights
		//!  set by setLights() is added to the final value stored into buffer.
		//!  For global illumination created by e.g. lights,
		//!  set both paramsDirect->applyLights and paramsIndirect->applyLights.
		//!  \n paramsIndirect->quality is ignored, only paramsDirect->quality matters.
		//!  Set to NULL for no indirect illumination.
		//! \return
		//!  Number of vertex buffers updated.
		//! \remarks
		//!  As a byproduct of calculation, internal state of solver (current solution)
		//!  is updated, so that it holds computed indirect illumination for sources
		//!  and quality specified in paramsIndirect.
		//!  Internal state is properly updated even when buffers don't exist so no other output is produced.
		//!  Following updateLightmap() or updateVertexBuffer() will include
		//!  this indirect lighting into computed lightmap or vertex buffer
		//!  if you set their params->applyCurrentSolution.
		//! \remarks
		//!  Update of selected objects (rather than all objects) is supported in multiple ways, use one of them:
		//!  - create vertex buffers for selected objects, make sure other vertex buffers are NULL and call updateVertexBuffers()
		//!  - for lighting in current solver, simply call updateVertexBuffer() for all selected objects
		//!  - call updateVertexBuffers(-1,-1,false,NULL,paramsIndirect) once to update current solution, call updateVertexBuffer(paramsDirect with applyCurrentSolution=true) for all selected objects
		virtual unsigned updateVertexBuffers(int layerNumberLighting, int layerNumberBentNormals, const UpdateParameters* paramsDirect, const UpdateParameters* paramsIndirect);

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

		//! Calculates and updates one lightmap with direct, indirect or global illumination on static object's surface.
		//
		//! Lightmap uses uv coordinates provided by RRMesh::getTriangleMapping().
		//! All uv coordinates must be in 0..1 range and two triangles
		//! may not overlap in texture space.
		//! If it's not satisfied, contents of created lightmap is undefined.
		//!
		//! Thread safe: yes if lightmap is safe.
		//!  \n Note1: LightsprintGL implementation of RRBuffer is not safe.
		//!  \n Note2: updateLightmap() uses multiple threads internally.
		//!
		//! Not supported if you use \ref calc_fireball.
		//!
		//! \param objectNumber
		//!  Number of object in this scene.
		//!  Object numbers are defined by order in which you pass objects to setStaticObjects().
		//! \param lightmap
		//!  Pixel buffer for storing calculated illumination.
		//!  Lightmap holds irradiance in custom scale, which is illumination
		//!  coming to object's surface, converted from physical W/m^2 units to your scale by RRScaler.
		//!  Lightmap could contain direct, indirect or global illumination, depending on
		//!  parameters you set in params.
		//!  May be NULL.
		//! \param bentNormals
		//!  Pixel buffer for storing calculated bent normals.
		//!  RGB values (range 0..1) are calculated from XYZ worldspace normalized normals
		//!  (range -1..1) by this formula: (XYZ+1)/2.
		//!  May be NULL.
		//! \param params
		//!  Parameters of the update process, NULL for the default parameters that
		//!  specify fast preview update (takes milliseconds).
		//!  Measure for ambient map in custom scale is RM_IRRADIANCE_CUSTOM_INDIRECT,
		//!  HDR GI lightmap with baked diffuse color is RM_EXITANCE_PHYSICAL etc.
		//! \param filtering
		//!  Parameters of lightmap filtering, set NULL for default ones.
		//! \return
		//!  Number of lightmaps and bent normal maps updated.
		//!  Zero when no update was executed because of invalid inputs.
		//!  Read system messages (RRReporter) for more details on failure.
		//! \remarks
		//!  In comparison with more general updateLightmaps() function, this one
		//!  lacks paramsIndirect. However, you can still include indirect illumination
		//!  while updating single lightmap, see updateLightmaps() remarks.
		virtual unsigned updateLightmap(unsigned objectNumber, RRBuffer* lightmap, RRBuffer* bentNormals, const UpdateParameters* params, const FilteringParameters* filtering);

		//! Calculates and updates all lightmaps with direct, indirect or global illumination on static scene's surfaces.
		//
		//! This is more powerful full scene version of limited single object's updateLightmap().
		//!
		//! Not supported if you use \ref calc_fireball.
		//!
		//! \param layerNumberLighting
		//!  Lightmaps for individual objects are stored into
		//!  getIllumination(objectNumber)->getLayer(layerNumber)->pixelBuffer.
		//!  \n Negative number disables update of buffers.
		//! \param layerNumberBentNormals
		//!  Bent normal maps for individual objects are stored into
		//!  getIllumination(objectNumber)->getLayer(layerNumberBentNormals)->pixelBuffer.
		//!  \n Negative number disables update of buffers.
		//! \param paramsDirect
		//!  Parameters of the update process specific for direct illumination component of final color.
		//!  With e.g. paramsDirect->applyLights, direct illumination created by lights 
		//!  set by setLights() is added to the final value stored into lightmap.
		//! \param paramsIndirect
		//!  Parameters of the update process specific for indirect illumination component of final color.
		//!  With e.g. paramsIndirect->applyLights, indirect illumination created by lights
		//!  set by setLights() is added to the final value stored into lightmap.
		//!  For global illumination created by e.g. lights,
		//!  set both paramsDirect->applyLights and paramsIndirect->applyLights.
		//!  \n paramsIndirect->quality is ignored, only paramsDirect->quality matters.
		//!  Set to NULL for no indirect illumination.
		//! \param filtering
		//!  Parameters of lightmap filtering, set NULL for default ones.
		//! \return
		//!  Number of lightmaps updated.
		//!  Zero when no update was executed because of invalid inputs.
		//!  Read system messages (RRReporter) for more details on failure.
		//! \remarks
		//!  As a byproduct of calculation, internal state of solver (current solution)
		//!  is updated, so that it holds computed indirect illumination for sources
		//!  and quality specified in paramsIndirect.
		//!  Internal state is properly updated even when buffers don't exist (so no other output is produced).
		//!  Following updateLightmap() or updateVertexBuffer() will include
		//!  this indirect lighting into computed lightmap or vertex buffer
		//!  if you set their params->applyCurrentSolution.
		//! \remarks
		//!  Update of selected objects (rather than all objects) is supported in multiple ways, use one of them:
		//!  - create pixel buffers for selected objects, make sure other pixel buffers are NULL and call updateLightmaps()
		//!  - if you don't need indirect illumination, simply call updateLightmap() for all selected objects
		//!  - call updateLightmaps(-1,-1,false,NULL,paramsIndirect) once to update current solution, call updateLightmap(paramsDirect with applyCurrentSolution=true) for all selected objects
		virtual unsigned updateLightmaps(int layerNumberLighting, int layerNumberBentNormals, const UpdateParameters* paramsDirect, const UpdateParameters* paramsIndirect, const FilteringParameters* filtering);

		//! Optional update of illumination cache, makes updateEnvironmentMap() faster.
		//
		//! Depends on geometry but not on lighting, may be executed before static scene lighting is computed.
		//! Call it when CPU is not fully loaded (on background or while waiting) to use otherwise wasted cycles.
		//! If you don't have such cycles, don't call it at all.
		//!
		//! Reads RRObjectIllumination variables with 'envMap' in name, update them before calling this function.
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
		//! Reads RRObjectIllumination variables with 'envMap' in name, update them before calling this function.
		//!
		//! Thread safe: yes if specularMap->setValues() and diffuseMap->setValues() are safe,
		//!  may be called from multiple threads at the same time if setValues() may be.
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
		void reportMaterialChange();

		//! Reports that position/rotation/shape of one or more lights has changed.
		//
		//! Call this function when any light in scene changes any property,
		//! so that direct illumination changes.
		//! For extra precision, you may call it also after each movement of shadow caster,
		//! however, smaller moving objects typically don't affect indirect illumination,
		//! so it's common optimization to not report illumination change in such situation.
		//! \param strong Hint for solver, was change in direct illumination significant?
		//!  Set true if not sure. Set false only if you know that change was minor.
		//!  False improves performance, but introduces errors if change is not minor.
		void reportDirectIlluminationChange(bool strong);

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
		void reportInteraction();


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
		//! When used, non-realtime functions like updateLightmaps() are not supported,
		//! but realtime functions like updateVertexBuffers() and updateEnvironmentMaps() are faster
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
		//! When used, non-realtime functions like updateLightmaps() are not supported,
		//! but realtime functions like updateVertexBuffers() and updateEnvironmentMaps() are faster
		//! and produce better results using less memory.
		//!
		//! \param filename
		//!  File with data to load, previously created by buildFireball().
		//!  Set NULL to use automatically generated name.
		//! \return
		//!  True if successful.
		bool loadFireball(const char* filename);


		//! Verifies data in solver and reports problems found using RRReporter.
		//
		//! While all precomputed lighting and cheap-to-detect realtime lighting problems
		//! are reported immediately even without verify(),
		//! more expensive realtime lighting checks are done only on this request.
		void verify();


		//! Returns multiObject created by merging all static objects in scene, see setStaticObjects().
		const RRObject* getMultiObjectCustom() const;

		//! As getMultiObjectCustom, but with materials converted to physical space.
		const RRObjectWithPhysicalMaterials* getMultiObjectPhysical() const;

		//! Loads illumination layer from disk. (Load of vertex buffers is temporarily disabled.)
		//! It is shortcut for calling RRBuffer::save() on all buffers.
		unsigned loadIllumination(const char* path, unsigned layerNumber);
		//! Saves illumination layer to disk.
		//! It is shortcut for calling RRBuffer::load() on all buffers.
		unsigned saveIllumination(const char* path, unsigned layerNumber);

	protected:
		//! Detects direct illumination on all faces in scene and returns it in array of RGBA values.
		//
		//! Necessary for realtime solver, rarely needed for offline calculations.
		//!
		//! detectDirectIllumination() is one of three paths for feeding solver with direct lighting,
		//! other two paths are setLights() and setEnvironment().
		//! While all three paths may be combined in offline calculation, detectDirectIllumination()
		//! is rarely needed there and doesn't have to be implemented for good results, other paths
		//! are sufficient.
		//! On the other end, detectDirectIllumination() is the only path for feeding realtime solver with direct lighting,
		//! so it must be implemented for realtime rendering of dynamic lights.
		//!
		//! This function is automatically called from calculate(),
		//! when solver needs current direct illumination values and you
		//! previously called reportDirectIlluminationChange(), so it's known,
		//! that direct illumination values in solver are outdated.
		//!
		//! What exactly is considered "direct illumination" here is implementation defined,
		//! but for consistent results, it should be complete illumination
		//! generated by your renderer, except for indirect illumination (e.g. constant ambient).
		//!
		//! Detected values are average per-triangle direct-lighting irradiances in custom scale.
		//! In other words, detect average triangle colors when direct lighting+shadows are applied,
		//! but materials are not. So result for fully shadowed triangle is 0, fully lit
		//! 0xffffff00 (00 is alpha, it is ignored).
		//!
		//! Returned array must stay valid at least until next detectDirectIllumination() call.
		//! Array is not automatically deleted, you are expected to delete it later.
		//! You are free to return always the same array or each time different one.
		//! Best practice for managing array of results is to allocate it in constructor, free it in destructor,
		//! always return the same pointer.
		//!
		//! Skeleton of implementation:
		//!\code
		//!virtual unsigned* detectDirectIllumination()
		//!{
		//!  unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
		//!  // for all triangles in static scene
		//!  for(unsigned triangleIndex=0;triangleIndex<numTriangles;triangleIndex++)
		//!  {
		//!    myIrradiances[triangleIndex] = detectedIrradiance...;
		//!  }
		//!  return myIrradiances;
		//!}
		//!\endcode
		//!
		//! \return Pointer to array of detected average per-triangle direct-lighting irradiances in custom scale
		//!  (= average triangle colors when material is not applied).
		//!  Values are stored in RGBA8 format.
		//!  Return NULL when direct illumination was not detected for any reason, this
		//!  function will be called again in next calculate().
		virtual unsigned* detectDirectIllumination() {return NULL;}

		//! Sets factor that multiplies intensity of detected direct illumination.
		//
		//! Default value is 1 = realistic.
		//! Calling it with boost=2 makes detected direct illumination
		//! for solver 2x stronger, so also computed indirect illumination is 2x stronger.
		//! You would get the same results if you multiply computed indirect lighting in shader,
		//! but this way it's for free, without overhead.
		void setDirectIlluminationBoost(RRReal boost);


	private:

		//! Detects direct illumination on all faces in scene and sends it to the solver.
		//
		//! This is more general version of detectDirectIllumination(),
		//! used for non-realtime calculations.
		//! It supports all light sources: current solver, environment, lights.
		//! \param paramsDirect
		//!  Parameters of update process.
		//! \param updateBentNormals
		//!  Set it always to false.
		virtual bool updateSolverDirectIllumination(const UpdateParameters* paramsDirect, bool updateBentNormals);

		//! Detects direct illumination, feeds solver and calculates until indirect illumination values are available.
		virtual bool updateSolverIndirectIllumination(const UpdateParameters* paramsIndirect, unsigned benchTexels, unsigned benchQuality);

		void       calculateCore(float improveStep,CalculateParameters* params=NULL);
		bool       gatherPerTriangle(const UpdateParameters* aparams, struct ProcessTexelResult* results, unsigned numResultSlots);
		unsigned   updateVertexBufferFromPerTriangleData(unsigned objectHandle, RRBuffer* vertexBuffer, RRVec3* perTriangleData, unsigned stride) const;
		void       updateVertexLookupTableDynamicSolver();
		void       updateVertexLookupTablePackedSolver();
		struct Private;
		Private* priv;
		friend class GatheredIrradianceHemisphere;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRLicense
	//! Everything related to your license number.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRLicense
	{
	public:
		//! States of your license.
		enum LicenseStatus
		{
			VALID,       ///< Valid license.
			EXPIRED,     ///< Expired license.
			WRONG,       ///< Wrong license. May be also double precision broken by Direct3D, see loadLicense().
			NO_INET,     ///< No internet connection to verify license.
			UNAVAILABLE, ///< Temporarily unable to verify license, try later.
		};
		//! Loads license from file.
		//
		//! Must be called before any other work with library.
		//!
		//! Uses doubles.
		//! If you create Direct3D device
		//! before licence check, use D3DCREATE_FPU_PRESERVE flag,
		//! otherwise Direct3D breaks double precision for whole
		//! application including DLLs and this function fails.
		//!
		//! May connect to Lightsprint servers for verification.
		//!
		//! \return Result of license check.
		//!  If it's not VALID, lighting computed by other functions
		//!  may be incorrect.
		static LicenseStatus loadLicense(const char* filename);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// Checks dll at runtime, detects wrong dll configuration or version
	//
	// First step of your application should be
	//   if(!RR_INTERFACE_OK) terminate_with_message(RR_INTERFACE_MISMATCH_MSG);
	//////////////////////////////////////////////////////////////////////////////

	//! Returns id of interface offered by library.
	RR_API unsigned RR_INTERFACE_ID_LIB();
	// Returns id of interface expected by app.
	#define RR_INTERFACE_ID_APP() unsigned( sizeof(rr::RRDynamicSolver) + 1 )
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
	#define RR_INTERFACE_MISMATCH_MSG "RRVision dll version mismatch.\nLibrary has interface: %d %s\nApplication expects  : %d %s\n",rr::RR_INTERFACE_ID_LIB(),rr::RR_INTERFACE_DESC_LIB(),RR_INTERFACE_ID_APP(),RR_INTERFACE_DESC_APP()

} // namespace

#endif
