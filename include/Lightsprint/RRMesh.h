#ifndef RRMESH_H
#define RRMESH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMesh.h
//! \brief LightsprintCore | unified mesh interface
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2011
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRDebug.h"
#include "RRHash.h"
#include "RRVector.h"
#include <climits> // UNDEFINED/UINT_MAX

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRMesh
	//! Common interface for any standard or proprietary triangle mesh structure.
	//
	//! RRMesh is typically only several bytes big adaptor, it provides unified 
	//! interface to your mesh, but it uses your mesh data, no data need to be duplicated.
	//!
	//! Thread safe: yes, may be accessed by any number of threads simultaneously.
	//!
	//! \section s1 Creating instances
	//!
	//! %RRMesh has built-in support for standard mesh formats used by
	//! rendering APIs - vertex and index buffers using triangle lists or
	//! triangle strips. See create() and createIndexed().
	//!
	//! %RRMesh has built-in support for baking multiple meshes 
	//! into one mesh (without need for additional memory). 
	//! This may simplify mesh oprations or improve performance in some situations.
	//! See createMultiMesh().
	//!
	//! %RRMesh has built-in support for creating self-contained mesh copies.
	//! See createCopy().
	//! While importers created from vertex buffer doesn't allocate more memory 
	//! and depend on vertex buffer, self-contained copy contains all mesh data
	//! and doesn't depend on any other objects.
	//!
	//! For other mesh formats (heightfield, realtime generated etc), 
	//! you may easily derive from %RRMesh and create your own mesh adaptor.
	//!
	//! \section s2 Optimizations
	//!
	//! %RRMesh may help you with mesh optimizations if requested,
	//! for example by removing duplicate vertices or degenerated triangles.
	//! 
	//! \section s3 Constancy
	//!
	//! All data provided by %RRMesh must be constant in time.
	//! Built-in importers guarantee constancy if you don't change
	//! their vertex/index buffers. Constancy of mesh copy is guaranteed always.
	//!
	//! \section s4 Indexing
	//!
	//! %RRMesh operates with two types of vertex and triangle indices.
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
	//!
	//! \section s5_frontback Front/back side
	//!
	//! For correct lighting, it's important to know where front and back
	//! sides of triangle are.
	//! Materials (see RRMaterial) define some properties separately for both sides of triangle.
	//! Renderers render both sides of triangle differently.
	//!
	//! When you see triangle vertices in CCW order, you see triangle's front side.
	//! In this situation triangle's normal must point to your (front) hemisphere.
	//!
	//! Some applications are known to define front/back differently:
	//! - 3DS MAX
	//! - Quake3
	//!
	//! How to swap front/back sides, when importing data from such applications?
	//! - Negate positions and normals in any 1 axis. If you don't provide normals
	//!   in your mesh, negate positions only. See example in RRObjectBSP.cpp, getVertex.
	//! - Or swap any 2 vertices in triangle.
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRMesh : public RRUniformlyAllocatedNonCopyable
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		RRMesh();
		virtual ~RRMesh();


		//
		// vertices
		//

		//! One vertex 3d space.
		typedef RRVec3 Vertex;

		//! Returns number of vertices in mesh.
		virtual unsigned     getNumVertices() const = 0;

		//! Writes position of v-th vertex in mesh to out.
		//
		//! Make sure you provide valid v is in range <0..getNumVertices()-1>.
		//! Implementations are allowed to expect valid v,
		//! result is undefined for invalid v (possible assert in debug, crash in release).
		//! 
		//! What exactly is vertex, do boxes have 8 or 24 vertices?
		//! \n RRMesh, RRCollider and RRObject create no constraints, you are free
		//!  to use your favorite approach - create box with 8 or 24
		//!  or any other number of vertices greater than 8, collisions and 
		//!  illumination will be computed correctly.
		//!  (24 because each one of 8 vertices is used by 3 sides with different normal)
		//! \n RRDynamicSolver depends on vertex list defined here.
		//!  If you request vertex buffer with per-vertex illumination,
		//!  vertex buffer will have getNumVertices() items.
		//!  So when writing new RRMesh implementations, create vertex list
		//!  so that vertex buffers with the same size and vertex order
		//!  are compatible with your renderer.
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
		//! Make sure you provide valid t in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t (performance reasons, may be called very often),
		//! so result is completely undefined for invalid t (possible crash).
		//!
		//! Order of vertices in triangle has influence on what side of triangle is front, which is important for lighting.
		//! See more details in \ref s5_frontback.
		virtual void         getTriangle(unsigned t, Triangle& out) const = 0;


		//
		// optional for faster access
		//

		//! %Triangle in 3d space defined by one vertex and two side vectors. This representation is most suitable for intersection tests.
		struct TriangleBody  {Vertex vertex0,side1,side2; bool isNotDegenerated() const;};
		//! Writes t-th triangle in mesh to out.
		//
		//! Make sure you provide valid t in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		//! \n There is default implementation, but if you know format of your data well, you may provide faster one.
		//! \n Speed of this function is important for intersection tests performance.
		virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;

		//! Writes t-th triangle plane to out.
		//
		//! Be sure to provide valid t is in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		//! \n There is default implementation, but if you know format of your data well, you may provide faster one.
		virtual bool         getTrianglePlane(unsigned t, RRVec4& out) const;

		//! Returns area of t-th triangle.
		//
		//! Be sure to provide valid t is in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		//! \n There is default implementation, but if you know format of your data well, you may provide faster one.
		virtual RRReal       getTriangleArea(unsigned t) const;

		//! Tangent basis in object space.
		//
		//! Normal must be normalized.
		//! Orthonormality is not mandatory.
		//! Tangent and bitangent affect only directional lightmaps.
		struct RR_API TangentBasis
		{
			RRVec3 normal;
			RRVec3 tangent;
			RRVec3 bitangent;
			//! Build orthonormal base from normalized normal.
			void buildBasisFromNormal();
		};
		//! Tangent bases in object space, for three vertices in triangle.
		struct TriangleNormals
		{
			TangentBasis vertex[3];
		};
		//! Writes tangent bases in triangle vertices to out. Normals are part of bases.
		//
		//! Tangent bases are used by global illumination solver and renderer.
		//! Normals should point to front side hemisphere, see \ref s5_frontback.
		//! \n Default implementation writes all vertex normals equal to triangle plane normal
		//! and constructs appropriate tangent space.
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param out Caller provided storage for result.
		//!  For valid t, requested normals are written to out. For invalid t, out stays unmodified.
		virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;

		//! Uv-coordinates of three vertices in triangle.
		struct TriangleMapping      {RRVec2 uv[3];};
		//! Writes t-th triangle's uv mapping to out.
		//
		//! \param t
		//!  Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param out
		//!  Caller provided storage for result.
		//!  For valid t, requested mapping is written to out. For invalid t, out stays unmodified.
		//! \param channel
		//!  Texcoord channel to use,
		//!  RRMaterial::diffuseReflectance.texcoord for diffuse texture mapping,
		//!  RRMaterial::lightmapTexcoord for unwrap used by lightmaps etc.
		//!  \n Note that for proper lighting, unwrap
		//!  must have all coordinates in <0..1> range and triangles must not overlap.
		//!  Unwrap may be imported or automatically generated by RRObjects::buildUnwrap().
		//! \return
		//!  True for valid t and supported channel, result was written to out.
		//!  False for invalid t or unsupported channel, out stays unmodified.
		virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const;


		//
		// optional for advanced importers
		//  post import number is always plain unsigned, 0..num-1
		//  pre import number is arbitrary, implementation defined
		//  all numbers in interface are post import, except for following preImportXxx:
		//

		//! PreImport number, index of vertex or triangle before import.
		struct PreImportNumber
		{
			//! PreImport index, arbitrary triangle or vertex number in single mesh before import.
			unsigned index;
			//! PreImport object number, index into array of objects in scene.
			unsigned object;
			PreImportNumber(unsigned _object, unsigned _index) {index=_index;object=_object;}
			PreImportNumber() {}
			bool operator ==(const PreImportNumber& a) {return index==a.index && object==a.object;}
		};

		//! Returns PreImport index of given vertex or UNDEFINED for invalid inputs.
		virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const {return PreImportNumber(0,postImportVertex);}

		//! Returns PostImport index of given vertex or UNDEFINED for invalid inputs.
		virtual unsigned getPostImportVertex(PreImportNumber preImportVertex, PreImportNumber preImportTriangle) const {return preImportVertex.index;}

		//! Returns PreImport index of given triangle or UNDEFINED for invalid inputs.
		virtual PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const {return PreImportNumber(0,postImportTriangle);}

		//! Returns PostImport index of given triangle or UNDEFINED for invalid inputs.
		virtual unsigned getPostImportTriangle(PreImportNumber preImportTriangle) const {return preImportTriangle.index;}

		//! Returns highest PreImport vertex number+1.
		//
		//! PreImport numbers are used as positions of vertices in vertex buffer, so returned number is also size of vertex buffer.
		//! Very slow (checks all vertices).
		unsigned getNumPreImportVertices() const;

		enum 
		{
			UNDEFINED = UINT_MAX ///< Index value reserved for situations where result is undefined, for example because of invalid inputs.
		};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Fills out with uv channels provided by mesh, in ascending order.
		//! Default implementation queries presence of channels 0 to 100, ignores higher channels.
		virtual void         getUvChannels(rr::RRVector<unsigned>& out) const;

		//! Returns axis aligned bounding box and center of mesh. Fast (cached).
		//
		//! \param mini
		//!  NULL or pointer to vec3 to be filled with minimum of computed AABB.
		//! \param maxi
		//!  NULL or pointer to vec3 to be filled with maximum of computed AABB.
		//! \param center
		//!  NULL or pointer to vec3 to be filled with average vertex position.
		virtual void         getAABB(RRVec3* mini, RRVec3* maxi, RRVec3* center) const;

		//! Returns average distance between two vertices. Slow (not cached).
		virtual RRReal       getAverageVertexDistance() const;

		//! Returns median of edge length / edge length in texture space. Slow (not cached).
		virtual RRReal       getMappingDensity(unsigned channel) const;
		
		//! Returns y coordinate of plane where triangles facing straight up have the biggest total area.
		//! In CG scenes, this is usually flat ground. Slow (not cached).
		virtual RRReal       findGroundLevel() const;

		//! Returns hash of mesh geometry (positions, not normals and uvs). Slow (not cached).
		virtual RRHash       getHash() const;

		//! Reports inconsistencies found in mesh.
		//
		//! \param lightmapTexcoord
		//!  Optional lightmap texcoord channel. UINT_MAX disables unwrap check.
		//! \param meshName
		//!  Optional mesh name included in report if inconsistency is found. May be NULL.
		//! \param numReports
		//!  Reserved, keep it NULL.
		//! \return
		//!  Number of problem reported, 0 for valid mesh.
		virtual unsigned     checkConsistency(unsigned lightmapTexcoord, const char* meshName, class NumReports* numReports = NULL) const;


		//////////////////////////////////////////////////////////////////////////////
		// Instance factory
		//////////////////////////////////////////////////////////////////////////////

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

		//! Creates %RRMesh from your vertex buffer.
		//
		//! \param flags See #Flags. Note that optimizations are not implemented for triangle lists, OPTIMIZE_XXX flags will be silently ignored.
		//! \param vertexFormat %Format of data in your vertex buffer. See #Format. Currently only FLOAT32 is supported.
		//! \param vertexBuffer Your vertex buffer.
		//! \param vertexCount Number of vertices in your vertex buffer.
		//! \param vertexStride Distance (in bytes) between n-th and (n+1)th vertex in your vertex buffer.
		//! \return Newly created instance of RRMesh or NULL in case of unsupported or invalid inputs.
		static RRMesh* create(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride);

		//! Creates %RRMesh from your vertex and index buffers.
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
		//! \return Newly created instance of RRMesh or NULL in case of unsupported or invalid inputs.
		static RRMesh* createIndexed(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride, Format indexFormat, void* indexBuffer, unsigned indexCount, float vertexStitchMaxDistance = 0);


		//! Creates and returns transformed mesh.
		//
		//! Created instance doesn't require additional memory, 
		//! but it depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//!
		//! Usually used when mesh in world space is needed and we have mesh in local space.
		//! In this case, world space matrix (the one that transforms from local to world) should be passed in transform.
		//!
		//! Only positions/normals/tangent space are transformed, custom channels are left untouched.
		//!
		//! Non-uniform transformations break tangent space orthogonality.
		RRMesh* createTransformed(const RRMatrix3x4* transform) const;

		//! Creates and returns union of multiple meshes (contains vertices and triangles of all meshes).
		//
		//! Created instance (MultiMesh) doesn't require additional memory, 
		//! but it depends on all meshes from array, they must stay alive for whole life of MultiMesh.
		//! \n This can be used to accelerate calculations, as one big object is nearly always faster than multiple small objects.
		//! \n This can be used to simplify calculations, as processing one object may be simpler than processing array of objects.
		//! \n For array with 1 element, pointer to that element may be returned.
		//! \n\n If you need to locate original triangles and vertices in MultiMesh, you have two choices:
		//! -# Use PreImpport<->PostImport conversions. PreImport number for MultiMesh is defined as PreImportNumber.
		//!  If you want to access triangle 2 in meshes[1], calculate index of triangle in MultiMesh as
		//!  indexOfTriangle = multiMesh->getPostImportTriangle(PreImportNumber(1,2)).
		//! -# Convert indices yourself. It is granted, that both indices and vertices preserve order of meshes in array:
		//!  lowest indices belong to meshes[0], meshes[1] follow etc. If you create MultiMesh from 2 meshes,
		//!  first with 3 vertices and second with 5 vertices, they will transform into 0,1,2 and 3,4,5,6,7 vertices in MultiMesh.
		//! \param meshes
		//!  Array of meshes, source data for MultiMesh.
		//! \param numMeshes
		//!  Length of 'meshes' array.
		//! \param fast
		//!  With false, multimesh has fixed size of several bytes.
		//!  With true, multimesh allocates 8 bytes per triangle, but it is faster,
		//!  especially in scenes made of hundreds of small meshes.
		static const RRMesh* createMultiMesh(const RRMesh* const* meshes, unsigned numMeshes, bool fast);

		//! Creates and returns nearly identical mesh with optimized set of vertices (removes duplicates).
		//
		//! Stitches identical or similar vertices so that number of vertices in returned mesh decreases.
		//! Only vertex positions, normals and uvs are tested, so vertices with completely
		//! different tangents may be stitched.
		//!
		//! Created instance requires only small amount of additional memory, 
		//!  but it depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//! If no vertex can be removed, 'this' is returned.
		//! \param maxDistanceBetweenVerticesToStitch
		//!  For negative value, no stitching is performed.
		//!  For 0, vertices with identical positions may be stitched.
		//!  For positive value, also vertices in lower or equal distance may be stitched.
		//! \param maxRadiansBetweenNormalsToStitch
		//!  For negative value, no stitching is performed.
		//!  For 0, vertices with identical normals may be stitched.
		//!  For positive value, also vertices with lower or equal angle between normals may be stitched.
		//! \param texcoords
		//!  Vertices are stitched only if they exactly match in selected texcoord channels.
		//!  NULL = texcoords are ignored, vertices don't have to match to be stitched.
		const RRMesh* createOptimizedVertices(float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, const RRVector<unsigned>* texcoords) const;

		//! Creates and returns identical mesh with optimized set of triangles (removes degenerated triangles).
		//
		//! Created instance requires only small amount of additional memory, 
		//!  but it depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//! If 'this' is already optimal, 'this' is returned.
		const RRMesh* createOptimizedTriangles() const;

		//! Creates and returns accelerated mesh.
		//
		//! Created instance caches all TriangleBase and TriangleNormals for whole mesh,
		//! so it allocates 144 bytes per triangle.
		//! It still depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//!
		//! It is very efficient when applied on multimesh made of hundreds of smaller meshes.
		//! It accelerates getTriangleBase() and getTriangleNormals().
		//! These functions are critical for performance of BSP_COMPACT and BSP_FAST colliders.
		//! Colliders are critical for performance of lightmap building.
		//!
		//! In practice, this function is not needed for lightmap building, use collider types
		//! BSP_COMPACT, BSP_FAST, BSP_FASTER, BSP_FASTEST in RRDynamicSolver::setStaticObjects()
		//! to trade performance / memory.
		const RRMesh* createAccelerated() const;

		//! Creates and returns identical mesh with all optimizations and filters previously applied baked.
		//
		//! Created instance doesn't require additional memory, 
		//!  but it depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//!
		//! All other mesh filters and optimizers let you convert between original and new triangle
		//! and vertex numbers using getPreImportVertex() and getPreImportTriangle().
		//! This filter only erases original (preImport) numbers and sets them equal to current (postImport) numbers.
		//! PreImport numbers are used by RRDynamicSolver for vertex buffer layout, so this filter
		//! adjusts layout of generated vertex buffers to use current vertex numbers.
		RRMesh* createVertexBufferRuler() const;

		//! Creates mesh with direct read-write access to internal data arrays.
		//
		//! Created mesh does not depend on the old one, it's safe to delete the old one.
		//! \n Note that created mesh contains reduced set of data
		//! - only selected texcoords
		//! - if indexed=true, shared vertex can't have different normal/tangents/mappings in different triangles
		//! - Pre/PostImportNumbers are lost
		class RRMeshArrays* createArrays(bool indexed, const RRVector<unsigned>& texcoords) const;

	protected:
		struct AABBCache* aabbCache; //! Cached results of getAABB().
	};


	//! Mesh that exposes internal structures for direct read-write access.
	//
	//! You may freely edit data in arrays, however, resizing mesh (changing numVertices, numTriangles) is restricted,
	//! it is not allowed if other meshes were derived from this one.
	//! Derived meshes are e.g. multiobject or worldspace object,
	//! they may depend on original mesh size and crash if it changes.
	//!
	//! When do we need it? Lightsprint has two major use cases:
	//! - Calculating GI for third party engine. In this case, we need only read-only access to individual
	//!   triangles and vertices and RRMesh already provides it via getTriangle()/getVertex().
	//!   RRMesh implementations usually read vertices and triangles directly from third party engine structures in memory;
	//!   duplicating data into RRMeshArrays would only waste memory.
	//! - Complete application development - render GI, build unwrap, manipulate meshes, vertices etc (without third party engine).
	//!   In this case, we need also write access and RRMeshArrays provides it.
	//!   There are no third party structures in memory, all data are stored in RRMeshArrays.
	//!
	//! If you need direct read-write access to data in RRMesh, try RTTI first, you may already have RRMeshArrays.
	//! If you don't have RRMeshArrays, create it via RRMesh::createArrays().
	class RR_API RRMeshArrays : public RRMesh
	{
	public:

		// Per-triangle data.
		unsigned numTriangles;
		Triangle* triangle; ///< 32bit triangle list, may be NULL only in completely empty mesh.

		// Per-vertex data, use resizeMesh() for allocations.
		unsigned numVertices;
		RRVec3* position; ///< May be NULL only in completely empty mesh.
		RRVec3* normal; ///< May be NULL only in completely empty mesh.
		RRVec3* tangent; ///< May be NULL.
		RRVec3* bitangent; ///< May be NULL.
		RRVector<RRVec2*> texcoord; ///< May contain mix of NULL and non-NULL channels, e.g. texcoord[5] array is missing but texcoord[6] array is present.

		//! Increase version each time you modify arrays, to let renderer know data in GPU are outdated.
		unsigned version;

		//! Memory management. Resizing doesn't preserve old data.
		//
		//! If you resize often, it's safe to resize once to max size and then change only numTriangles/numVertices.
		//! If allocation fails, mesh is resized to 0 and false is returned.
		bool                 resizeMesh(unsigned numTriangles,unsigned numVertices, const rr::RRVector<unsigned>* texcoords, bool tangents);

		//! Overwrites content of this RRMeshArrays, copies data from given RRMesh.
		//
		//! \param mesh
		//!  Data source, mesh data are copied to this.
		//! \param indexed
		//!  False splits vertices, makes numVertices=3*numTriangles.
		//! \param texcoords
		//!  Lets you specify what texcoord channels to copy from original mesh.
		bool                 reload(const RRMesh* mesh, bool indexed, const RRVector<unsigned>& texcoords);


		//////////////////////////////////////////////////////////////////////////////
		// Implementation of RRMesh interface
		//////////////////////////////////////////////////////////////////////////////

		RRMeshArrays();
		virtual ~RRMeshArrays();
		virtual unsigned     getNumVertices() const;
		virtual void         getVertex(unsigned v, Vertex& out) const;
		virtual unsigned     getNumTriangles() const;
		virtual void         getTriangle(unsigned t, Triangle& out) const;
		virtual void         getTriangleBody(unsigned i, TriangleBody& out) const;
		virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
		virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const;
		virtual void         getUvChannels(rr::RRVector<unsigned>& out) const;
		virtual void         getAABB(RRVec3* mini, RRVec3* maxi, RRVec3* center) const;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Flips front/back if at least this number of normals in triangle points to back side.
		//! So all triangles are flipped if numNormalsThatMustPointBack==0.
		//! \return Number of triangles flipped.
		unsigned             flipFrontBack(unsigned numNormalsThatMustPointBack);
	private:
		unsigned poolSize; ///< All arrays in mesh are allocated from one pool of this size.
	};

} // namespace

#endif
