#ifndef RROBJECT_H
#define RROBJECT_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRObject.h
//! \brief LightsprintCore | 3d object with geometry, materials, position etc
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2012
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
	//! - materials and their assignment to faces, see #faceGroups
	//! - transformation matrix, see getWorldMatrix() and setWorldMatrix()
	//! - collider for fast ray-mesh intersections, see getCollider() and setCollider()
	//! - indirectly also mesh (via getCollider()->getMesh())
	//! - LODs, see getTriangleLOD()
	//! - object illumination, see #illumination
	//! - object name, see #name
	//!
	//! \section s3 Links between objects
	//! solver and RRScene -> RRObjects -> RRObject -> RRCollider -> RRMesh
	//! \n where A -> B means that
	//!  - A has pointer to B
	//!  - there is no automatic reference counting in B and no automatic destruction of B from A
	//! \n This means that multiple objects may share one collider and mesh,
	//!    multiple scenes or solvers may share one object etc.
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

			FaceGroup()
			{
				material = NULL; // Material must not stay NULL, set it as soon as possible.
				numTriangles = 0;
			}
			FaceGroup(RRMaterial* _material, unsigned _numTriangles)
			{
				material = _material;
				numTriangles = _numTriangles;
			}
			bool operator ==(const FaceGroup& a) const
			{
				return material==a.material && numTriangles==a.numTriangles;
			}
			bool operator !=(const FaceGroup& a) const
			{
				return !(a==*this);
			}
		};
		//! Specifies materials assigned to all triangles.
		class RR_API FaceGroups : public RRVector<FaceGroup>
		{
		public:
			bool containsEmittance() const;
			void getTexcoords(RRVector<unsigned>& texcoords, bool forUnwrap, bool forDiffuse, bool forSpecular, bool forEmissive, bool forTransparent) const;
		};
		//! Assignment of materials to triangles.
		//
		//! First facegroup describes first numTriangles in object, next facegroup next triangles etc.
		//! Results are undefined if faceGroups contains NULL materials or fewer triangles than mesh.
		FaceGroups faceGroups;

		//! Returns collider of underlying mesh. It is also access to mesh itself (via getCollider()->getMesh()).
		//! Must always return valid collider, implementation is not allowed to return NULL.
		virtual RRCollider* getCollider() const {return collider;}
		//! Sets collider and mesh.
		//
		//! By changing collider, you change also mesh and possibly number of triangles,
		//! so make sure to update also faceGroups to have the same number of triangles as mesh.
		//! Object does not adopt/delete the collider, it keeps
		//! Caller must ensure collider stays alive until object destruction or next setCollider() call.
		virtual void setCollider(RRCollider* collider);

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
		//! \n Caller is allowed to modify returned materials including textures
		//! (textures are owned and deleted by material, so when changing texture, old one must be deleted).
		//! Filtered objects (e.g. objects created by createMultiObject()) usually
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
		//! \n\n Thread safe: yes, offline solver calls it from many thread concurrently.
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
		virtual void getPointMaterial(unsigned t, RRVec2 uv, RRPointMaterial& out, const RRScaler* scaler = NULL) const;

		//! Information about single object, its place in hierarchy of LODs.
		//
		//! Objects with the same #base represent the same entity, they are levels of detail (LOD) of the same entity.
		//! If they share #base but differ in #level, they are different LODs of the same entity
		//! and they don't shadow and illuminate each other. Only #level 0 shadows and illuminates other entities.
		//! If they share both #base and #level, they are the same representation of the same entity,
		//! i.e. they shadow and illuminate each other.
		struct LodInfo
		{
			//! Objects with the same #base represent the same entity, they are levels of detail (LOD) of the same entity.
			const void* base;
			//! Objects that share #base but differ in #level are different LODs of the same entity
			//! and they don't shadow and illuminate each other. Only level 0 shadows and illuminates other entities.
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
		//! Returns NULL for identity, for use in "if (matrix) slow_transform_path; else fast_identity_path;" scenarios.
		//! Transformation can be changed by setWorldMatrix().
		//! \return Pointer to matrix that transforms object space to world space.
		//!  May return NULL for identity/no transformation. 
		//!  Pointer must be constant and stay valid for whole life of object.
		//!  Matrix may change during object life.
		virtual const RRMatrix3x4* getWorldMatrix() const;
		//! Returns object transformation from local to world space.
		const RRMatrix3x4& getWorldMatrixRef() const;

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

		//! Illumination of the object.
		RRObjectIllumination illumination;

		//! Optional name of the object.
		RRString name;

		//! Is this object dynamic = is it safe to modify it?
		//
		//! This flag does not enforce anything, you set it as you wish and you comply with it if you wish.
		//! Object does not become static by clearing this flag, it is static if you treat it as static,
		//! e.g. by passing it to setStaticObjects() and not moving it.
		//! But it's practical to have this bit of information here, it is saved to .rr3 files,
		//! scene viewer reads it from .rr3 files etc.
		bool isDynamic;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory

		//! Creates and returns RRMesh that describes mesh after transformation to world space.
		//
		//! Newly created instance allocates no additional memory, but depends on
		//! original object, so it is not allowed to let new instance live longer than original object.
		RRMesh* createWorldSpaceMesh() const;

		//! Creates and returns union of multiple objects (contains geometry and materials from all objects), in world space.
		//
		//! Created instance (MultiObject) doesn't require additional memory, 
		//! but it depends on all objects from array, they must stay alive for whole life of MultiObject.
		//! \n This can be used to accelerate calculations, as one big object is nearly always faster than multiple small objects.
		//! \n This can be used to simplify calculations, as processing one object may be simpler than processing array of objects.
		//! \n\n For description how to access original triangles and vertices in MultiObject, 
		//!  see RRMesh::createMultiMesh().
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
		//!  \n Vertices are stitched even if uv coordinates differ. Therefore stitchig is safe for offline calculation,
		//!  but it could break mapping in relatime renderer.
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
		//!  If it is NULL, 'this' is returned.
		//! \param aborting
		//!  May be set asynchronously, aborts creation.
		class RRObject* createObjectWithPhysicalMaterials(const class RRScaler* scaler, bool& aborting);


		// other tools

		//! Reports inconsistencies found in object. Returns number of problem reported.
		//! \param objectNumber Arbitrary string that is inserted into error messages, to identify object.
		unsigned checkConsistency(const char* objectNumber) const;

		//! Creates and returns collision handler, that finds closest visible surface.
		//
		//! \n Finds closest surface with RRMaterial::sideBits::render and specularTransmittance.color!=1.
		//! \n It is suitable e.g for picking objects in rendering window, only visible pixels collide.
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

		//! Structure used by recommendLayerParameters().
		struct RR_API LayerParameters
		{
			// inputs to RRObject::recommendLayerParameters()
			int            objectIndex; ///< default implementation uses it only when formatting recommended filename
			unsigned       suggestedMapWidth;
			unsigned       suggestedMapHeight;
			unsigned       suggestedMinMapSize;
			unsigned       suggestedMaxMapSize;
			float          suggestedPixelsPerWorldUnit;
			RRString       suggestedPath; ///< If not set, default directory is used.
			RRString       suggestedName; ///< If not set, 5-digit object number is used.
			RRString       suggestedExt; ///< If not set, "png" is used for textures, "rrbuffer" for vertex buffers.

			// outputs of RRObject::recommendLayerParameters()
			RRBufferType   actualType;
			unsigned       actualWidth;
			unsigned       actualHeight;
			RRBufferFormat actualFormat;
			bool           actualScaled;
			RRString       actualFilename; ///< Set to suggestedPath + suggestedName + suggestedExt.
			bool           actualBuildNonDirectional; ///< not yet used outside Gamebryo
			bool           actualBuildDirectional; ///< not yet used outside Gamebryo
			bool           actualBuildBentNormals; ///< not yet used outside Gamebryo

			// tools
			LayerParameters()
			{
				objectIndex = -1;
				suggestedMapWidth = 256;
				suggestedMapHeight = 256;
				suggestedMinMapSize = 32;
				suggestedMaxMapSize = 1024;
				suggestedPixelsPerWorldUnit = 1;
			}

			//! Creates buffer from actualXxx fields (filled by RRObject::recommendLayerParameters()).
			//
			//! If DXT format is recommended, RGB or RGBA is created instead, because solver can't store directly to DXT.
			//! User may change format later by buffer->setFormat(actualFormat).
			//! \param forceFloats
			//!  Creates float buffer.
			//! \param forceAlpha
			//!  Creates buffer with alpha channel.
			//! \param insertBeforeExtension
			//!  Buffer's filename is set to actualFilename with this string inserted before extension. May be NULL.
			RRBuffer* createBuffer(bool forceFloats = false, bool forceAlpha = false, const wchar_t* insertBeforeExtension = NULL) const;
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
		virtual void recommendLayerParameters(RRObject::LayerParameters& layerParameters) const;

	private:
		RRCollider* collider;
		RRMatrix3x4* worldMatrix;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObjects
	//! Set of objects with interface similar to std::vector.
	//
	//! GI solver uses this class to set all static or dynamic objects at once.
	//! You can adapt content from memory or load content from files to RRObjects, see scene adapters in LightsprintIO library;
	//! or you can fill RRObjects instance manually, using push_back(object).
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjects : public RRVector<RRObject*>
	{
	public:
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
		virtual unsigned loadLayer(int layerNumber, const RRString& path, const RRString& ext) const;

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
		virtual unsigned saveLayer(int layerNumber, const RRString& path, const RRString& ext) const;

		//! Allocates buffers for realtime GI illumination.
		//
		//! This is helper function, RRDynamicSolver::allocateBuffersForRealtimeGI() calls it for
		//! objects in solver. You can call it directly for finer control over buffers allocated,
		//! but most likely you don't need to.
		//! \param layerLightmap
		//!  If >=0, vertex buffers in illumination->getLayer(layerLightmap) are allocated, resized or deleted according to other parameters.
		//!  Pass <0 if you don't want to touch vertex buffers.
		//!  Vertex buffers are suitable (=we can realtime update them) only for static objects.
		//! \param layerEnvironment
		//!  If >=0, cubemaps in illumination->getLayer(layerEnvironment) are allocated, resized or deleted according to other parameters.
		//!  Pass <0 if you don't want to touch cubemaps.
		//!  Cubemaps are suitable (=we can realtime update them) for both static and dynamic objects.
		//! \param diffuseEnvMapSize
		//!  If materials have diffuse reflection, reflection map of at least this size will be allocated in illumination.
		//!  Size 4 is usually good enough.
		//! \param specularEnvMapSize
		//!  If materials have specular reflection, reflection map of at least this size will be allocated in illumination.
		//!  Size 16 is usually sufficient, not very sharp, but makes rendering fast.
		//! \param allocateNewBuffers
		//!  If buffer does not exist yet, true = it will be allocated, false = no action.
		//! \param changeExistingBuffers
		//!  If buffer already exists, true = it will be resized or deleted accordingly, false = no action.
		//! \param specularThreshold
		//!  Only objects with specular color above threshold apply for specular cube reflection, 0=all objects apply, 1=only objects with spec color 1 apply.
		//! \param depthThreshold
		//!  Only objects with depth above threshold apply for specular cube reflection, 0=all objects apply, 0.1=all but near planar objects apply, 1=none apply.
		//! \return
		//!  Number of buffers allocated or reallocated.
		virtual unsigned allocateBuffersForRealtimeGI(int layerLightmap, int layerEnvironment, unsigned diffuseEnvMapSize, unsigned specularEnvMapSize, bool allocateNewBuffers, bool changeExistingBuffers, float specularThreshold, float depthThreshold) const;

		//! Reports inconsistencies found in objects.
		//
		//! \param objectType
		//!  Optional identifier of collection, e.g "static", "dynamic". May be NULL.
		//! \return Number of problem reported.
		unsigned checkConsistency(const char* objectType) const;

		//! Rebuilds unwrap in all meshes.
		//
		//! New unwrap is created into the lowest unused uv channel, the same channel in all meshes.
		//! All materials are updated to use new unwrap.
		//!
		//! Unwrapper doesn't modify pointers to objects, colliders, materials or meshes,
		//! but it can add new vertices in mesh, if unwrap contains seams.
		//! Therefore structures that depend on exact number of vertices may need update after unwrapping.
		//! This is case of solver; if you build unwrap in solver <code>solver->getStaticObjects().buildUnwrap(...)</code>,
		//! you have to resend modified objects to solver <code>solver->setStaticObjects(solver->getStaticObjects(),...)</code>.
		//! \param resolution
		//!  Expected lightmap resolution, e.g. 1024 for 1024x1024.
		//! \param aborting
		//!  May be set asynchronously, aborts build.
		//! \return Number of new unwrap uv channel, it's the same for all meshes. UINT_MAX in case of failure.
		virtual unsigned buildUnwrap(unsigned resolution, bool& aborting) const;

		//! Inserts all materials found in objects into collection.
		//
		//! May shuffle elements that were already present in collection.
		//! Removes duplicates, the same material is never listed twice.
		void getAllMaterials(RRVector<RRMaterial*>& materials) const;

		//! Flips front/back if at least this number of normals in triangle points to back side.
		//
		//! So all triangles are flipped if numNormalsThatMustPointBack==0.
		//! \return Number of triangles flipped.
		virtual unsigned flipFrontBack(unsigned numNormalsThatMustPointBack, bool report) const;

		//! Rebuilds objects to make them smooth (possibly changing numbers of triangles, vertices, facegroups).
		//
		//! If there are multiple objects sharing one mesh, all objects must be smoothed at once
		//! (because smoothing can remove triangles and object facegroups must reflect that).
		//!
		//! Smoothing doesn't modify pointers to objects, colliders, materials or meshes,
		//! but it can add/remove triangles/vertices in mesh, depending on parameters.
		//! Therefore structures that depend on exact number of triangles/vertices may need update after smoothing.
		//! This is case of solver; if you smooth in solver <code>solver->getStaticObjects().smoothAndStitch(...)</code>,
		//! you have to resend modified objects to solver <code>solver->setStaticObjects(solver->getStaticObjects(),...)</code>.
		//! \param splitVertices
		//!  Allows splitting vertices to make mesh less smooth.
		//! \param stitchVertices
		//!  Allows stitching vertices to make mesh more smooth.
		//! \param removeDegeneratedTriangles
		//!  Removes degenerated triangles (already present or created by stitching).
		//!  Note that number of triangles may drop to zero (while number of vertices may still be positive),
		//!  you can use removeEmptyObjects() to remove such objects.
		//! \param generateNormals
		//!  True = resets all vertex normals to averages of normals of connected faces.
		//!  False = old normals stay.
		//! \param maxDistanceBetweenVerticesToStitch
		//!  When stitchVertices==true, controls maximal distance between vertices to stitch and smooth.
		//! \param maxRadiansBetweenNormalsToStitch
		//!  When stitchVertices==true, controls maximal angle between face normals to stitch and smooth.
		//! \param maxDistanceBetweenUvsToStitch
		//!  When stitchVertices==true, controls maximal distance between uvs to stitch and smooth.
		//! \param report
		//!  Allows reporting warnings.
		virtual void smoothAndStitch(bool splitVertices, bool stitchVertices, bool removeDegeneratedTriangles, bool generateNormals, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, float maxDistanceBetweenUvsToStitch, bool report) const;

		//! Multiplies emittance in all materials, both colors and textures.
		virtual void multiplyEmittance(float emissiveMultiplier) const;

		//! Deletes selected object components: tangents, unwrap, uv channels not referenced by materials.
		void deleteComponents(bool deleteTangents, bool deleteUnwrap, bool deleteUnusedUvChannels, bool deleteEmptyFacegroups) const;

		//! Removes objects with zero triangles or vertices. Does not delete them.
		void removeEmptyObjects();

		//! Destructor does not delete objects in collection (but individual adapters may do).
		virtual ~RRObjects() {};
	};

} // namespace

#endif
