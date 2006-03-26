#ifndef COLLIDER_RRCOLLIDER_H
#define COLLIDER_RRCOLLIDER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRCollider.h
//! \brief RRCollider - library for fast "ray x mesh" intersections
//! \version 2006.3.26
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#	ifdef RRCOLLIDER_STATIC
		// use static library
		#define RRCOLLIDER_API
		#ifdef NDEBUG
			#pragma comment(lib,"RRCollider_s.lib")
		#else
			#pragma comment(lib,"RRCollider_sd.lib")
		#endif
#	else
#	ifdef RRCOLLIDER_DLL_BUILD
		// build dll
		#define RRCOLLIDER_API __declspec(dllexport)
#	else // use dll
#define RRCOLLIDER_API __declspec(dllimport)
#pragma comment(lib,"RRCollider.lib")
#	endif
#	endif
#else
	// use static library
	#define RRCOLLIDER_API
#endif
//error : inserted by sunifdef: "#define RR_DEVELOPMENT" contradicts -U at R:\work2\.git-rewrite\t\include\RRCollider.h~(34)

#include <new>      // operators new/delete
#include <limits.h> // UNDEFINED

namespace rrCollider /// Encapsulates whole Collider library.
{

	//////////////////////////////////////////////////////////////////////////////
	//
	// primitives
	//
	// RRReal - real number
	// RRVec2 - vector in 2d
	// RRVec3 - vector in 3d
	// RRVec4 - vector in 4d
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Real number used in most of calculations.
	typedef float RRReal;

	//! Vector of 2 real numbers plus basic support.
	struct RRCOLLIDER_API RRVec2
	{
		RRReal    x;
		RRReal    y;
		RRReal& operator [](int i)          const {return ((RRReal*)this)[i];}

		RRVec2()                                  {}
		explicit RRVec2(RRReal a)                 {x=y=a;}
		RRVec2(RRReal ax,RRReal ay)               {x=ax;y=ay;}
		RRVec2 operator + (const RRVec2& a) const {return RRVec2(x+a.x,y+a.y);}
		RRVec2 operator - (const RRVec2& a) const {return RRVec2(x-a.x,y-a.y);}
		RRVec2 operator * (RRReal f)        const {return RRVec2(x*f,y*f);}
		RRVec2 operator * (const RRVec2& a) const {return RRVec2(x*a.x,y*a.y);}
		RRVec2 operator / (RRReal f)        const {return RRVec2(x/f,y/f);}
		RRVec2 operator / (const RRVec2& a) const {return RRVec2(x/a.x,y/a.y);}
		RRVec2 operator +=(const RRVec2& a)       {x+=a.x;y+=a.y;return *this;}
		RRVec2 operator -=(const RRVec2& a)       {x-=a.x;y-=a.y;return *this;}
		RRVec2 operator *=(RRReal f)              {x*=f;y*=f;return *this;}
		RRVec2 operator *=(const RRVec2& a)       {x*=a.x;y*=a.y;return *this;}
		RRVec2 operator /=(RRReal f)              {x/=f;y/=f;return *this;}
		RRVec2 operator /=(const RRVec2& a)       {x/=a.x;y/=a.y;return *this;}
		bool   operator ==(const RRVec2& a) const {return a.x==x && a.y==y;}
		bool   operator !=(const RRVec2& a) const {return a.x!=x || a.y!=y;}
	};

	//! Vector of 3 real numbers plus basic support.
	struct RRCOLLIDER_API RRVec3 : public RRVec2
	{
		RRReal    z;

