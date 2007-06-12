#ifndef RRDYNAMICSOLVER_H
#define RRDYNAMICSOLVER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRDynamicSolver.h
//! \brief RRDynamicSolver - global illumination solver for dynamic scenes
//! \version 2007.6.12
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <vector>
#include "RRIllumination.h"
#include "RRStaticSolver.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Direct light source, directional or point light with programmable function.
	//
	//! Thread safe: yes, may be accessed by any number of threads simultaneously.
	//! All custom implementations must be thread safe too.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRLight
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		//! Types of lights.
		enum Type
		{
			//! Infinitely distant light source, all light rays are parallel (direction).
			DIRECTIONAL,
			//! Point or spot light source, all light rays start in one point (position).
			POINT,
		};
		//! Type of light source.
		Type type;

		//! Position of light source in world space. Relevant only for POINT light.
		RRVec3 position;

		//! Direction of light in world space. Relevant only for DIRECTIONAL light.
		//! Doesn't have to be normalized.
		RRVec3 direction;

		//! Irradiance in physical scale, programmable color, distance and spotlight attenuation function.
		//
		//! \param receiverPosition
		//!  Position of point in world space illuminated by this light.
		//! \param scaler
		//!  Currently active custom scaler, provided for your convenience.
		//!  You may compute irradiance directly in physical scale and don't use it,
		//!  which is the most efficient way,
		//!  but if you calculate in custom scale, convert your result to physical scale using scaler.
		//! \return
		//!  Irradiance at receiverPosition, in physical scale [W/m^2],
		//!  assuming that receiver is oriented towards light.
		virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const = 0;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates directional light without distance attenuation.
		//
		//! It is good approximation of sun, which is so distant, that its
		//! distance attenuation is hardly visible on our small scale.
		//! \param direction
		//!  Direction of light in world space.
		//! \param irradiance
		//!  Irradiance at receiver, assuming it is oriented towards light.
		static RRLight* createDirectionalLight(const RRVec3& direction, const RRVec3& irradiance);

		//! Creates omnidirectional point light with physically correct distance attenuation.
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance at distance 1, assuming that receiver is oriented towards light.
		static RRLight* createPointLight(const RRVec3& position, const RRVec3& irradianceAtDistance1);

		//! Creates spot point light with physically correct distance attenuation.
		//
		//! Light rays start in position and go in directions up to outerAngleRad far from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance at distance 1, assuming that receiver is oriented towards light.
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Angle in radians. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Angle in radians. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		static RRLight* createSpotLight(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRIlluminatedObject
	//! Static 3d object with storage space for calculated illumination.
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
	//! Set of illuminated objects with std::vector interface.
	//
	//! This is usual product of adapter that creates Lightsprint interface for external 3d scene.
	//! You may use it for example to
	//! - send it to RRDynamicSolver and calculate global illumination
	//! - manipulate this set before sending it to RRDynamicSolver, e.g. remove moving objects
	//! - render it immediately, without calculating global illumination
	//! - render it when global illumination is calculated
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRObjects : public std::vector<RRIlluminatedObject>
	{
	public:
		virtual ~RRObjects() {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRLights
	//! Set of lights with std::vector interface.
	//
	//! This is usual product of adapter that creates Lightsprint interface for external 3d scene.
	//! You may use it for example to
	//! - send it to RRDynamicSolver and calculate global illumination
	//! - manipulate this set before sending it to RRDynamicSolver, e.g. remove moving lights
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRLights : public std::vector<RRLight*>
	{
	public:
		virtual ~RRLights() {};
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
	//!   save it to disk using RRIlluminationPixelBuffer::save()
	//!   or RRIlluminationEnvironmentMap::save() functions.
	//!
	//! Custom access to GPU and your renderer is not implemented here.
	//! You may implement it in your RRDynamicSolver subclass
	//! or use RRRealtimeRadioosityGL, that implements GPU access using OpenGL 2.0.
	//!
	//! Sample HelloRealtimeRadiosity shows both typical usage scenarios,
	//! rendering with realtime global illumination and precalculations.
	//!
	//! It is not allowed to create and use multiple instances at the same time.
	//!
	//! Thread safe: Partially.
	//!  All updateXxxx() functions may be called from multiple threads at the same time,
	//!  if you don't ask them to create missing buffers.
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
		void setScaler(RRScaler* scaler);

		//! Returns scaler used by this scene i/o operations, set by setScaler().
		const RRScaler* getScaler() const;


		//! Sets environment around scene.
		//
		//! By default, scene contains no environment, which is the same as black environment.
		//! Environment matters only for updateLightmap() and updateLightmaps(), it is not used by realtime GI solver.
		//! \param environment
		//!  HDR map of environment around scene.
		//!  Its RRIlluminationEnvironmentMap::getValue() should return
		//!  values in physical scale.
		//!  Note that environment is not adopted, you are still responsible for deleting it
		//!  when it's no longer needed.
		void setEnvironment(RRIlluminationEnvironmentMap* environment);

		//! Returns environment around scene, set by setEnvironment().
		const RRIlluminationEnvironmentMap* getEnvironment() const;


		//! Sets lights in scene, all at once.
		//
		//! By default, scene contains no lights.
		//! Lights matter only for updateLightmap() and updateLightmaps(), they are not used by realtime GI solver.
		//!
		//! Note that even without lights, scene may be still lit, by 
		//! - custom direct illumination, see detectDirectIllumination().
		//! - emissive materials used by static objects
		//! - environment, see setEnvironment()
		void setLights(const RRLights& lights);

		//! Returns lights in scene, set by setLights().
		const RRLights& getLights() const;


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
		//! \param objects
		//!  Static contents of your scene, set of static objects.
		//!  Objects should not move (in 3d space) during our lifetime.
		//!  Object's getTriangleMaterial should return values in custom scale.
		//!  For now, all objects must have the same data channels (see RRChanneledData).
		//! \param smoothing
		//!  Static scene illumination smoothing.
		//!  Set NULL for default values.
		void setObjects(RRObjects& objects, const RRStaticSolver::SmoothingParameters* smoothing);

		//! Returns number of static objects in scene.
		unsigned getNumObjects() const;

		//! Returns i-th static object in scene.
		RRObject* getObject(unsigned i);

		//! Returns illumination of i-th static object in scene.
		RRObjectIllumination* getIllumination(unsigned i);
		//! Returns illumination of i-th static object in scene.
		const RRObjectIllumination* getIllumination(unsigned i) const;

		//! Calculates and improves indirect illumination on static objects.
		//
		//! To be called once per frame while rendering. To be called even when
		//! rendering is paused, calculation runs on background.
		//!
		//! With architect edition (no precalculations), it helps to improve performance if
		//! you don't repeatedly render scene that doesn't change, for example in game editor.
		//! Note that user can't see any difference, you only save time and power 
		//! by not rendering the same image again.
		//! calculate() uses these periods for more intense calculations,
		//! creating higher quality illumination.
		//! It detects these periods from you not calling reportDirectIlluminationChange() and reportInteraction().
		//!
		//! \return
		//!  IMPROVED when any vertex or pixel buffer was updated with improved illumination.
		//!  NOT_IMPROVED otherwise. FINISHED = exact solution was reached, no further calculations are necessary.
		RRStaticSolver::Improvement calculate();

		//! Returns version of global illumination solution.
		//
		//! You may use this number to avoid unnecessary updates of illumination buffers.
		//! Store version together with your illumination buffers and don't update them
		//! (updateVertexBuffers(), updateLightmaps()) until this number changes.
		//!
		//! Version may be incremented only by calculate().
		unsigned getSolutionVersion() const;


		//! Parameters for updateVertexBuffer(), updateVertexBuffers(), updateLightmap(), updateLightmaps().
		struct UpdateParameters
		{
			//! Requested type of results, applies only to updates of external buffers.
			//
			//! Attributes direct/indirect specify what parts of solver data read when applyCurrentSolution=true,
			//! they have no influence on applyLights and applyEnvironment.
			RRRadiometricMeasure measure;

			//! Use current indirect solution computed by compute() as the only source of illumination.
			bool applyCurrentSolution;

			//! Use lights set by setLights() as one of sources of illumination.
			bool applyLights;

			//! Use environment set by setEnvironment() as one of sources of illumination.
			bool applyEnvironment;

			//! Quality of computed illumination coming from current solution and environment (not from lights).
			//! Higher number = higher quality in longer time.
			//! For 1000 and higher, lightmap contains small per pixel details,
			//! and update takes minutes.
			//! 0-999 is faster, with per pixel details, but not recommended due to artifacts.
			//!
			//! If applyCurrentSolution is enabled and quality is zero,
			//! very fast (milliseconds) update is executed and lightmap contains
			//! nearly no noise, but small per pixel details are missing.
			unsigned quality;

			//! 0..1 ratio, texels with greater fraction of hemisphere 
			//! seeing inside objects (or below rug, see rugDistance)
			//! are masked away.
			//! Default value 1 disables any correction.
			RRReal insideObjectsTreshold;

			//! Distance in world space, illumination coming from closer surfaces is masked away.
			//! Set it slightly above distance of rug and ground, to prevent darkness
			//! under the rug leaking half texel outside (instead, light around rug will
			//! leak under the rug). Set it to zero to disable any corrections.
			RRReal rugDistance;

			//! Turns on diagnostic output, generated map contains diagnostic values.
			bool diagnosticOutput;

			//! Sets default parameters for fast realtime update.
			UpdateParameters()
			{
				measure = RM_IRRADIANCE_PHYSICAL_INDIRECT;
				applyCurrentSolution = true;
				applyLights = false;
				applyEnvironment = false;
				quality = 0;
				insideObjectsTreshold = 1;
				rugDistance = 0.001f;
				diagnosticOutput = false;
			}
		};

		//! Updates vertex buffer with direct, indirect or global illumination on single static object's surface.
		//
		//! \param objectNumber
		//!  Number of object in this scene.
		//!  Object numbers are defined by order in which you pass objects to setObjects().
		//! \param vertexBuffer
		//!  Destination vertex buffer for indirect illumination.
		//! \param params
		//!  Parameters of the update process, NULL for the default parameters that
		//!  specify fast update (takes milliseconds) of RM_IRRADIANCE_PHYSICAL_INDIRECT data.
		//!  params->measure specifies type of information stored in vertex buffer.
		//!  For typical scenario with per pixel direct illumination and per vertex indirect illumination,
		//!  use RM_IRRADIANCE_PHYSICAL_INDIRECT (faster) or RM_IRRADIANCE_CUSTOM_INDIRECT.
		//! \return
		//!  Number of vertex buffers updated, 0 or 1.
		virtual unsigned updateVertexBuffer(unsigned objectNumber, RRIlluminationVertexBuffer* vertexBuffer, const UpdateParameters* params);

		//! Updates vertex buffers with direct, indirect or global illumination on whole static scene's surface.
		//
		//! \param layerNumber
		//!  Vertex colors for individual objects are stored into
		//!  getIllumination(objectNumber)->getLayer(layerNumber)->vertexBuffer.
		//! \param createMissingBuffers
		//!  If destination buffer doesn't exist, it is created by newVertexBuffer().
		//! \param paramsDirect
		//!  Parameters of the update process specific for direct illumination component of final color.
		//!  With e.g. paramsDirect->applyLights, direct illumination created by lights 
		//!  set by setLights() is added to the final value stored into buffer.
		//!  \n params->measure specifies type of information stored in vertex buffer.
		//!  For typical scenario with per pixel direct illumination and per vertex indirect illumination,
		//!  use RM_IRRADIANCE_PHYSICAL_INDIRECT (faster) or RM_IRRADIANCE_CUSTOM_INDIRECT.
		//!  Set to NULL for no indirect illumination.
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
		virtual unsigned updateVertexBuffers(unsigned layerNumber, bool createMissingBuffers, const UpdateParameters* paramsDirect, const UpdateParameters* paramsIndirect);

		//! Calculates and updates one lightmap with direct, indirect or global illumination on static object's surface.
		//
		//! Lightmap uses uv coordinates provided by RRMesh::getTriangleMapping().
		//! All uv coordinates must be in 0..1 range and two triangles
		//! may not overlap in texture space.
		//! If it's not satisfied, contents of created lightmap is undefined.
		//!
		//! Thread safe: yes if lightmap is safe.
		//!  \n Note1: LightsprintGL implementation of RRIlluminationPixelBuffer is not safe.
		//!  \n Note2: updateLightmap() uses multiple threads internally.
		//!
		//! \param objectNumber
		//!  Number of object in this scene.
		//!  Object numbers are defined by order in which you pass objects to setObjects().
		//! \param lightmap
		//!  Pixel buffer for storing calculated ambient map.
		//!  Lightmap holds irradiance in custom scale, which is illumination
		//!  coming to object's surface, converted from physical W/m^2 units to your scale by RRScaler.
		//!  Lightmap could contain direct, indirect or global illumination, depending on
		//!  parameters you set in params.
		//! \param params
		//!  Parameters of the update process, NULL for the default parameters that
		//!  specify fast preview update (takes milliseconds).
		//!  Measure for ambient map in custom scale is RM_IRRADIANCE_CUSTOM_INDIRECT,
		//!  HDR GI lightmap with baked diffuse color is RM_EXITANCE_PHYSICAL etc.
		//! \return
		//!  Number of lightmaps updated.
		//!  Zero when no update was executed because of invalid inputs.
		//!  Read system messages (RRReporter) for more details on failure.
		virtual unsigned updateLightmap(unsigned objectNumber, RRIlluminationPixelBuffer* lightmap, const UpdateParameters* params);

		//! Calculates and updates all lightmaps with direct, indirect or global illumination on static scene's surfaces.
		//
		//! This is more powerful full scene version of limited single object's updateLightmap().
		//!
		//! \param layerNumber
		//!  Lightmaps for individual objects are stored into
		//!  getIllumination(objectNumber)->getLayer(layerNumber)->pixelBuffer.
		//! \param createMissingBuffers
		//!  If destination buffer doesn't exist, it is created by newPixelBuffer().
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
		//! \return
		//!  Number of lightmaps updated.
		//!  Zero when no update was executed because of invalid inputs.
		//!  Read system messages (RRReporter) for more details on failure.
		virtual unsigned updateLightmaps(unsigned layerNumber, bool createMissingBuffers, const UpdateParameters* paramsDirect, const UpdateParameters* paramsIndirect);

		//! Calculates and updates environment maps for dynamic object at given position.
		//
		//! Generates specular and diffuse environment maps with object's indirect illumination
		//! \n- specular map is to be sampled (by reflected view direction) in object's glossy pixels
		//! \n- diffuse map is to be sampled (by surface normal) in object's rough pixels
		//!
		//! This function is all you need for global illumination of dynamic objects,
		//! you don't have to import them into solver.
		//! (but you still need to import static objects and call calculate() regularly)
		//!
		//! Thread safe: yes if specularMap->setValues and diffuseMap->setValues is safe.
		//!  may be called from multiple threads at the same time if setValues may be.
		//!  \n Note1: LightsprintGL implementation of RRIlluminationEnvironmentMap is safe.
		//!  \n Note2: updateEnvironmentMaps() uses multiple threads internally.
		//!
		//! \param objectCenter
		//!  Center of your dynamic object in world space coordinates.
		//! \param gatherSize
		//!  Number of samples gathered from scene will be gatherSize*gatherSize*6.
		//!  It doesn't have to be power of two.
		//!  Increase for higher quality, decrease for higher speed.
		//!  Size 16 is good, but 4 could be enough if you don't need specular map.
		//! \param specularMap
		//!  Your custom environment map for rendering object's specular reflection.
		//!  To be filled with calculated data in custom scale.
		//!  Set to NULL for no specular reflection.
		//! \param specularSize
		//!  Size of environment map for rendering object's specular reflection.
		//!  Map will have 6*specularSize*specularSize pixels.
		//!  If your custom specularMap requires size to be power of two, make sure you enter correct size here.
		//!  Use smaller size for faster update.
		//!  Size 16 is good.
		//! \param diffuseMap
		//!  Your custom environment map for rendering object's diffuse reflection.
		//!  To be filled with calculated data in custom scale.
		//!  Set to NULL for no diffuse reflection.
		//! \param diffuseSize
		//!  Size of environment map for rendering object's diffuse reflection.
		//!  Map will have 6*diffuseSize*diffuseSize pixels.
		//!  If your custom diffuseMap requires size to be power of two, make sure you enter correct size here.
		//!  Use smaller size for faster update.
		//!  Size 4 is good.
		//! \return
		//!  Number of environment maps updated.
		unsigned updateEnvironmentMaps(RRVec3 objectCenter, unsigned gatherSize,
			unsigned specularSize, RRIlluminationEnvironmentMap* specularMap,
			unsigned diffuseSize, RRIlluminationEnvironmentMap* diffuseMap);


		//! Reports that appearance of one or more materials has changed.
		//!
		//! Call this when you change material properties in your material editor.
		void reportMaterialChange();

		//! Reports that position/rotation/shape of one or more lights has changed.
		//
		//! Call this function when any light in scene changes any property,
		//! so that direct illumination changes.
		//! For extra precision, you may call it also after each dynamic object movement,
		//! however, smaller moving objects typically don't affect indirect illumination,
		//! so it's common optimization to 
		//! \param strong Hint for solver, was change in illumination strong, does illumination 
		//!  change significantly? Good hint improves performance.
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


		//! Returns multiObject created by merging all objects present in scene.
		//! MultiObject is created when you insert objects and call calculate().
		const RRObject* getMultiObjectCustom() const;

		//! As getMultiObjectCustom, but with materials converted to physical space.
		const RRObjectWithPhysicalMaterials* getMultiObjectPhysical() const;

		//! As getMultiObjectPhysical, but with space for storage of detected direct illumination.
		RRObjectWithIllumination* getMultiObjectPhysicalWithIllumination();

		//! Returns the scene for direct queries of single triangle/vertex illumination.
		//
		//! Scene is created from getMultiObjectPhysicalWithIllumination()
		//! when you insert objects and call calculate().
		const RRStaticSolver* getStaticSolver() const;

	protected:
		//! Autodetects material properties of all materials present in scene.
		//
		//! To be implemented by you.
		//! New values must appear in RRObject-s already present in scene.
		//! \n\n It is perfectly ok to write empty implementation if your application never modifies materials.
		virtual void detectMaterials() = 0;

		//! Detects direct illumination on all faces in scene and sends it to the solver.
		//
		//! Used by realtime solver.
		//! This function is automatically called from calculate(),
		//! when solver needs current direct illumination values and you
		//! previously called reportDirectIlluminationChange(), so it's known,
		//! that direct illumination values in solver are outdated.
		//!
		//! For realtime solution, don't pass light coords and other 
		//! light properties into solver, complete information should be encoded
		//! in per-triangle irradiances detected here.
		//!
		//! What exactly is "direct illumination" here is implementation defined,
		//! but for consistent results, it should be complete illumination
		//! generated by your renderer, except indirect illumination (e.g. constant ambient).
		//!
		//! Skeleton of implementation (shows sending values into solver, not detecting them):
		//!\code
		//!virtual bool detectDirectIllumination()
		//!{
		//!  unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
		//!  // for all triangles in static scene
		//!  for(unsigned triangleIndex=0;triangleIndex<numTriangles;triangleIndex++)
		//!  {
		//!    // set triangle illumination
		//!    getMultiObjectPhysicalWithIllumination()->setTriangleIllumination(triangleIndex,,);
		//!  }
		//!  return true;
		//!}
		//!\endcode
		//!
		//! \return True when direct illumination values in solver were successfully updated.
		//!  When false is returned for any reason, 
		//!  direct illumination in solver is considered invalid and this
		//!  function will be called again in next calculate().
		virtual bool detectDirectIllumination() = 0;


		//! Detects direct illumination on all faces in scene and sends it to the solver.
		//
		//! This is more general version of detectDirectIllumination(),
		//! used for non-realtime calculations.
		//! It supports all light sources: current solver, environment, lights.
		virtual bool updateSolverDirectIllumination(const UpdateParameters* paramsDirect);

		//! Detects direct illumination, feeds solver and calculates until indirect illumination values are available.
		virtual bool updateSolverIndirectIllumination(const UpdateParameters* paramsIndirect, unsigned benchTexels, unsigned benchQuality);

		//! Returns new vertex buffer (for indirect illumination) in your custom format.
		//
		//! Default implementation allocates 3 floats per vertex in RAM.
		//! This is good for editor, but you may want to use 4 bytes per vertex in game to save memory,
		//! so reimplement this function and return different vertex buffer class.
		//! You may even implement monochromatic (1 float or 1 byte) format if you don't need color bleeding.
		virtual RRIlluminationVertexBuffer* newVertexBuffer(unsigned numVertices);

		//! Returns new pixel buffer (for ambient map) in your custom format.
		//
		//! If you don't want to use ambient maps, return NULL.
		//! Default implementation returns NULL.
		virtual RRIlluminationPixelBuffer* newPixelBuffer(RRObject* object);

	private:
		enum ChangeStrength
		{
			NO_CHANGE,
			SMALL_CHANGE,
			BIG_CHANGE,
		};
		// calculate
		RRObjects  objects;
		RRLights   lights;
		const RRIlluminationEnvironmentMap* environment;
		RRStaticSolver::SmoothingParameters smoothing;
		bool       dirtyMaterials;
		bool       dirtyGeometry;
		ChangeStrength dirtyLights; // 0=no light change, 1=small light change, 2=strong light change
		bool       dirtyResults;
		long       lastInteractionTime;
		long       lastCalcEndTime;
		long       lastReadingResultsTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriod;
		RRObject*  multiObjectCustom;
		RRObjectWithPhysicalMaterials* multiObjectPhysical;
		RRObjectWithIllumination* multiObjectPhysicalWithIllumination;
		protected: // temporary
		RRStaticSolver*   scene;
		unsigned   solutionVersion;
		private:
		RRReal     minimalSafeDistance; // minimal distance safely used in current scene, proportional to scene size
		RRStaticSolver::Improvement calculateCore(float improveStep);
		// read results
		RRScaler*  scaler;
		void       updateVertexLookupTable();
		std::vector<std::vector<std::pair<unsigned,unsigned> > > preVertex2PostTriangleVertex; ///< readResults lookup table
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// Sanity checks.
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
	#define RR_INTERFACE_MISMATCH_MSG "RRDynamicSolver version mismatch.\nLibrary has interface: %d %s\nApplication expects  : %d %s\n",rr::RR_INTERFACE_ID_LIB(),rr::RR_INTERFACE_DESC_LIB(),RR_INTERFACE_ID_APP(),RR_INTERFACE_DESC_APP()

} // namespace

#endif
