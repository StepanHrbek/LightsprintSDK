#ifndef RROBJECT_H
#define RROBJECT_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRObject.h
//! \brief LightsprintCore | 3d object with geometry, materials, position etc
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRCollider.h"
#include "RRMaterial.h"
#include "RRIllumination.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObject
	//! Common interface for all proprietary object formats.
	//
	//! \section s1 Provided information
	//! %RRObject provides information about
	//! - object material properties
	//! - object collider for fast ray-mesh intersections
	//! - indirectly also object geometry (via getCollider()->getMesh())
	//! - optionally object transformation matrix
	//! - optionally unwrap (for future versions)
	//!
	//! \section s2 Creating instances
	//! The only way to create first instance is to derive from %RRObject.
	//! This is caused by lack of standard material description formats,
	//! everyone uses different description and needs his own object importer.
	//!
	//! Once you have object importers, there is built-in support for 
	//! - pretransforming mesh, see createWorldSpaceMesh()
	//! - baking multiple objects together, see createMultiObject()
	//!
	//! \section s3 Links between objects
	//! solver -> RRObject -> RRCollider -> RRMesh
	//! \n where A -> B means that
	//!  - A has pointer to B
	//!  - there is no automatic reference counting in B and no automatic destruction of B from A
	//! \n This means you may have multiple objects sharing one collider and mesh.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObject : public RRUniformlyAllocatedNonCopyable
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		RRObject();
		virtual ~RRObject();

		//! Specifies material assigned to a group of triangles.
		struct FaceGroup
		{
			RRMaterial* material; ///< Material assigned to all triangles in group. Must not be NULL.
			unsigned numTriangles; ///< Number of triangles in group.

			FaceGroup(RRMaterial* _material, unsigned _numTriangles)
			{
				material = _material;
				numTriangles = _numTriangles;
			}
		};
		//! Specifies materials assigned to all triangles.
		class RR_API FaceGroups : public RRVector<FaceGroup>
		{
		public:
			void getBlending(bool& containsBlended, bool& containsNonBlended) const;
			void getTexcoords(RRVector<unsigned>& texcoords, bool forUnwrap, bool forDiffuse, bool forSpecular, bool forEmissive, bool forTransparent) const;
		};
		//! Assignment of materials to triangles.
		//
		//! First facegroup describes first numTriangles in object, next facegroup next triangles etc.
		//! Results are undefined if faceGroups contains NULL materials or fewer triangles than mesh.
		FaceGroups faceGroups;

		//! Returns collider of underlying mesh. It is also access to mesh itself (via getCollider()->getMesh()).
		//! Must always return valid collider, implementation is not allowed to return NULL.
		virtual const RRCollider* getCollider() const = 0;

		//! Returns material description for given triangle.
		//
		//! Default implementation is just different way to access data stored in faceGroups.
		//! However, this function may provide additional information.
		//! If you wish to disable lighting or shadowing for specific light-caster-receiver combinations,
		//! reimplement this function to return NULL for that combination of parameters,
		//! fall back to default implementation for others combinations.
		//!
		//! Although more precise per-pixel material query is available in getPointMaterial(),
		//! this per-triangle version is often preferred for its speed and simplicity.
		//! Returned pointer must stay valid and constant for whole life of object.
		//!
		//! <b>Editing materials</b>
		//! \nCaller is allowed to modify returned materials including textures, but if he allocates new textures,
		//! he is responsible for freeing them. RRObject must free only what RRObject allocated.
		//! Filtered objects (e.g. objects created by createMultiObject(), createWorldSpaceObject()) usually
		//! share materials, so by modifying base object, filtered one is modified too.
		//! There is one notable exception - createObjectWithPhysicalMaterials() creates object with new independent
		//! set of RRMaterials, only textures inside materials are shared with original object.
		//! \param t
		//!  Triangle number.
		//! \param light
		//!  NULL or one of lights in scene.
		//!  With light==NULL, returned material must be always the same non-NULL.
		//!  With light!=NULL, it is allowed to return NULL to disable lighting or shadowing, see receiver for details.
		//! \param receiver
		//!  NULL or one of static objects in scene.
		//!  Used only when light!=NULL, controls properties of given light.
		//!  When receiver==NULL, you may return NULL to make triangle invisible for given light (disables both direct lighting and shadow-casting).
		//!  When receiver!=NULL, you may return NULL to disable direct shadow casting of triangle for given light and receiver.
		virtual RRMaterial* getTriangleMaterial(unsigned t, const class RRLight* light, const RRObject* receiver) const;

		//! Returns material description for point on object's surface.
		//
		//! Use it to query material properties for any given pixel.
		//! This is higher quality but slower per-pixel version of faster per-triangle getTriangleMaterial().
		//! \n\n Default implementation takes point details from optional textures in material returned by getTriangleMaterial().
		//! \n\n Offline GI solver uses getPointMaterial() only if requested lightmap quality>=getTriangleMaterial()->minimalQualityForPointMaterials.
		//! Realtime GI solvers never call getPointMaterial().
		//! \param t
		//!  Triangle number.
		//! \param uv
		//!  2D coordinates of point, in triangle's space. Triangle vertices are in 0,0 1,0 0,1.
		//! \param out
		//!  For default scaler=NULL, out is undefined on input, function fills it with material.
		//!  For special case of scaler!=NULL, see explanation in scaler.
		//! \param scaler
		//!  You are expected to keep it NULL, only solver calls it with scaler.
		//!  When called normally without scaler, function goal is to fill out in adapter's native(custom) scale.
		//!  When called with scaler, out is already filled with per-triangle material in physical scale
		//!  (colors in physical scale, textures are in adapter's default scale to save memory),
		//!  function's goal is to modify colors in physical scale. Default implementation
		//!  reads custom scale colors from textures and converts them to physical scale using scaler.
		virtual void getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out, const RRScaler* scaler = NULL) const;

		//! Information about single object, what LOD it is.
		struct LodInfo
		{
			//! Two objects are LODs of the same model only if they have identical bases.
			const void* base;
			//! LODs of the same model always differ in level. There must be always at least level 0 for each base.
			unsigned level;
			//! Object is rendered only if its distance satisfies distanceMin<=distance<distanceMax.
			RRReal distanceMin;
			//! Object is rendered only if its distance satisfies distanceMin<=distance<distanceMax.
			RRReal distanceMax;
		};
		//! Returns information about single object, what LOD it is.
		//
		//! In Lightsprint, LODs are completely separated objects without any pointers linking them.
		//! The only information that connects them comes from this function.
		//! \n Default implementation makes all objects unique, they return different bases and level 0.
		//! \param t
		//!  Triangle number, relevant only for multiobjects, individual triangles in multiobject may return
		//!  different results.
		//! \param out
		//!  Caller provided storage for result.
		//!  For valid t, requested LOD info is written to out. For invalid t, out stays unmodified.
		virtual void getTriangleLod(unsigned t, LodInfo& out) const;

		//! Returns object transformation from local to world space.
		//
		//! Default transformation is NULL, which means no transformation, identity.
		//! Transformation can be changed by setWorldMatrix().
		//! \return Pointer to matrix that transforms object space to world space.
		//!  May return NULL for identity/no transformation. 
		//!  Pointer must be constant and stay valid for whole life of object.
		//!  Matrix may change during object life.
		virtual const RRMatrix3x4*  getWorldMatrix();

		//! Sets object transformation from local to world space.
		//
		//! It copies data from your matrix rather than remembering your pointer.
		//! NULL is accepted as no transformation.
		//! If you set identity matrix, getWorldMatrix() will return NULL.
		virtual void setWorldMatrix(const RRMatrix3x4* worldMatrix);

		//! Returns arbitrary additional data provided by adapter, or NULL for unsupported data.
		//
		//! \param name
		//!  Identifier of custom data requested. It is good practise to use names 
		//!  that describe both type and semantic of returned data.
		//!  You are free to define and support any names in your adapters.
		//!  Usage example: <code>const char* objectName = (const char*)object->getCustomData("const char* objectName");</code>
		virtual void* getCustomData(const char* name) const;

		//! Illumination of this object.
		//
		//! Initialized to NULL, solver allocates it, ~RRObject deletes it.
		//! \n You can allocate illumination manually, solver allocates it only if it is NULL.
		RRObjectIllumination* illumination;

		//! Optional name of object.
		RRString name;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory

		//! Creates and returns RRMesh that describes mesh after transformation to world space.
		//
		//! Newly created instance allocates no additional memory, but depends on
		//! original object, so it is not allowed to let new instance live longer than original object.
		RRMesh* createWorldSpaceMesh();

		//! Creates and returns RRObject that describes object after transformation to world space.
		//
		//! Newly created instance has no transformation matrix, but it is still on the same 
		//! place in world space, because all vertices are transformed.
		//! \n Newly created instance allocates no additional memory, but depends on
		//! original object, so it is not allowed to let new instance live longer than the original object.
		//! \param negScaleMakesOuterInner
		//!  True = If you negatively scale singlesided box visible only from outside, it will become visible only from inside. This results in simpler code, recommended.
		//!  \n False = Makes negatively scaled objects visible from the same side.
		//!  \n\n Implementation details:
		//!  \n Both original and transformed object share the same mesh and materials, so both 
		//!  objects contain triangles with the same vertex order (e.g. ABC, 
		//!  not ACB) and materials visible for example from outside.
		//!  Negative scale naturally makes the object visible from inside
		//!  and rays collide with the inner side. This is the case of negScaleMakesOuterInner=true.
		//!  \n However one may want to change this behaviour. 
		//!  \n To get the transformed object visible from the opposite side and rays collide with the opposite side,
		//!  one can change the mesh (vertex order in all triangles) and share materials
		//!  or share the mesh and change materials.
		//!  It is more efficient to share the mesh and change materials.
		//!  So transformed object shares the mesh but when it detects negative scale,
		//!  it swaps sideBits[0] and sideBits[1] in all materials.
		//!  \n\n Note that shared RRMesh knows nothing about your local negScaleMakesOuterInner setting,
		//!  it is encoded in RRObject materials,
		//!  so if you calculate singlesided collision on mesh from newly created object,
		//!  give it a collision handler object->createCollisionHandlerFirstVisible()
		//!  which scans object's materials and responds to your local negScaleMakesOuterInner.
		//!  \n\n With negScaleMakesOuterInner=false, all materials sideBits[0] and [1] are swapped,
		//!  so where your system processes hits to front side on original object, it processes 
		//!  hits to back side on negatively scaled object.
		//!  Note that forced singlesided test (simple test without collision handler, see RRRay::TEST_SINGLESIDED) 
		//!  detects always front sides, so it won't work with negative scale and negScaleMakesOuterInner=false.
		//!  \n\nNote that if negScaleMakesOuterInner=false, worldSpaceObject->getTriangleMaterial() on negatively
		//!  scaled objects returns temporaries, editing them has no effect. If you wish to edit materials,
		//!  edit them in original object, it will immediately change them also in created world space object.
		//! \param intersectTechnique
		//!  Technique used for collider construction.
		//! \param aborting
		//!  May be set asynchronously, aborts creation.
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		//!  It is passed to RRCollider::create(), so
		//!  default NULL caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		RRObject* createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation);

		//! Creates and returns union of multiple objects (contains geometry and materials from all objects).
		//
		//! Created instance (MultiObject) doesn't require additional memory, 
		//! but it depends on all objects from array, they must stay alive for whole life of MultiObject.
		//! \n This can be used to accelerate calculations, as one big object is nearly always faster than multiple small objects.
		//! \n This can be used to simplify calculations, as processing one object may be simpler than processing array of objects.
		//! \n For array with 1 element, pointer to that element may be returned.
		//! \n\n For description how to access original triangles and vertices in MultiObject, 
		//!  see RRMesh::createMultiMesh(). Note that for non-negative vertexWeldDistance,
		//!  some vertices may be optimized out, so prefer PreImpport<->PostImport conversions.
		//! \param objects
		//!  Collection of objects you want to create multiobject from.
		//!  Objects from collection should stay alive for whole life of multiobjects (this is your responsibility).
		//!  Collection alone may be destructed immediately by you.
		//! \param intersectTechnique
		//!  Technique used for collider construction.
		//! \param aborting
		//!  May be set asynchronously, aborts creation.
		//! \param maxDistanceBetweenVerticesToStitch
		//!  Distance in world units. Vertices with lower or equal distance may be stitched into one vertex
		//!  (if they satisfy also maxRadiansBetweenNormalsToStitch).
		//!  Zero may stitch only identical vertices, negative value means no action.
		//! \param maxRadiansBetweenNormalsToStitch
		//!  Vertices with lower or equal angle between normals may be stitched into one vertex
		//!  (if they satisfy also maxDistanceBetweenVerticesToStitch).
		//!  Zero may stitch only identical normals, negative value means no action.
		//! \param optimizeTriangles
		//!  True removes degenerated triangles.
		//!  It is always good to get rid of degenerated triangles (true), but sometimes you know
		//!  there are no degenerated triangles at all and you can save few cycles by setting false.
		//! \param speed
		//!  Could make object faster, but needs additional memory.
		//!  \n 0 = normal speed, 0bytes/triangle overhead
		//!  \n 1 = +speed, 12bytes/triangle overhead
		//!  \n 2 = ++speed, 156bytes/triangle overhead
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		//!  It is passed to RRCollider::create(), so
		//!  default NULL caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		static RRObject* createMultiObject(const class RRObjects* objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, bool optimizeTriangles, unsigned speed, const char* cacheLocation);

		//! Creates and returns object with materials converted to physical space.
		//
		//! Created instance contains copy of all materials, converted and adjusted to fit in physical space.
		//! \n It is typically used to convert user provided objects in custom scale to physical scale.
		//! \n Created instance depends on original object and scaler, so it is not allowed to delete original object and scaler before deleting newly created instance.
		//! \param scaler
		//!  Scaler used for custom scale -> physical scale conversion.
		//!  Provide the same scaler you use for the rest of calculation.
		//! \param aborting
		//!  May be set asynchronously, aborts creation.
		class RRObject* createObjectWithPhysicalMaterials(const class RRScaler* scaler, bool& aborting);


		// other tools

		//! Creates and returns collision handler, that finds closest visible surface.
		//
		//! \n Finds closest surface with RRMaterial::sideBits::render.
		//! \n It is suitable e.g for picking objects in rendering window, only rendered pixels collide.
		//!
		//! Thread safe: this function yes, but created collision handler no.
		//! (typical use case: for n threads, use 1 collider, n rays and n handlers.)
		RRCollisionHandler* createCollisionHandlerFirstVisible() const;

		//! Generates position and direction suitable for automatically placed camera.
		//
		//! All parameters are filled by function.
		//! outMaxdist is upper bound for distance between two points in scene,
		//! it could be used for setting camera far.
		void generateRandomCamera(RRVec3& outPos, RRVec3& outDir, RRReal& outMaxdist) const;

		//! Returns hash of object data. Slow (not cached).
		//
		//! Hashing covers object properties that affect realtime global illumination:
		//! positions, normals, tangents, texcoords, material properties (even extracted from textures), transformation.
		//! Hashing doesn't cover: full texture data, names, uv indices.
		virtual RRHash getHash() const;

		//! Fills faceGroups according to results provided by getTriangleMaterial().
		//
		//! Fills faceGroups automatically, however, you may call it only if you implemented
		//! your own RRObject with your own self-contained getTriangleMaterial() not reading data from faceGroups.
		void updateFaceGroupsFromTriangleMaterials();
	private:
		RRMatrix3x4* worldMatrix;
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

	class RR_API RRObjects : public RRVector<RRObject*>
	{
	public:
		//! Structure used by recommendLayerParameters().
		struct LayerParameters
		{
			// inputs to RRObjects::recommendLayerParameters()
			int            objectIndex; ///< -1 = global params for all objects; 0,1,2... params specific for given object
			unsigned       suggestedMapSize;
			unsigned       suggestedMinMapSize;
			unsigned       suggestedMaxMapSize;
			float          suggestedPixelsPerWorldUnit;
			const char*    suggestedPath;
			const char*    suggestedExt;

			// outputs of RRObjects::recommendLayerParameters()
			RRBufferType   actualType;
			unsigned       actualWidth;
			unsigned       actualHeight;
			RRBufferFormat actualFormat;
			bool           actualScaled;
			char*          actualFilename; ///< NULL in constructor, malloced in RRObjects::recommendLayerParameters(), freed in destructor.
			bool           actualBuildNonDirectional; ///< not yet used outside Gamebryo
			bool           actualBuildDirectional; ///< not yet used outside Gamebryo
			bool           actualBuildBentNormals; ///< not yet used outside Gamebryo

			// tools
			LayerParameters()
			{
				objectIndex = -1;
				suggestedMapSize = 256;
				suggestedMinMapSize = 32;
				suggestedMaxMapSize = 1024;
				suggestedPixelsPerWorldUnit = 1;
				suggestedPath = "";
				suggestedExt = "png";
				actualFilename = NULL;
			}

			//! Creates buffer from actualXxx fields (filled by RRObjects::recommendLayerParameters())
			RRBuffer* createBuffer() const
			{
				return RRBuffer::create(actualType,actualWidth,actualHeight,1,actualFormat,actualScaled,NULL);
			}

			~LayerParameters()
			{
				free(actualFilename);
			}
		};

		//! Recommends layer parameters (resolution, filename etc).
		//
		//! Nearly all tools and samples need to decide what lightmap resolutions to use, what lightmap filenames
		//! to use etc.
		//! Samples are free to use arbitrary resolutions and filenames and often they do.
		//! But for convenience of undecided tools and samples, this function is provided as
		//! a central point of their decision making.
		//!
		//! Applications often have some opinion, so this function takes their suggections on resolution,
		//! path, extension etc, stored in layerParameters (suggestedXxx fields).
		//! Recommended new resolution, filename etc are output into the same structure (actualXxx fields).
		//!
		//! Custom implementations of this function use different decision making rules.
		//! For example adapter of Gamebryo .gsa scene never recommends vertex buffer,
		//! because Gamebryo doesn't support lighting in vertex buffers yet.
		//! \param layerParameters
		//!  Structure of both inputs (suggestedXxx) and outputs (actualXxx).
		//!  Outputs are filled by this function.
		virtual void recommendLayerParameters(RRObjects::LayerParameters& layerParameters) const;
		//! Loads illumination layer from disk.
		//
		//! It is shortcut for calling illumination->getLayer() = RRBuffer::load() on all elements in this container.
		//! \param layerNumber
		//!  Layer to load, nothing is done for negative number.
		//! \param path
		//!  Where to read files, should have trailing slash.
		//! \param ext
		//!  File format of maps to load, e.g. "png".
		//!  Vertex buffers are always loaded from .vbu, without regard to ext.
		//! \remark
		//!  rr_io::registerLoaders() must be called for image saves/loads to work.
		virtual unsigned loadLayer(int layerNumber, const char* path, const char* ext) const;

		//! Saves illumination layer to disk.
		//
		//! It is shortcut for calling illumination->getLayer()->save() on all elements in this container.
		//! \param layerNumber
		//!  Layer to save, nothing is done for negative number.
		//! \param path
		//!  Where to store files, should have trailing slash. Subdirectories are not created.
		//! \param ext
		//!  File format of maps to save, e.g. "png".
		//!  Vertex buffers are always saved to .vbu, without regard to ext.
		//! \remark
		//!  rr_io::registerLoaders() must be called for image saves/loads to work.
		virtual unsigned saveLayer(int layerNumber, const char* path, const char* ext) const;

		//! Destructor does not delete objects in collection (but individual adapters may do).
		virtual ~RRObjects() {};
	};

} // namespace

#endif