		RRVec3()                                    {}
		explicit RRVec3(RRReal a)                   {x=y=z=a;}
		RRVec3(RRReal ax,RRReal ay,RRReal az)       {x=ax;y=ay;z=az;}
		RRVec3 operator + (const RRVec3& a)   const {return RRVec3(x+a.x,y+a.y,z+a.z);}
		RRVec3 operator - (const RRVec3& a)   const {return RRVec3(x-a.x,y-a.y,z-a.z);}
		RRVec3 operator * (RRReal f)          const {return RRVec3(x*f,y*f,z*f);}
		RRVec3 operator * (const RRVec3& a)   const {return RRVec3(x*a.x,y*a.y,z*a.z);}
		RRVec3 operator / (RRReal f)          const {return RRVec3(x/f,y/f,z/f);}
		RRVec3 operator / (const RRVec3& a)   const {return RRVec3(x/a.x,y/a.y,z/a.z);}
		RRVec3 operator / (int f)             const {return RRVec3(x/f,y/f,z/f);}
		RRVec3 operator / (unsigned f)        const {return RRVec3(x/f,y/f,z/f);}
		RRVec3 operator +=(const RRVec3& a)         {x+=a.x;y+=a.y;z+=a.z;return *this;}
		RRVec3 operator -=(const RRVec3& a)         {x-=a.x;y-=a.y;z-=a.z;return *this;}
		RRVec3 operator *=(RRReal f)                {x*=f;y*=f;z*=f;return *this;}
		RRVec3 operator *=(const RRVec3& a)         {x*=a.x;y*=a.y;z*=a.z;return *this;}
		RRVec3 operator /=(RRReal f)                {x/=f;y/=f;z/=f;return *this;}
		RRVec3 operator /=(const RRVec3& a)         {x/=a.x;y/=a.y;z/=a.z;return *this;}
		bool   operator ==(const RRVec3& a)   const {return a.x==x && a.y==y && a.z==z;}
		bool   operator !=(const RRVec3& a)   const {return a.x!=x || a.y!=y || a.z!=z;}
	};

	//! Vector of 4 real numbers plus basic support.
	struct RRCOLLIDER_API RRVec4 : public RRVec3
	{
		RRReal    w;

		void operator =(const RRVec3 a)   {x=a.x;y=a.y;z=a.z;}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRMeshImporter
	//! Common interface for any standard or proprietary triangle mesh structure.
	//
	//! \section s4 Creating instances
	//!
	//! %RRMeshImporter has built-in support for standard mesh formats used by
	//! rendering APIs - vertex and index buffers using triangle lists or
	//! triangle strips. See create() and createIndexed().
	//!
	//! %RRMeshImporter has built-in support for baking multiple meshes 
	//! into one mesh (without need for additional memory). 
	//! This may simplify mesh oprations or improve performance in some situations.
	//! See createMultiMesh().
	//!
	//! %RRMeshImporter has built-in support for creating self-contained mesh copies.
	//! See createCopy().
	//! While importers created from vertex buffer doesn't allocate more memory 
	//! and depend on vertex buffer, self-contained copy contains all mesh data
	//! and doesn't depend on any other objects.
	//!
	//! For other mesh formats (heightfield, realtime generated etc), 
	//! you may easily derive from %RRMeshImporter and create your own importer.
	//!
	//! \section s6 Optimizations
	//!
	//! %RRMeshImporter may help you with mesh optimizations if requested,
	//! for example by removing duplicate vertices or degenerated triangles.
	//! 
	//! \section s6 Constancy
	//!
	//! All data provided by %RRMeshImporter must be constant in time.
	//! Built-in importers guarantee constancy if you don't change
	//! their vertex/index buffers. Constancy of mesh copy is guaranteed always.
	//!
	//! \section s5 Indexing
	//!
	//! %RRMeshImporter operates with two types of vertex and triangle indices.
	//! -# PostImport indices, always 0..num-1 (where num=getNumTriangles
	//! or getNumVertices), these are used in most calls. When not stated else,
	//! index is PostImport.
	//! \n Example: with 100-triangle mesh, triangle indices are 0..99.
	//! -# PreImport indices, optional, arbitrary numbers provided by 
	//! importer for your convenience.
	//! \n Example: could be offsets into your vertex buffer.
	//! \n Pre<->Post mapping is defined by importer and is arbitrary, but constant.
	//!
	//! All Pre-Post conversion functions must accept all unsigned values.
	//! When query makes no sense, they return UNDEFINED.
	//! This is because 
	//! -# valid PreImport numbers may be spread across whole unsigned 
	//! range and caller could be unaware of their validity.
	//! -# such queries are rare and not performance critical.
	//!
	//! All other function use PostImport numbers and may support only 
	//! their 0..num-1 range.
	//! When called with out of range value, result is undefined.
	//! Debug version may return arbitrary number or throw assert.
	//! Release version may return arbitrary number or crash.
	//! This is because 
	//! -# valid PostImport numbers are easy to ensure on caller side.
	//! -# such queries are very critical for performance.
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRMeshImporter
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRMeshImporter() {}

