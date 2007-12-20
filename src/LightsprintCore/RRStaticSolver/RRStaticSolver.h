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
	//    if(!paused) {
	//      scene->illuminationImprove(endIfUserWantsToInteract);
	//      if(once a while) read back results using scene->getTriangleMeasure();
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
		RRStaticSolver(RRObject* object, const RRDynamicSolver::SmoothingParameters* smoothing);

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
		//! \return Calculation state, see Improvement.
		Improvement   illuminationReset(bool resetFactors, bool resetPropagation);

		//! Improve illumination until endfunc returns true.
		//
		//! If you want calculation as fast as possible, make sure that most of time
		//! is spent here. You may want to interleave calculation by reading results, rendering and processing
		//! event queue, but stop rendering and idle looping when nothing changes and user doesn't interact,
		//! don't read results too often etc.
		//! \param endfunc Callback used to determine whether to stop or continue calculating.
		//!  It should be very fast, just checking system variables like time or event queue length.
		//!  It is called very often, slow endfunc may have big impact on performance.
		//! \param context Value is passed to endfunc without any modification.
		//! \return Calculation state, see Improvement.
		Improvement   illuminationImprove(bool endfunc(void*), void* context);

		//! Returns illumination accuracy in proprietary scene dependent units. Higher is more accurate.
		RRReal        illuminationAccuracy();


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

		//! Illumination information for triangle's subtriangle.
		//
		//! Subtriangle is triangular area inside triangle given by three 2d coordinates in triangle space.
		//! If triangle has no subdivision, the only subtriangle fills whole triangle with following coordinates:
		//!  RRVec2(0,0), RRVec2(1,0), RRVec2(0,1)
		//! Illumination is recorded in subtriangle vertices.
		struct SubtriangleIllumination
		{
			RRVec2 texCoord[3]; ///< Subtriangle vertices positions in triangle space, triangle vertex0 is in 0,0, vertex1 is in 1,0, vertex2 is in 0,1.
			RRVec3 measure[3]; ///< Subtriangle vertices illumination.
		};
		//! Callback for passing multiple SubtriangleIlluminations to you.
		typedef void (SubtriangleIlluminationEater)(const SubtriangleIllumination& si, void* context);
		//! Reads illumination of triangle's subtriangles in units given by measure.
		//
		//! Reads results in format suitable for high quality texture based rendering (with adaptive subdivision).
		//! \param triangle
		//!  Index of triangle you want to get results for. Valid triangle index is <0..getNumTriangles()-1>.
		//! \param measure
		//!  Specifies what to measure, using what units.
		//! \param scaler
		//!  Custom scaler for results in non physical scale. Scale conversion is enabled by measure.scaled.
		//! \param callback
		//!  Your callback that will be called for each triangle's subtriangle.
		//! \param context
		//!  Value is passed to callback without any modification.
		//! \return
		//!  Number of subtriangles processed.
		unsigned      getSubtriangleMeasure(unsigned triangle, RRRadiometricMeasure measure, const RRScaler* scaler, SubtriangleIlluminationEater* callback, void* context) const;

		//! Build Fireball. For internal use by RRDynamicSolver::buildFireball().
		const class PackedSolverFile* buildFireball(unsigned raysPerTriangle);


	private:
		class Scene*  scene;
		friend class Gatherer;
	};


} // namespace

#endif
