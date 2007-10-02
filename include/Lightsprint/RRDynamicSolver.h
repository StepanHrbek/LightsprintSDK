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
#include "RRStaticSolver.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Direct light source, directional or point light with programmable function.
	//
	//! Custom lights with this interface may be created.
	//! Predefined point and spot lights support only physically correct
	//! distance attenuation, but more options will be added soon.
	//!
	//! Thread safe: yes, may be accessed by any number of threads simultaneously.
	//! All custom implementations must be thread safe too.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRLight : public RRUniformlyAllocated
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
		//!  but if you calculate in custom scale, convert your result to physical
		//!  scale using scaler->getPhysicalScale() before returning it.
		//!  \n Lightsprint calculates internally in physical scale, that's why
		//!  it's more efficient to expect result in physical scale
		//!  rather than in screen colors or any other custom scale.
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
		//!  Irradiance in physical scale at receiver, assuming it is oriented towards light.
		static RRLight* createDirectionalLight(const RRVec3& direction, const RRVec3& irradiance);

		//! Creates omnidirectional point light with physically correct distance attenuation.
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance in physical scale at distance 1, assuming that receiver is oriented towards light.
		static RRLight* createPointLight(const RRVec3& position, const RRVec3& irradianceAtDistance1);

		//! Creates omnidirectional point light with radius/exponent based distance attenuation (physically incorrect).
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorAtDistance0
		//!  Irradiance in custom scale (usually screen color) of lit surface at distance 0.
		//! \param radius
		//!  Distance in world space, where light disappears due to its distance attenuation.
		//!  So light has effect in sphere of given radius.
		//! \param fallOffExponent
		//!  Distance attenuation in custom scale is computed as pow(MAX(0,1-distance/radius),fallOffExponent).
		static RRLight* createPointLightRadiusExp(const RRVec3& position, const RRVec3& colorAtDistance0, RRReal radius, RRReal fallOffExponent);

		//! Creates omnidirectional point light with polynom based distance attenuation (physically incorrect).
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorAtDistance0
		//!  Irradiance in custom scale (usually screen color) of lit surface at distance 0.
		//! \param polynom
		//!  Distance attenuation in custom scale is computed as 1/(polynom[0]+polynom[1]*distance+polynom[2]*distance^2).
		static RRLight* createPointLightPoly(const RRVec3& position, const RRVec3& colorAtDistance0, RRVec3 polynom);

		//! Creates spot light with physically correct distance attenuation.
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance in physical scale at distance 1, assuming that receiver is oriented towards light.
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Angle in radians. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Angle in radians. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		static RRLight* createSpotLight(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);

		//! Creates spot light with radius/exponent based distance attenuation (physically incorrect).
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorAtDistance0
		//!  Irradiance in custom scale (usually screen color) of lit surface at distance 0.
		//! \param radius
		//!  Distance in world space, where light disappears due to its distance attenuation.
		//!  So light has effect in sphere of given radius.
		//! \param fallOffExponent
		//!  Distance attenuation in custom scale is computed as pow(MAX(0,1-distance/radius),fallOffExponent).
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Angle in radians. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Angle in radians. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		static RRLight* createSpotLightRadiusExp(const RRVec3& position, const RRVec3& colorAtDistance0, RRReal radius, RRReal fallOffExponent, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);

		//! Creates spot light with polynom based distance attenuation (physically incorrect).
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorAtDistance0
		//!  Irradiance in custom scale (usually screen color) of lit surface at distance 0.
		//! \param polynom
		//!  Distance attenuation in custom scale is computed as 1/(polynom[0]+polynom[1]*distance+polynom[2]*distance^2).
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Angle in radians. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Angle in radians. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		static RRLight* createSpotLightPoly(const RRVec3& position, const RRVec3& colorAtDistance0, RRVec3 polynom, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRLights
	//! Set of lights with interface similar to std::vector.
	//
	//! This is usual product of adapter that creates Lightsprint interface for external 3d scene.
	//! You may use it for example to
	//! - send it to RRDynamicSolver and calculate global illumination
	//! - manipulate this set before sending it to RRDynamicSolver, e.g. remove moving lights
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRLights : public RRVector<RRLight*>
	{
	public:
		virtual ~RRLights() {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRStaticObject
	//! Static 3d object with storage space for calculated illumination.
	//
	//////////////////////////////////////////////////////////////////////////////

	struct RRStaticObject
	{
		RRObject* object;
		RRObjectIllumination* illumination;
		RRStaticObject(RRObject* o, RRObjectIllumination* i) : object(o), illumination(i) {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRStaticObjects
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

	class RRStaticObjects : public RRVector<RRStaticObject>
	{
	public:
		virtual ~RRStaticObjects() {};
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
	//! Sample RealtimeRadiosity shows both typical usage scenarios,
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
		virtual void setScaler(RRScaler* scaler);

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
		//!
		//! setStaticObjects() removes effects of previous loadFireball() or calculate().
		//!
		//! \param objects
		//!  Static contents of your scene, set of static objects.
		//!  Objects should not move (in 3d space) during our lifetime.
		//!  Object's getTriangleMaterial() and getPointMaterial() should return values in custom scale.
		//!  If objects provide custom data channels (see RRChanneledData), all objects must have the same channels.
		//! \param smoothing
		//!  Static scene illumination smoothing.
		//!  Set NULL for default values.
		void setStaticObjects(RRStaticObjects& objects, const RRStaticSolver::SmoothingParameters* smoothing);

		//! Returns number of static objects in scene.
		unsigned getNumObjects() const;

		//! Returns i-th static object in scene.
		RRObject* getObject(unsigned i);

		//! Returns illumination of i-th static object in scene.
		RRObjectIllumination* getIllumination(unsigned i);
		//! Returns illumination of i-th static object in scene.
		const RRObjectIllumination* getIllumination(unsigned i) const;

		//! Optional parameters of calculate(). Currently used only by Fireball.
		struct CalculateParams
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
			CalculateParams()
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
		//! \return
		//!  IMPROVED when any vertex or pixel buffer was updated with improved illumination.
		//!  NOT_IMPROVED otherwise. FINISHED = exact solution was reached, no further calculations are necessary.
		virtual RRStaticSolver::Improvement calculate(CalculateParams* params = NULL);

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
			//! Requested type of results, applies only to updates of external buffers.
			//
			//! Attributes direct/indirect specify what parts of solver data read when applyCurrentSolution=true,
			//! they have no influence on applyLights/applyEnvironment.
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
			//! Higher number = higher quality. Time taken grows mostly linearly with this number.
			//! 1000 is usually sufficient for production, with small per pixel details
			//! and precise antialiasing computed.
			//! Lower quality is good for tests, with per pixel details, but with artifacts
			//! and aliasing.
			//!
			//! Positive number makes calculation non-realtime.
			//!
			//! If applyCurrentSolution is enabled, other applyXxx disabled and quality is zero,
			//! very fast (milliseconds) realtime technique is used and outputs contains
			//! nearly no noise, but in case of lightmaps, small per pixel details are missing.
			unsigned quality;

			//! Deprecated. Only partially supported since 2007.08.21.
			//
			//! 0..1 ratio, texels with greater fraction of hemisphere 
			//! seeing inside objects (or below rug, see rugDistance)
			//! are masked away.
			//! Default value 1 disables any correction.
			//!
			//! Not used in realtime calculation (see #quality).
			RRReal insideObjectsTreshold;

			//! Deprecated. Only partially supported since 2007.08.21.
			//
			//! Distance in world space, illumination coming from closer surfaces is masked away.
			//! Set it slightly above distance of rug and ground, to prevent darkness
			//! under the rug leaking half texel outside (instead, light around rug will
			//! leak under the rug). Set it to zero to disable any corrections.
			//!
			//! Not used in realtime calculation (see #quality).
			RRReal rugDistance;

			//! Distance in world space; illumination never comes from greater distance.
			//
			//! As long as it is bigger than scene size, results are realistic.
			//! Setting it below scene size makes results less realistic, illumination
			//! gets increasingly influenced by outer environment/sky instead of scene.
			//! (Rays are shot from texel into scene. When scene is not intersected
			//! in this or lower distance from texel, illumination is read from 
			//! outer environment/sky.)
			//!
			//! Not used in realtime calculation (see #quality).
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
		//!  params.applyCurrentSolution=1; applyLights=0; applyEnvironment=0; quality=0.
		//!  For higher quality final gathered results, use updateVertexBuffers().
		virtual unsigned updateVertexBuffer(unsigned objectNumber, RRIlluminationVertexBuffer* vertexBuffer, const UpdateParameters* params);

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
		//! \param createMissingBuffers
		//!  If destination buffer doesn't exist, it is created by newVertexBuffer().
		//! \param paramsDirect
		//!  Parameters of the update process specific for direct illumination component of final color.
		//!  With e.g. paramsDirect->applyLights, direct illumination created by lights 
		//!  set by setLights() is added to the final value stored into buffer.
		//!  \n params->measure specifies type of information stored in vertex buffer.
		//!  For typical scenario with per pixel direct illumination and per vertex indirect illumination,
		//!  use RM_IRRADIANCE_PHYSICAL_INDIRECT (faster) or RM_IRRADIANCE_CUSTOM_INDIRECT.
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
		//!  Internal state is properly updated even when you don't specify createMissingBuffers
		//!  and buffers don't exist (so no other output is produced).
		//!  Following updateLightmap() or updateVertexBuffer() will include
		//!  this indirect lighting into computed lightmap or vertex buffer
		//!  if you set their params->applyCurrentSolution.
		//! \remarks
		//!  Update of selected objects (rather than all objects) is supported in multiple ways, use one of them:
		//!  - for lighting in current solver, simply call updateVertexBuffer() for all selected objects
		//!  - create vertex buffers for selected objects, make sure other vertex buffers are NULL and call updateVertexBuffers() with createMissingBuffers=false
		//!  - call updateVertexBuffers(-1,-1,false,NULL,paramsIndirect) once to update current solution, call updateVertexBuffer(paramsDirect with applyCurrentSolution=true) for all selected objects
		virtual unsigned updateVertexBuffers(int layerNumberLighting, int layerNumberBentNormals, bool createMissingBuffers, const UpdateParameters* paramsDirect, const UpdateParameters* paramsIndirect);

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
		//! \return
		//!  Number of lightmaps and bent normal maps updated.
		//!  Zero when no update was executed because of invalid inputs.
		//!  Read system messages (RRReporter) for more details on failure.
		//! \remarks
		//!  In comparison with more general updateLightmaps() function, this one
		//!  lacks paramsIndirect. However, you can still include indirect illumination
		//!  while updating single lightmap, see updateLightmaps() remarks.
		virtual unsigned updateLightmap(unsigned objectNumber, RRIlluminationPixelBuffer* lightmap, RRIlluminationPixelBuffer* bentNormals, const UpdateParameters* params);

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
		//! \remarks
		//!  As a byproduct of calculation, internal state of solver (current solution)
		//!  is updated, so that it holds computed indirect illumination for sources
		//!  and quality specified in paramsIndirect.
		//!  Internal state is properly updated even when you don't specify createMissingBuffers
		//!  and buffers don't exist (so no other output is produced).
		//!  Following updateLightmap() or updateVertexBuffer() will include
		//!  this indirect lighting into computed lightmap or vertex buffer
		//!  if you set their params->applyCurrentSolution.
		//! \remarks
		//!  Update of selected objects (rather than all objects) is supported in multiple ways, use one of them:
		//!  - if you don't need indirect illumination, simply call updateLightmap() for all selected objects
		//!  - create pixel buffers for selected objects, make sure other pixel buffers are NULL and call updateLightmaps() with createMissingBuffers=false
		//!  - call updateLightmaps(-1,-1,false,NULL,paramsIndirect) once to update current solution, call updateLightmap(paramsDirect with applyCurrentSolution=true) for all selected objects
		virtual unsigned updateLightmaps(int layerNumberLighting, int layerNumberBentNormals, bool createMissingBuffers, const UpdateParameters* paramsDirect, const UpdateParameters* paramsIndirect);

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
		//
		//! Call this when you change material properties in your material editor.
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
		//!  Set to NULL for no saving.
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
		//!  File with data computed by buildFireball().
		//! \return
		//!  True if successful.
		bool loadFireball(const char* filename);

		//! Build or load and then start Fireball.
		//
		//! While loadFireball() and buildFireball() give you opportunity to manage filenames
		//! e.g. for transfer from PC toolchain to other platform, startFireball()
		//! is shortcut for PC only use, it starts Fireball without any questions.
		//! It calls loadFireball() and buildFireball() internally.
		//! \param avgRaysPerTriangle
		//!  Used only if Fireball file doesn't exist yet.
		//!  Average number of rays per triangle used to compute form factors.
		//!  Higher number = longer calculation, higher quality results, bigger file.
		//!  If zero, current factors are used, without recalculating.
		bool startFireball(unsigned raysPerTriangle);

		//! Returns multiObject created by merging all objects present in scene.
		//! MultiObject is created when you insert objects and call calculate().
		const RRObject* getMultiObjectCustom() const;

		//! As getMultiObjectCustom, but with materials converted to physical space.
		const RRObjectWithPhysicalMaterials* getMultiObjectPhysical() const;

		//! As getMultiObjectPhysical, but with space for storage of detected direct illumination.
		RRObjectWithIllumination* getMultiObjectPhysicalWithIllumination();

		//! Returns static solver for direct queries of single triangle/vertex illumination.
		//
		//! Static solver is created from getMultiObjectPhysicalWithIllumination()
		//! after setStaticObjects() and calculate().
		//! Static solver is not created if you use \ref calc_fireball.
		const RRStaticSolver* getStaticSolver() const;

	protected:
		//! Autodetects material properties of all materials present in scene.
		//
		//! To be implemented by you.
		//! New values must appear in RRObject-s already present in scene.
		//! \n\n It is perfectly ok to write empty implementation if your application never modifies materials.
		virtual void detectMaterials() = 0;

		//! Detects direct illumination on all faces in scene and returns it in array of RGBA values.
		//
		//! Used by realtime solver.
		//! This function is automatically called from calculate(),
		//! when solver needs current direct illumination values and you
		//! previously called reportDirectIlluminationChange(), so it's known,
		//! that direct illumination values in solver are outdated.
		//!
		//! For realtime solution, don't call setLights().
		//! Instead, provide complete information about lights here,
		//! in form of (detected) per-triangle irradiances.
		//!
		//! What exactly is "direct illumination" here is implementation defined,
		//! but for consistent results, it should be complete illumination
		//! generated by your renderer, except indirect illumination (e.g. constant ambient).
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
		virtual unsigned* detectDirectIllumination() = 0;

		//! Sets factor that multiplies intensity of detected direct illumination.
		//
		//! Default value is 1 = realistic.
		//! Calling it with boost=2 makes detected direct illumination
		//! for solver 2x stronger, so also computed indirect illumination is 2x stronger.
		//! You would get the same results if you multiply computed indirect lighting in shader,
		//! but this way it's for free, without overhead.
		void setDirectIlluminationBoost(RRReal boost);


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

		RRStaticSolver::Improvement calculateCore(float improveStep,CalculateParams* params=NULL);
		bool       gatherPerTriangle(const UpdateParameters* aparams, struct ProcessTexelResult* results, unsigned numResultSlots);
		unsigned   updateVertexBufferFromPerTriangleData(unsigned objectHandle, RRIlluminationVertexBuffer* vertexBuffer, RRVec3* perTriangleData, unsigned stride) const;
		void       updateVertexLookupTableDynamicSolver();
		void       updateVertexLookupTablePackedSolver();
		struct Private;
		Private* priv;
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