		//
		// vertices
		//
		//! One vertex 3d space.
		typedef RRVec3 Vertex;
		//! Returns number of vertices in mesh.
		virtual unsigned     getNumVertices() const = 0;
		//! Writes v-th vertex in mesh to out.
		//
		//! Be sure to provide valid v is in range <0..getNumVertices()-1>.
		//! Implementators are allowed to expect valid v, so result is completely undefined for invalid v (possible crash).
		virtual void         getVertex(unsigned v, Vertex& out) const = 0;

		//
		// triangles
		//
		//! One triangle in mesh, defined by indices of its vertices.
		struct Triangle      {unsigned m[3]; unsigned&operator[](int i){return m[i];} const unsigned&operator[](int i)const{return m[i];}};
		//! Returns number of triangles in mesh.
		virtual unsigned     getNumTriangles() const = 0;

		//! Writes t-th triangle in mesh to out.
		//
		//! Be sure to provide valid t is in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		virtual void         getTriangle(unsigned t, Triangle& out) const = 0;

		//
		// optional for faster access
		//
		//! %Triangle in 3d space defined by one vertex and two side vectors. This representation is most suitable for intersection tests.
		struct TriangleBody  {Vertex vertex0,side1,side2;};
		//! Plane in 3d space defined by its normal (in x,y,z) and w so that normal*point+w=0 for all points of plane.
		typedef RRVec4 Plane;
		//! Writes t-th triangle in mesh to out.
		//
		//! Be sure to provide valid t is in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		//! \n There is default implementation, but if you know format of your data well, you may provide faster one.
		//! \n This call is important for performance of intersection tests.
		virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;
		//! Writes t-th triangle plane to out.
		//
		//! Be sure to provide valid t is in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		//! \n There is default implementation, but if you know format of your data well, you may provide faster one.
		virtual bool         getTrianglePlane(unsigned t, Plane& out) const;
		//! Returns area of t-th triangle.
		//
		//! Be sure to provide valid t is in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		//! \n There is default implementation, but if you know format of your data well, you may provide faster one.
		virtual RRReal       getTriangleArea(unsigned t) const;

