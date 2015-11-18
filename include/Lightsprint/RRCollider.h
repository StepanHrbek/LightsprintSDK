//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RRCollider.h
//! \brief LightsprintCore | fast ray-mesh intersections
//----------------------------------------------------------------------------

#ifndef RRCOLLIDER_H
#define RRCOLLIDER_H

#include "RRMesh.h"
#include "RRCamera.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRCollisionHandler
	//! Interface for non-trivial handling of collisions in collision test.
	//
	//! You can use it to
	//! - intersect mesh that contains singlesided faces
	//! - intersect mesh with layers that can be independently turned on/off
	//! - gather all intersections with one mesh
	//!
	//! It's intentionally not part of RRMesh, so you can easily combine
	//! different collision handlers with one geometry.
	//! \n It's intentionally not part of RRRay, so you can easily combine
	//! different collision handlers at runtime.
	//!
	//! Thread safe: no, holds state, may be accessed by one thread at moment.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRCollisionHandler
	{
	public:
		virtual ~RRCollisionHandler() {}

		//! Prepares for single intersection test.
		//
		//! Is called at the beginning of RRCollider::intersect().
		//! May set additional flags in ray.
		virtual void init(class RRRay& ray) = 0;

		//! Handles each collision detected by single intersection test.
		//
		//! It is called at each triangle hit inside RRCollider::intersect().
		//! When its result is false, collision is rejected and testing continues.
		//! When its result is true, collision is accepted, but it does not necessarily end testing,
		//! as collider can find closer itersection after testing more distant one first.
		//! In general case, intersections are reported unordered.
		//! Intersections are reported in order from the nearest one (and first positive result ends testing) only for IT_BSP techniques.
		//! \return Return false if you want to discard intersection and continue testing.
		virtual bool collides(const class RRRay& ray) = 0;

		//! Cleans up after single intersection test.
		//
		//! Is called at the end of RRCollider::intersect().
		//! Result is returned as result from RRCollider::intersect().
		virtual bool done() = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRRay
	//! Ray to intersect with object.
	//
	//! Contains all inputs and outputs for RRCollider::intersect(), with
	//! all RRVec3/4 aligned for SIMD instructions.
	//! Alignment is automatic for all static and local instances on stack (thanks to RR_ALIGNED)
	//! and for instances on heap (thanks to RRAligned). If you add ray into your class as a member variable,
	//! either dynamically allocate ray (with create()) or statically allocate instances of your class
	//! or derive your class from RRAligned, otherwise program might crash due to unaligned data access.
	//!
	//! Thread safe: no, holds state, may be accessed by one thread at moment.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RR_ALIGNED RRRay : public RRAligned
	{
	public:
		//! Initializes 1 RRRay. All is zeroed, all FILL flags on.
		RRRay();

		//! Creates 1 RRRay. All is zeroed, all FILL flags on. Use delete when no longer needed.
		static RRRay* create();

		//! Creates array of RRRays. Use delete[] when no longer needed.
		static RRRay* create(unsigned n);

		//! Flags define which outputs to fill. (Some outputs may be filled even when not requested by flag.)
		enum Flags
		{ 
			FILL_DISTANCE   =(1<<0), ///< Fill hitDistance.
			FILL_POINT3D    =(1<<1), ///< Fill hitPoint3d.
			FILL_POINT2D    =(1<<2), ///< Fill hitPoint2d.
			FILL_PLANE      =(1<<3), ///< Fill hitPlane.
			FILL_TRIANGLE   =(1<<4), ///< Fill hitTriangle.
			FILL_SIDE       =(1<<5), ///< Fill hitFrontSide.
		};

		// inputs
		RRVec3p         rayOrigin;      ///< In. (-Inf,Inf), ray origin. Never modify last component, it must stay 1.
		RRVec3p         rayDir;         ///< In. <-Inf,Inf>, ray direction. Must be normalized.
		RRReal          rayLengthMin;   ///< In. <0,Inf), test intersection in distances from range <rayLengthMin,rayLengthMax>.
		RRReal          rayLengthMax;   ///< In. <0,Inf), test intersection in distances from range <rayLengthMin,rayLengthMax>.
		unsigned        rayFlags;       ///< InOut. Flags that specify what to find. Additional flags may be set by collision handler.

		// outputs (valid after positive test, undefined otherwise)
		RRReal          hitDistance;    ///< Out. Hit distance in object space.
		unsigned        hitTriangle;    ///< Out. Index of triangle (postImport) within object that was hit.
		RRVec2          hitPoint2d;     ///< Out. Hit coordinate in triangle space defined so that vertex[0]=0,0 vertex[1]=1,0 vertex[2]=0,1. Barycentric/areal coordinates of hit point are 1-hitPoint2d[0]-hitPoint2d[1],hitPoint2d[0],hitPoint2d[1].
		RRVec4          hitPlane;       ///< Out. Plane of hitTriangle in object space. RRVec3 part is normal, normalized. Result is based on vertex positions rather than normals provided by mesh, so plane is constant for whole triangle.
		RRVec3          hitPoint3d;     ///< Out. Hit coordinate in object space.
		bool            hitFrontSide;   ///< Out. True = face was hit from the front side.
		RRVec3          hitPadding1;    ///< Out. Undefined, never modify.
		RRVec2          hitPadding2;    ///< Out. Undefined, never modify.
#if !defined(_M_X64) && !defined(_LP64)
		RRVec2          hitPadding3;    ///< Out. Undefined, never modify.
#endif
		const class RRObject* hitObject;///< (In/)Out. If collider was created from RRObjects, intersected object is stored here. If collider was created from RRMesh, hitObject is not modified (but it's good practise to set it before calling intersect(), so that your collision handlers and other code can rely on hitObject being always set). If pointer to multiObject is returned, hitTriangle also points to triangle within multiObject; you can translate both to singleObject using convertHitFromMultiToSingleObject().
		RRCollisionHandler*   collisionHandler;///< In. Optional collision handler for user-defined surface behaviour.

		//! If hitObject points to solver's multiobject, this function changes it to individual object. hitTriangle is modified accordingly.
		//
		//! After successful solver->getCollider()->intersect(this), hitObject might point to solver's multiobject.
		//! This function checks if it is the case, and modifies hitObject and hitTriangle to point to singleobject, using code similar to
		//! hitObject = solver->getStaticObjects()[hitObject->getCollider()->getMesh()->getPreImportTriangle(hitTriangle).object]
		//! It is not done automatically, because it is often unnecessary, not doing it makes collider faster.
		bool convertHitFromMultiToSingleObject(class RRSolver* solver);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRCollider
	//! Finds ray - trianglemesh intersections.
	//
	//! Thread safe: yes, stateless, may be accessed by any number of threads simultaneously.
	//!
	//! Mesh passed to create() must stay constant for whole life of collider,
	//! otherwise collision results are undefined.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRCollider : public RRAlignedNonCopyable
	{
	public:
		//! Built-in techniques for finding ray-mesh intersections. Others can be added via registerTechnique().
		enum IntersectTechnique
		{
			IT_LINEAR,          ///< Speed   1%, size    0. Fallback technique when better one fails.
			IT_BSP_COMPACT,     ///< Speed 100%, size   ~5 bytes per triangle. Slow build. For platforms with extremely limited memory.
			IT_BSP_FAST,        ///< Speed 175%, size  ~31 bytes per triangle. Slow build. For platforms with limited memory.
			IT_BSP_FASTER,      ///< Speed 200%, size  ~60 bytes per triangle. Slow build.
			IT_BSP_FASTEST,     ///< Speed 230%, size ~200 bytes per triangle. Slow build.
			IT_BVH_COMPACT,     ///< Speed 200%, size ~150 bytes per triangle. Fast build.
			IT_BVH_FAST,        ///< Speed 230%, size ~162 bytes per triangle. Fast build. Usually the best choice.
			IT_VERIFICATION,    ///< Only for verification purposes, performs tests using all known techniques and compares results.
		};

		//! Type of custom collider builder.
		typedef RRCollider* Builder(const RRMesh* mesh, const class RRObjects* objects, IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, void* buildParams);
		//! Lets you register custom collider builder. If other builder was already registered with the same intersectTechnique number, it is replaced with the new builder.
		static void registerTechnique(unsigned intersectTechnique, Builder* builder);

		//! Creates and returns collider, acceleration structure for finding ray x mesh intersections.
		//
		//! \param mesh Mesh you want to collide with. If set, objects must be nullptr.
		//! \param objects Objects you want to collide with. If set, mesh must be nullptr.
		//! \param intersectTechnique Technique used for accelerating collision searches. See #IntersectTechnique.
		//! \param aborting May be set asynchronously. When set, collider creation is aborted.
		//! \param cacheLocation Optional location of cache, path to directory where acceleration structures may be cached.
		//!        Default nullptr caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		//! \param buildParams Optional additional parameters, specific for each technique and not revealed for public use.
		//! \return Created collider.
		static RRCollider* create(const RRMesh* mesh, const class RRObjects* objects, IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation=nullptr, void* buildParams=0);

		//! Finds ray x mesh intersections.
		//
		//! \param ray All inputs and outputs for search.
		//! \return True if intersection was found and reported into ray.
		//!
		//! Finds nearest intersection of ray and mesh in distance
		//! <ray->rayLengthMin,ray->rayLengthMax> and fills output attributes in ray
		//! specified by ray->rayFlags.
		//! \n See RRRay for more details on inputs, especially flags that further specify collision test.
		//! \n When ray->collisionHandler!=nullptr, it is called and it may accept or refuse intersection.
		//! \n When collisionHandler accepts intersection, true is returned.
		//! When collisionHandler rejects intersection, search continues.
		//! \n False is returned when there were no accepted intersection.
		//!
		//! There is an exception for IntersectTechnique IT_LINEAR: intersections are found in random order.
		//!
		//! \section CollisionHandler
		//! Ray->collisionHandler can be used to:
		//! - gather all intersections instead of just first one. CollisionHandler can gather
		//!   or immediately process them. This is faster and more precise approach than multiple
		//!   calls of %intersect() with increasing rayLengthMin to distance of last intersection.
		//! - not collide with optional parts of mesh (layers) that are turned off at this moment.
		//! - find pixel precise collisions with alpha keyed textures.
		//! - collide with specific probability.
		//!
		//! \section Multithreading
		//! You are encouraged to find multiple intersections in multiple threads at the same time.
		//! This will improve your performance on multicore CPUs. \n Even with Intel's hyperthreading,
		//! which is inferior to two fully-fledged cores, searching multiple intersections at the same time brings
		//! surprisingly high performance bonus.
		//! \n All you need is one RRRay and optionally one RRCollisionHandler for each thread, 
		//!  other structures like RRMesh and RRCollider are stateless and
		//!  may be accessed by arbitrary number of threads simultaneously.
		//! \n If you are not familiar with OpenMP, be sure to examine it. With OpenMP, which is built-in 
		//!  feature of modern compilers, searching multiple intersections
		//!  at the same time is matter of one or few lines of code.
		virtual bool intersect(RRRay& ray) const = 0;

		//! Intersects mesh with batch of rays at once.
		//
		//! Ease of use:
		//!  Unlike intersect(), intersectBatch() makes use of all available cores/processors.
		//!
		//! Performance:
		//!  Batches always add overhead. Small batches add thread creation overhead (not significant 
		//!  for big ones). Big batches add slow system memory write/read overhead (not significant for small ones).
		//!  The most efficient technique is to avoid batches, create multiple threads in your code and
		//!  process individual rays using intersect() immediately after their creation, when they are still in L1 cache
		//!  (it's safe to run intersect() from multiple threads at once).
		//!
		//! Coherent rays:
		//!  Coherent rays in batch are processed faster, but only because the same memory words
		//!  are accessed, no special algorithm that depends on equal rayOrigins is used.
		//!
		//! Conclusion:
		//!  If performance is critical for you, experiment, try multiple approaches.
		//!  Use intersectBatch() if you have multiple cores and you don't want to manage threads yourself.
		//!  Otherwise look at BunnyBenchmark sample where OpenMP threading and intersect() are used for higher performance.
		//!
		//! \param ray
		//!  Array of rays. Each ray contains both inputs and outputs.
		//!  For each ray, hitDistance=-1 says that ray had no intersection with mesh,
		//!  any other value means that intersection was detected.
		//! \param numRays
		//!  Number of rays in array.
		void intersectBatch(RRRay* ray, unsigned numRays) const;


		//! Shoots rays from point, measures distance to first collision, updates distanceMinMax. It can be used for automatic near/far adjustment.
		//
		//! \param point
		//!  Point to shoot from.
		//! \param object
		//!  Optional, enables testing materials, rays don't stop when they go through transparent pixels.
		//! \param shadowRays
		//!  True if you wish to measure distance to the nearest shadow caster (even if it is invisible), rather than to visible object.
		//! \param distanceMinMax
		//!  Has to be initialized. distanceMinMax[0] is decreased whenever closer collision is found,
		//!  distanceMinMax[1] is increased whenever farther collision is found.
		//!  Stays unchanged when ray doesn't collide at all.
		//! \param numRays
		//!  Approximately how many rays it shoots (higher number = slower but more accurate).
		void getDistancesFromPoint(const RRVec3& point, const RRObject* object, bool shadowRays, RRVec2& distanceMinMax, unsigned numRays = 300) const;
		//! Similar to getDistancesFromPoint, but shoots from camera.
		void getDistancesFromCamera(const RRCamera& camera, const RRObject* object, bool shadowRays, RRVec2& distanceMinMax, unsigned numRays = 100) const;


		//! \return Mesh that was passed to create().
		//!  Must always return valid mesh, implementation is not allowed to return nullptr.
		virtual const RRMesh* getMesh() const = 0;

		//! \return Technique used by collider. May differ from technique requested in create().
		virtual IntersectTechnique getTechnique() const = 0;

		//! Changes technique, but only if collider was created with IT_LINEAR.
		//
		//! Other colliders have technique hardcoded, for tiny bit higher speed.
		//! \param intersectTechnique Technique used for accelerating collision searches. See #IntersectTechnique.
		//! \param aborting May be set asynchronously. When set, technique change is aborted, old one is preserved.
		virtual void setTechnique(IntersectTechnique intersectTechnique, bool& aborting);

		//! \return Total amount of system memory occupied by collider.
		virtual size_t getMemoryOccupied() const = 0;

		virtual ~RRCollider() {};
	};



} // namespace

#endif
