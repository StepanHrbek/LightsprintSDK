// --------------------------------------------------------------------------
// Radiosity solver - high level.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRSTATICSOLVER_H
#define RRSTATICSOLVER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRStaticSolver.h
//! \brief LightsprintCore | global illumination solver for static scenes
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{



	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRStaticSolver
	//! 3d scene where illumination can be calculated.
	//
	//! %RRStaticSolver covers the most important functionality of library -
	//! illumination calculation. It allows you to setup multiple objects in space,
	//! setup other states that affect calculations, perform calculations
	//! and read back results.
	//!
	//! Common scenario is to perform calculations while allowing 
	//! user to continue work and see illumination improving on the background.
	//! This could be done by inserting following code into your main loop
	//! \code
	//    if (!paused) {
	//      scene->illuminationImprove(endIfUserWantsToInteract);
	//      if (once a while) read back results using scene->getTriangleMeasure();
	//    } \endcode
	//! \n For smooth integration without feel of slowing down, it is good to pause 
	//! calculation after each user interaction and resume it again once there is
	//! no user interaction for at least 0.5 sec.
	//! \n When user modifies scene, calculation may be restarted using illuminationReset().
	//!  For scenes small enough, this creates global illumination in dynamic scene 
	//!  with full interactivity.
	//!
	//! Thread safe: It is legal to create multiple scenes and
	//!  use them in parallel, in multiple threads.
	//!  It is legal to read results from one scene in multiple threads at once.
	//!  It is not allowed to calculate in one scene in multiple threads at once.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRStaticSolver
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		//
		// import geometry
		//

		//! Creates new static scene.
		//
		//! For highest performance, stay with low number of possibly big objects 
		//! rather than high number of small ones.
		//! One big object can be created out of many small ones using RRObject::createMultiObject().
		//!
		//! Different objects always belong to different smoothgroups. So with flat wall cut into two objects,
		//! sharp edge will possibly appear between them.
		//! This can be fixed by merging standalone objects into one object using RRObject::createMultiObject().
		//! \param object
		//!  World-space object wrapper that defines object shape and material.
		//!  \n If object has transformation different from identity, pass object->createWorldSpaceObject()
		//!  rather than object itself, otherwise object transformation will be ignored.
		//!  \n object->getTriangleMaterial() and getPointMaterial() should return values in physical scale.
		//! \param smoothing
		//!  Illumination smoothing parameters.
		//! \param aborting
		//!  When set, even asynchronously during create(), work is aborted.
		static RRStaticSolver* create(RRObject* object, const RRDynamicSolver::SmoothingParameters* smoothing, bool& aborting);

		//! Destructs static scene.
		~RRStaticSolver();


		//////////////////////////////////////////////////////////////////////////////
		//
		// calculate radiosity
		//

		//! Describes result of illumination calculation.
		enum Improvement
		{
			IMPROVED,       ///< Illumination was improved during this call.
			NOT_IMPROVED,   ///< Although some calculations were done, illumination was not yet improved during this call.
			FINISHED,       ///< Correctly finished calculation (probably no light in scene). Further calls for improvement have no effect.
			INTERNAL_ERROR, ///< Internal error, probably caused by invalid inputs (but should not happen). Further calls for improvement have no effect.
		};

		//! Reset illumination to original state defined by objects.
		//
		//! There is no need to reset illumination right after scene creation, it is already reset.
		//! \param resetFactors
		//!  True: Resetting factors at any moment means complete restart of calculation with all expenses.
		//!  \n False: With factors preserved, part of calculation is reused, but you must ensure, that
		//!  geometry and materials were not modified. This is especially handy when only primary
		//!  lights move, but objects and materials stay unchanged.
		//! \param resetPropagation
		//!  Both options work well in typical situations, but well choosen value may improve performance.
		//!  \n True: Illumination already propagated using old factors is reset.
		//!     It is faster option when illumination changes 
		//!     significantly -> use after big light movement, light on/off.
		//!  \n False: Illumination already propagated using old factors is preserved for 
		//!     future calculation. It is only updated. It is faster option when illumination 
		//!     changes by small amount -> use after tiny light movement, small color/intensity change.
		//! \param directIrradianceCustomRGBA8
		//!  Array of per-triangle detected direct irradiances in RGBA8 format, one value per multiobject triangle.
		//!  May be NULL.
		//! \param customToPhysical
		//!  Table used to convert custom scale bytes to physical scale floats.
		//!  May be NULL if directIrradianceCustomRGBA8 is NULL.
		//! \param directIrradiancePhysicalRGB
		//!  Array of per-triangle detected direct irradiances in RGB format, one value per multiobject triangle.
		//!  May be NULL.
		//! \return Calculation state, see Improvement.
		Improvement   illuminationReset(bool resetFactors, bool resetPropagation, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB);

		class EndFunc
		{
		public:
			//! Return true to stop calculation.
			//! It should be very fast, just checking system variables like time or event queue length.
			//! It may be called very often, slow code may have big impact on performance.
			virtual bool requestsEnd() = 0;
			//! Return true to use slower calculation path that queries requestsEnd() more often.
			virtual bool requestsRealtimeResponse() = 0;
		};

		//! Improve illumination until endfunc requests end.
		//
		//! If you want calculation as fast as possible, make sure that most of time
		//! is spent here. You may want to interleave calculation by reading results, rendering and processing
		//! event queue, but stop rendering and idle looping when nothing changes and user doesn't interact,
		//! don't read results too often etc.
		//! \param endfunc Custom callback used to determine whether to stop or continue calculating.
		//! \return Calculation state, see Improvement.
		Improvement   illuminationImprove(EndFunc& endfunc);

		//! Returns illumination accuracy in proprietary scene dependent units. Higher is more accurate. Fast, reads single number.
		RRReal        illuminationAccuracy();

		//! Returns illumination accuracy in 0..1 range. Higher is more accurate. Slow, traverses whole scene.
		//RRReal        illuminationConvergence();


		//////////////////////////////////////////////////////////////////////////////
		//
		// read results
		//

		//! Smoothing only. Returns single smoothed vertex value, given arbitrary per triangle data.
		RRVec3        getVertexDataFromTriangleData(unsigned questionedTriangle, unsigned questionedVertex012, const RRVec3* perTriangleData, unsigned stride) const;

		//! Reads illumination of triangle's vertex in units given by measure.
		//
		//! Reads results in format suitable for fast vertex based rendering without subdivision.
		//! \param triangle Index of triangle you want to get results for. Valid triangle index is <0..getNumTriangles()-1>.
		//! \param vertex Index of triangle's vertex you want to get results for. Valid vertices are 0, 1, 2.
		//!  For invalid vertex number, average value for whole triangle is taken instead of smoothed value in vertex.
		//! \param measure
		//!  Specifies what to measure, using what units.
		//! \param scaler
		//!  Custom scaler for results in non physical scale. Scale conversion is enabled by measure.scaled.
		//! \param out
		//!  For valid inputs, illumination level is stored here. For invalid inputs, nothing is changed.
		//! \return
		//!  True if out was successfully filled. False may be caused by invalid inputs.
		bool          getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, const RRScaler* scaler, RRVec3& out) const;

		//! Build Fireball. For internal use by RRDynamicSolver::buildFireball().
		const class PackedSolverFile* buildFireball(unsigned raysPerTriangle);

	private:
		class Scene*  scene;
		friend class Gatherer;
		friend class RRCollisionHandlerGatherHemisphere;
		friend class RRDynamicSolver;
		RRStaticSolver(RRObject* object, const RRDynamicSolver::SmoothingParameters* smoothing, class Object* obj, bool& aborting);
	};


} // namespace

#endif