		//
		// optional for advanced importers
		//  post import number is always plain unsigned, 0..num-1
		//  pre import number is implementation defined
		//  all numbers in interface are post import, except for following preImportXxx:
		//
		//! Returns PreImport index of given vertex or UNDEFINED for invalid inputs.
		virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const {return postImportVertex;}
		//! Returns PostImport index of given vertex or UNDEFINED for invalid inputs.
		virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const {return preImportVertex;}
		//! Returns PreImport index of given triangle or UNDEFINED for invalid inputs.
		virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const {return postImportTriangle;}
		//! Returns PostImport index of given triangle or UNDEFINED for invalid inputs.
		virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const {return preImportTriangle;}
		enum 
		{
			UNDEFINED = UINT_MAX ///< Index value reserved for situations where result is undefined, for example because of invalid inputs.
		};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory
		//! Identifiers of data formats.
		enum Format
		{
			UINT8   = 0, ///< Id of uint8_t.
			UINT16  = 1, ///< Id of uint16_t.
			UINT32  = 2, ///< Id of uint32_t.
			FLOAT32 = 4, ///< Id of float.
		};
		//! Flags that help to specify your create() or createIndexed() request.
		enum Flags
		{
			TRI_LIST            = 0,      ///< Interpret data as triangle list.
			TRI_STRIP           = (1<<0), ///< Interpret data as triangle strip.
			OPTIMIZED_VERTICES  = (1<<1), ///< Remove identical and unused vertices.
			OPTIMIZED_TRIANGLES = (1<<2), ///< Remove degenerated triangles.
		};
		//! Structure of PreImport index in MultiMeshes created by createMultiMesh().
		//
		//! Underlying importers must use PreImport values that fit into index, this is not runtime checked.
		//! This implies that it is not allowed to create MultiMesh from MultiMeshes.
		struct MultiMeshPreImportNumber 
		{
			unsigned index : sizeof(unsigned)*8-12; ///< Original PreImport index of element (vertex or triangle). For 32bit int: max 1M triangles/vertices in one object.
			unsigned object : 12; ///< Index into array of original meshes. For 32bit int: max 4k objects.
			MultiMeshPreImportNumber() {}
			MultiMeshPreImportNumber(unsigned aobject, unsigned aindex) {index=aindex;object=aobject;}
			MultiMeshPreImportNumber(unsigned i) {*(unsigned*)this = i;} ///< Implicit unsigned -> MultiMeshPreImportNumber conversion.
			operator unsigned () {return *(unsigned*)this;} ///< Implicit MultiMeshPreImportNumber -> unsigned conversion.
		};
		//! Creates %RRMeshImporter from your vertex buffer.
		//
		//! \param flags See #Flags.
		//! \param vertexFormat %Format of data in your vertex buffer. See #Format. Currently only FLOAT32 is supported.
		//! \param vertexBuffer Your vertex buffer.
		//! \param vertexCount Number of vertices in your vertex buffer.
		//! \param vertexStride Distance (in bytes) between n-th and (n+1)th vertex in your vertex buffer.
		//! \return Newly created instance of RRMeshImporter or NULL in case of unsupported or invalid inputs.
		static RRMeshImporter* create(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride);
		//! Creates %RRMeshImporter from your vertex and index buffers.
		//
		//! \param flags See #Flags.
		//! \param vertexFormat %Format of data in your your vertex buffer. See #Format. Currently only FLOAT32 is supported.
		//! \param vertexBuffer Your vertex buffer.
		//! \param vertexCount Number of vertices in your vertex buffer.
		//! \param vertexStride Distance (in bytes) between n-th and (n+1)th vertex in your vertex buffer.
		//! \param indexFormat %Format of data in your index buffer. See #Format. Only UINT8, UINT16 and UINT32 is supported.
		//! \param indexBuffer Your index buffer.
		//! \param indexCount Number of indices in your index buffer.
		//! \param vertexStitchMaxDistance Max distance for vertex stitching. For default 0, vertices with equal coordinates are stitched and get equal vertex index (number of vertices returned by getNumVertices() is then lower). For negative value, no stitching is performed. For positive value, also vertices in lower or equal distance will be stitched.
		//! \return Newly created instance of RRMeshImporter or NULL in case of unsupported or invalid inputs.
		static RRMeshImporter* createIndexed(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride, Format indexFormat, void* indexBuffer, unsigned indexCount, float vertexStitchMaxDistance = 0);
		//! Creates and returns copy of your instance.
		//
		//! Created copy is completely independent on any other objects and may be deleted sooner or later.
		//! \n It is expected that your input instance is well formed (returns correct and consistent values).
		//! \n Copy may be faster than original, but may require more memory.
		RRMeshImporter*        createCopy();
		//! Creates and returns union of multiple meshes (contains vertices and triangles from all meshes).
		//
		//! Created instance (MultiMesh) doesn't require additional memory, 
		//! but it depends on all meshes from array, they must stay alive for whole life of MultiMesh.
		//! \n This can be used to accelerate calculations, as one big object is nearly always faster than multiple small objects.
		//! \n This can be used to simplify calculations, as processing one object may be simpler than processing array of objects.
		//! \n For array with 1 element, pointer to that element may be returned.
		//! \n\n If you need to locate original triangles and vertices in MultiMesh, you have two choices:
		//! -# Use PreImpport<->PostImport conversions. PreImport number for MultiMesh is defined as MultiMeshPreImportNumber.
		//!  If you want to access triangle 2 in meshes[1], calculate index of triangle in MultiMesh as
		//!  indexOfTriangle = multiMesh->getPostImportTriangle(MultiMeshPreImportNumber(2,1)).
		//! -# Convert indices yourself. It is granted, that both indices and vertices preserve order of meshes in array:
		//!  lowest indices belong to meshes[0], meshes[1] follow etc. If you create MultiMesh from 2 meshes,
		//!  first with 3 vertices and second with 5 vertices, they will transform into 0,1,2 and 3,4,5,6,7 vertices in MultiMesh.
		static RRMeshImporter* createMultiMesh(RRMeshImporter* const* meshes, unsigned numMeshes);
		//! Creates and returns mesh with optimized vertices.
		//
		//! \param vertexStitchMaxDistance
		//!  For default 0, vertices with equal coordinates are stitched and get equal vertex index (number of vertices returned by getNumVertices() is then lower).
		//!  For negative value, no stitching is performed.
		//!  For positive value, also vertices in lower or equal distance will be stitched.
		RRMeshImporter*        createOptimizedVertices(float vertexStitchMaxDistance = 0);

