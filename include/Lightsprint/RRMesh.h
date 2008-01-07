#ifndef RRMESH_H
#define RRMESH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMesh.h
//! \brief LightsprintCore | unified mesh interface
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2008
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRDebug.h"
#include <climits> // UNDEFINED

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRChanneledData
	//! Common interface for data stored in channels - arrays of implementation defined size and type.
	//
	//! It is common in computer graphics applications to store many different kinds of data
	//! for each vertex in mesh, other kind of data for each triangle in mesh etc.
	//!
	//! This interface gives access to all such informations and is extensible enough
	//! so we don't need to know types of data now. Each implementation defines its own data types.

	class RR_API RRChanneledData : public RRUniformlyAllocated
	{
	public:
		//! Writes size of selected channel into numItems and itemSize.
		//
		//! Call it to query channel presence and check its size.
		//! Querying unsupported channels should not generate any warnings.
		//! \param channelId Id of channel, e.g. RRMesh::CHANNEL_VERTEX_POS - channel holding vertex positions.
		//!  Each class that implements this method (e.g. RRMesh, RRObject) defines supported channel ids
		//!  CHANNEL_xxx.
		//! \param numItems When not NULL, it is filled with number of items. Items are indexed by 0..numItems-1.
		//!  Zero is filled for unknown channel.
		//! \param itemSize When not NULL, it is filled with size of one item in bytes.
		//!  Item type (and thus size) for each channel is part of each CHANNEL_xxx description.
		//!  Zero is filled for unknown channel.
		virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;

		//! Copies one data item from selected channel into buffer provided by you.
		//
		//! To query channel presence, call getChannelSize().
		//! Querying unsupported channels here should generate warning, see RRReporter.
		//! \param channelId Id of channel, e.g. RRMesh::CHANNEL_VERTEX_POS - channel holding vertex positions.
		//!  Each class that implements this method (e.g. RRMesh, RRObject) defines supported channel ids
		//!  CHANNEL_xxx.
		//! \param itemIndex Index of intem inside channel. Items are indexed from 0 to numItems-1,
		//!  see getChannelSize for numItems.
		//! \param itemData When not NULL, data item from selected channel and index
		//!  is copied to it. It is your responsibility to provide big enough buffer. 
		//!  Item will be copied into first itemSize bytes of buffer.
		//!  See getChannelSize for size of one item.
		//! \param itemSize Size of one item. Both you and implementation must know 
		//!  this value, this parameter exists only for security reasons. If your size 
		//!  doesn't match what implementation expects, item is not copied.
		//!  See getChannelSize for what implementation thinks about item size.
		//! \return True when itemSize is correct, item exists and was copied.
		//!  False when item doesn't exist or itemSize doesn't fit and nothing was copied.
		virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

		virtual ~RRChanneledData() {};
	};


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

	class RR_API RRMesh : public RRChanneledData
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Channels
		//////////////////////////////////////////////////////////////////////////////

		enum
		{
			INDEXED_BY_VERTEX                = 0x0000,
			INDEXED_BY_TRIANGLE              = 0x1000,
			INDEXED_BY_OBJECT                = 0x3000,
			CHANNEL_TRIANGLE_DIFFUSE_TEX         = INDEXED_BY_TRIANGLE+5, ///< channel contains RRBuffer* for each triangle
			CHANNEL_TRIANGLE_EMISSIVE_TEX        = INDEXED_BY_TRIANGLE+6, ///< channel contains RRBuffer* for each triangle
			CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV = INDEXED_BY_TRIANGLE+7, ///< channel contains RRVec2[3] for each triangle
			CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV= INDEXED_BY_TRIANGLE+8, ///< channel contains RRVec2[3] for each triangle
			CHANNEL_TRIANGLE_OBJECT_ILLUMINATION = INDEXED_BY_TRIANGLE+9, ///< channel contains RRObjectIllumination* for each triangle
		};

		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRMesh() {}


		//
		// vertices
		//

		//! One vertex 3d space.
		typedef RRVec3 Vertex;

		//! Returns number of vertices in mesh.
		virtual unsigned     getNumVertices() const = 0;

		//! Writes position of v-th vertex in mesh to out.
		//
		//! Be sure to provide valid v is in range <0..getNumVertices()-1>.
		//! Implementators are allowed to expect valid v, so result is completely undefined for invalid v (possible crash).
		//! 
		//! What exactly is vertex, do boxes have 8 or 24 vertices?
		//! \n RRMesh, RRCollider and RRVision create no constraints, you are free
		//!  to use your favorite approach - create box with 8 or 24
		//!  or any other number of vertices greater than 8, collisions and 
		//!  illumination will be computed correctly.
		//!  (24 because each one of 8 vertices is used by 3 sides with different normal)
		//! \n RRDynamicSolver depends on vertex list defined here.
		//!  If you request vertex buffer with per-vertex irradiance,
		//!  vertex buffer will have getNumVertices() items.
		//!  So when writing new RRMesh implementations, create vertex list
		//!  so that vertex buffers with the same size and vertex order
		//!  are friendly for your renderer.
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
		struct TriangleBody  {Vertex vertex0,side1,side2;};
		//! Writes t-th triangle in mesh to out.
		//
		//! Make sure you provide valid t in range <0..getNumTriangles()-1>.
		//! Implementators are allowed to expect valid t, so result is completely undefined for invalid t (possible crash).
		//! \n There is default implementation, but if you know format of your data well, you may provide faster one.
		//! \n Speed of this function is important for intersection tests performance.
		virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;

		//! Plane in 3d space defined by its normal (in x,y,z) and w so that normal*point+w=0 for all points of plane.
		typedef RRVec3p Plane;
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

		//! Three normals for three vertices in triangle. In object space, normalized.
		struct TriangleNormals      {RRVec3 norm[3];};
		//! Writes to out vertex normalized normals of triangle.
		//
		//! Normals are used by global illumination solver and renderer.
		//! Normals should point to front side hemisphere, see \ref s5_frontback.
		//! \n Default implementation writes all vertex normals equal to triangle plane normal.
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param out Caller provided storage for result.
		//!  For valid t, requested normals are written to out. For invalid t, out stays unmodified.
		virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;

		//! Three uv-coords for three vertices in triangle.
		struct TriangleMapping      {RRVec2 uv[3];};
		//! Writes t-th triangle uv mapping for mesh unwrap into 0..1 x 0..1 space.
		//
		//! Unwrap may be used for calculated lightmaps/ambient maps.
		//! Note that for good results, all coordinates must be in 0..1 range and two triangles
		//! must not overlap in texture space. If it's not satisfied, results are undefined.
		//!
		//! \n Default implementation automatically generates unwrap of low quality.
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param out Caller provided storage for result.
		//!  For valid t, requested mapping is written to out. For invalid t, out stays unmodified.
		virtual void         getTriangleMapping(unsigned t, TriangleMapping& out) const;

		//! Returns axis aligned bounding box and center of mesh.
		//
		//! \param mini
		//!  NULL or pointer to vec3 to be filled with minimum of computed AABB.
		//! \param maxi
		//!  NULL or pointer to vec3 to be filled with maximum of computed AABB.
		//! \param center
		//!  NULL or pointer to vec3 to be filled with average vertex position.
		virtual void         getAABB(RRVec3* mini, RRVec3* maxi, RRVec3* center);


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
			//! Original PreImport index of element (vertex or triangle).
			//! For 32bit int: max 1M triangles/vertices in one mesh is supported by multimesh.
			unsigned index : sizeof(unsigned)*8-12;
			//! Index into array of original meshes used to create multimesh.
			//! For 32bit int: max 4k meshes is supported by multimesh.
			unsigned object : 12;
			MultiMeshPreImportNumber() {}
			MultiMeshPreImportNumber(unsigned aobject, unsigned aindex) {index=aindex;object=aobject;}
			//! Implicit unsigned -> MultiMeshPreImportNumber conversion.
			MultiMeshPreImportNumber(unsigned i) {
				//*(unsigned*)this = i; // not safe with strict aliasing
				index = i; object = i>>(sizeof(unsigned)*8-12);
				}
			//! Implicit MultiMeshPreImportNumber -> unsigned conversion.
			operator unsigned () {
				//return *(unsigned*)this; // not safe with strict aliasing
				return index + (object<<(sizeof(unsigned)*8-12));
				}
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

		//! Creates and returns partial copy of your instance (everything except custom channels).
		//
		//! Created copy is completely independent on any other objects and may be deleted sooner or later.
		//! \n It is expected that your input instance is well formed (returns correct and consistent values).
		//! \n Copy may be faster than original, but may require more memory.
		//! \n This function may fail and return NULL.
		RRMesh* createCopy();

		//! Creates and returns transformed mesh.
		//
		//! Created instance doesn't require additional memory, 
		//! but it depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//!
		//! Usually used when mesh in world space is needed and we have mesh in local space.
		//! In this case, world space matrix (the one that transforms from local to world) should be passed in transform.
		//!
		//! Only positions and normals are transformed, custom channels are left untouched.
		RRMesh* createTransformed(const RRMatrix3x4* transform);

		//! Creates and returns union of multiple meshes (contains vertices and triangles of all meshes).
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
		//! \param meshes
		//!  Array of meshes, source data for MultiMesh.
		//!  For now, all meshes must have the same data channels (see RRChanneledData).
		//! \param numMeshes
		//!  Length of 'meshes' array.
		static RRMesh* createMultiMesh(RRMesh* const* meshes, unsigned numMeshes);

		//! Creates and returns nearly identical mesh with optimized set of vertices (removes duplicates).
		//
		//! Created instance requires only small amount of additional memory, 
		//!  but it depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//! If 'this' is already optimal or vertexStitchMaxDistance is negative, 'this' is returned.
		//!
		//! Warning: Only vertex positions are tested for match, differences in normals etc are ignored.
		//! \param vertexStitchMaxDistance
		//!  For default 0, vertices with equal coordinates are stitched and get equal vertex index (number of vertices returned by getNumVertices() is then lower).
		//!  For negative value, no stitching is performed.
		//!  For positive value, also vertices in lower or equal distance will be stitched.
		RRMesh* createOptimizedVertices(float vertexStitchMaxDistance = 0);

		//! Creates and returns identical mesh with optimized set of triangles (removes degenerated triangles).
		//
		//! Created instance requires only small amount of additional memory, 
		//!  but it depends on 'this' mesh, 'this' must stay alive for whole life of created instance.
		//! If 'this' is already optimal, 'this' is returned.
		RRMesh* createOptimizedTriangles();

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
		RRMesh* createVertexBufferRuler();


		//! Verifies that mesh is well formed.
		//
		//! Reports any problems found using RRReporter.
		//! \return Number of problem reports sent, 0 for valid mesh.
		unsigned verify();
	};

} // namespace

#endif
