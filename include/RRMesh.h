#ifndef RRMESH_H
#define RRMESH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMesh.h
//! \brief RRMesh - unified mesh interface
//! \version 2006.8
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRMath.h"
#include <climits> // UNDEFINED

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#if defined(RR_RELEASE) || (defined(NDEBUG) && !defined(RR_DEBUG))
			#pragma comment(lib,"RRMesh_s.lib")
		#else
			#pragma comment(lib,"RRMesh_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_MESH
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#pragma comment(lib,"RRMesh.lib")
#	endif
#	endif
#endif

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

	class RRChanneledData
	{
	public:
		//! Writes size of selected channel into numItems and itemSize.
		//
		//! \param channelId Id of channel, eg. RRMesh::CHANNEL_VERTEX_POS - channel holding vertex positions.
		//!  Each class that implements this method (eg. RRMesh, RRObject) defines supported channel ids
		//!  CHANNEL_xxx.
		//! \param numItems When not NULL, it is filled with number of items. Items are indexed by 0..numItems-1.
		//!  Zero is filled for unknown channel.
		//! \param itemSize When not NULL, it is filled with size of one item in bytes.
		//!  Item type (and thus size) for each channel is part of each CHANNEL_xxx description.
		//!  Zero is filled for unknown channel.
		virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
		//! Copies one data item from selected channel into buffer provided by you.
		//
		//! \param channelId Id of channel, eg. RRMesh::CHANNEL_VERTEX_POS - channel holding vertex positions.
		//!  Each class that implements this method (eg. RRMesh, RRObject) defines supported channel ids
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
	//! interface to your mesh, but it uses your mesh data, no data are duplicated.
	//!
	//! \section s4 Creating instances
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
	//! you may easily derive from %RRMesh and create your own importer.
	//!
	//! \section s6 Optimizations
	//!
	//! %RRMesh may help you with mesh optimizations if requested,
	//! for example by removing duplicate vertices or degenerated triangles.
	//! 
	//! \section s6 Constancy
	//!
	//! All data provided by %RRMesh must be constant in time.
	//! Built-in importers guarantee constancy if you don't change
	//! their vertex/index buffers. Constancy of mesh copy is guaranteed always.
	//!
	//! Thread safe: yes, stateless, may be accessed by any number of threads simultaneously.
	//!
	//! \section s5 Indexing
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
			INDEXED_BY_SURFACE               = 0x2000,
			INDEXED_BY_OBJECT                = 0x3000,
			// If we decide later that some information from RRMesh
			// should be provided via new RRChanneledData interface,
			// these channel numbers will be used.
			//CHANNEL_VERTEX_POS               = INDEXED_BY_VERTEX+0, //! RRVec3
			//CHANNEL_TRIANGLE_VERTICES_IDX    = INDEXED_BY_TRIANGLE+0, //! unsigned[3]
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
			MultiMeshPreImportNumber(unsigned i) {
				//*(unsigned*)this = i; // not safe with gcc 3.4.5
				index = i; object = i>>(sizeof(unsigned)*8-12);
				} ///< Implicit unsigned -> MultiMeshPreImportNumber conversion.
			operator unsigned () {
				//return *(unsigned*)this; // not safe with gcc 3.4.5
				return index + (object<<(sizeof(unsigned)*8-12));
				} ///< Implicit MultiMeshPreImportNumber -> unsigned conversion.
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
		//! Creates and returns copy of your instance.
		//
		//! Created copy is completely independent on any other objects and may be deleted sooner or later.
		//! \n It is expected that your input instance is well formed (returns correct and consistent values).
		//! \n Copy may be faster than original, but may require more memory.
		RRMesh* createCopy();
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
		static RRMesh* createMultiMesh(RRMesh* const* meshes, unsigned numMeshes);
		//! Creates and returns nearly identical mesh with optimized set of vertices (removes duplicates).
		//
		//! \param vertexStitchMaxDistance
		//!  For default 0, vertices with equal coordinates are stitched and get equal vertex index (number of vertices returned by getNumVertices() is then lower).
		//!  For negative value, no stitching is performed.
		//!  For positive value, also vertices in lower or equal distance will be stitched.
		RRMesh* createOptimizedVertices(float vertexStitchMaxDistance = 0);
		//! Creates and returns identical mesh with optimized set of triangles (removes degenerated triangles).
		RRMesh* createOptimizedTriangles();
		//! Saves mesh to file.
		//
		//! \param filename
		//!  Filename for saved mesh. Format is platform specific (eg. "c:\\mymesh" or "/pub/mymesh").
		//! \return True on successful save.
		bool save(char* filename);
		//! Loads mesh from file to newly created instance.
		//
		//! Loaded mesh represents the same geometry as saved mesh, but internal implementation (speed, 
		//! occupied memory) may differ.
		//! \param filename
		//!  Filename of mesh to load.
		//! \return Newly created instance or NULL when load failed.
		static RRMesh* load(char* filename);

		// verification
		//! Callback for reporting text messages.
		typedef void Reporter(const char* msg, void* context);
		//! Verifies that mesh is well formed.
		//
		//! \param reporter All inconsistencies are reported using this callback.
		//! \param context This value is sent to reporter without any modifications from verify.
		//! \returns Number of reports performed.
		unsigned verify(Reporter* reporter, void* context);
	};

} // namespace

#endif