		// verification
		//! Callback for reporting text messages.
		typedef void Reporter(const char* msg, void* context);
		//! Verifies that mesh is well formed.
		//
		//! \param reporter All inconsistencies are reported using this callback.
		//! \param context This value is sent to reporter without any modifications from verify.
		//! \returns Number of reports performed.
		unsigned               verify(Reporter* reporter, void* context);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRMeshSurfaceImporter
	//! Interface for non-trivial behaviour of mesh surfaces.
	//
	//! Derive to define behaviour of YOUR surfaces.
	//! You need it only when you want to
	//! - intersect mesh that contains both singlesided and twosided faces
	//! - iterate over multiple intersections with mesh
	//!
	//! It's intentionally not part of RRMeshImporter, so you can easily combine
	//! different surface behaviours with one geometry.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRMeshSurfaceImporter
	{
	public:
		virtual ~RRMeshSurfaceImporter() {}

		//! Arbitrates whether to accept or not to accept given hit (intersection of ray x mesh).
		//
		//! acceptHit is called at each intersection of ray x mesh.
		//! \n For IT_BSP techniques, intersections are reported in order from the nearest one.
		//! For IT_LINEAR technique, intersections go unsorted.
		//! \returns If you want ray to continue penetrating mesh in the same direction and finding further intersections, return true.
		virtual bool         acceptHit(const class RRRay* ray) = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRAligned
	//! Base class for objects aligned in memory as required by SIMD instructions.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRAligned
	{
	public:
		//! Allocates aligned space for instance of any derived class.
		void* operator new(std::size_t n);
		//! Allocates aligned space for array of instances of any derived class.
		void* operator new[](std::size_t n);
		//! Frees aligned space allocated by new.
		void operator delete(void* p, std::size_t n);
		//! Frees aligned space allocated by new[].
		void operator delete[](void* p, std::size_t n);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRRay
	//! Ray to intersect with object.
	//
	//! Contains all inputs and outputs for RRCollider::intersect().
	//! All fields of at least 3 floats are aligned for SIMD.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRRay : public RRAligned
	{
	public:
		//! Creates 1 RRRay. All is zeroed, all FILL flags on. You may destroy it by delete.
		static RRRay* create();
		//! Creates array of RRRays. You may destroy them by delete[].
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
			TEST_SINGLESIDED=(1<<6), ///< Detect collision only against front side. Default is to test both sides.
		};
		// inputs
		RRVec4          rayOrigin;      ///< In. (-Inf,Inf), ray origin. Never modify last component, it must stay 1.
		RRVec4          rayDirInv;      ///< In. <-Inf,Inf>, 1/ray direction. Direction must be normalized.
		RRReal          rayLengthMin;   ///< In. <0,Inf), test intersection in distances from range <rayLengthMin,rayLengthMax>.
		RRReal          rayLengthMax;   ///< In. <0,Inf), test intersection in distances from range <rayLengthMin,rayLengthMax>.
		unsigned        rayFlags;       ///< In. Flags that specify what to find.
		RRMeshSurfaceImporter* surfaceImporter; ///< In. Optional surface importer for user-defined surface behaviour.
		// outputs (valid after positive test, undefined otherwise)
		RRReal          hitDistance;    ///< Out. Hit distance in object space.
		unsigned        hitTriangle;    ///< Out. Index of triangle (postImport) that was hit.
		RRVec2          hitPoint2d;     ///< Out. Hit coordinate in triangle space (vertex[0]=0,0 vertex[1]=1,0 vertex[2]=0,1).
		RRVec4          hitPlane;       ///< Out. Plane of hitTriangle in object space. RRVec3 part is normalized.
		RRVec3          hitPoint3d;     ///< Out. Hit coordinate in object space.
		bool            hitFrontSide;   ///< Out. True = face was hit from the front side.
		RRVec4          hitPadding[2];  ///< Out. Undefined, never modify.
	private:
		RRRay(); // intentionally private so one can't accidentally create unaligned instance
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRCollider
	//! Single object able to calculate ray x trimesh intersections.
	//
	//! Mesh is defined by importer passed to create().
	//! All results from importer must be constant in time, 
	//! otherwise collision results are undefined.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRCollider
	{
	public:
		// create
		enum IntersectTechnique
		{
			IT_LINEAR,          ///< Speed   1%, size   0. Fallback technique when better one fails.
			IT_BSP_COMPACT,     ///< Speed 100%, size  ~5 bytes per triangle. Optimal for platforms with limited memory.
			IT_BSP_FAST,        ///< Speed 175%, size ~31 bytes per triangle.
			IT_BSP_FASTEST,     ///< Speed 200%, size ~58 bytes per triangle.
			IT_VERIFICATION,    ///< Only for verification purposes, performs tests using all known techniques and compares results.
		};
		//! Creates and returns collider, acceleration structure for finding ray x mesh intersections.
		//
		//! \param importer Importer of mesh you want to collide with.
		//! \param intersectTechnique Technique used for accelerating collision searches. See #IntersectTechnique.
		//! \param cacheLocation Optional location of cache, path to directory where acceleration structures may be cached.
		//! \param buildParams Optional additional parameters, specific for each technique and not revealed for public use.
		static RRCollider*   create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, const char* cacheLocation=NULL, void* buildParams=0);

		//! Finds ray x mesh intersections.
		//
		//! \param ray All inputs and outputs for search.
		//! \returns Whether intersection was found and reported into ray.
		//!
		//! Finds nearest intersection of ray and mesh in distance
		//! <ray->rayLengthMin,ray->rayLengthMax> and fills output attributes in ray
		//! specified by ray->rayFlags.
		//! \n When ray->surfaceImporter!=NULL, it is called and it may accept or refuse intersection.
		//! \n When intersection is accepted, true is returned.
		//! When intersection is rejected, search continues.
		//! \n False is returned when there is no accepted intersection.
		//!
		//! There is exception for IntersectTechnique IT_LINEAR, intersections are found in random order.
		//!
		//! \section SurfaceImporter
		//! Ray->surfaceImporter may be used
		//! - To gather all intersections instead of just first one. SurfaceImporter may gather 
		//!   or immediately process them. This is faster and more precise approach than multiple
		//!   calls of %intersect() with increasing rayLengthMin to distance of last intersection.
		//! - To not collide with optional parts of mesh that are turned off at this moment.
		//! - To find pixel precise collisions with alpha keyed textures.
		//! - To collide with specific probability.
		//!
		//! \section Multithreading
		//! You are encouraged to find multiple intersections in multiple threads at the same time.
		//! This will improve your performance on multicore CPUs. \n Even with Intel's hyperthreading,
		//! which is inferior to two fully-fledged cores, searching multiple intersections at the same time brings
		//! surprisingly high performance bonus.
		//! \n All you need is one RRRay for each thread, other structures like mesh and collider
		//!  may be accessed by arbitrary number of threads simultaneously.
		//! \n If you are not familiar with OpenMP, be sure to examine it. With OpenMP, which is built-in 
		//!  feature of modern compilers, searching multiple intersections
		//!  at the same time is matter of one or few lines of code.
		virtual bool         intersect(RRRay* ray) const = 0;

		// helpers
		//! \returns Importer that was passed to create().
		virtual RRMeshImporter*    getImporter() const = 0;
		//! \returns Technique used by collider. May differ from technique requested in create().
		virtual IntersectTechnique getTechnique() const = 0;
		//! \returns Total amount of system memory occupied by collider.
		virtual unsigned           getMemoryOccupied() const = 0;
		virtual ~RRCollider() {};
	};




	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRLicense
	//! Everything related to your license number.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRLicense
	{
	public:
		//! Lets you present your license information.
		//
		//! This must be called before any other work with library.
		static void registerLicense(char* licenseOwner, char* licenseNumber);
	};

} // namespace

#endif
