#ifndef RRILLUMINATION_H
#define RRILLUMINATION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumination.h
//! \brief LightsprintCore | storage for calculated illumination
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cstring> // NULL
#include "RRBuffer.h"

#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Data structure with object's illumination and more.
	//
	//! This is collection of various buffers that may contain
	//! lightmaps, bent normals, reflection cube maps, ambient occlusion.
	//! This collection belongs to one object in your scene.
	//!
	//! Buffers are created by you and deleted by destructor (unless you delete them manually).
	//!
	//! RRDynamicSolver::updateXxx() functions store calculated illumination in buffers.
	//! They usually preserve buffer properties you selected while creating buffer:
	//! BufferType, BufferFormat and scale. So you are free to select between per-pixel
	//! and per-vertex lighting etc.
	//!
	//! Renderer renders illumination from these buffers.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjectIllumination: public RRUniformlyAllocatedNonCopyable
	{
	public:
		//! \param numPreImportVertices
		//!  PreImport (original) number of mesh vertices, length of vertex buffer for rendering.
		//!  You may set it 0 for dynamic objects, they don't use vertex buffers so they don't need this information.
		RRObjectIllumination(unsigned numPreImportVertices);
		~RRObjectIllumination();

		//
		// Vertex and pixel buffers, update and render supported for static objects.
		//

		//! Returns layer of illumination (irradiance) values for whole object.
		//
		//! Illumination can be stored in vertex color buffer or lightmap (2d).
		//! Multiple layers (e.g. one layer per light source) can be mixed by renderer.
		//! At the beginning, all layers are NULL. You are free to create them
		//! (getLayer() = RRBuffer::create()), they will be deleted automatically
		//! in ~RRObjectIllumination.
		//! \param layerNumber
		//!  Index of illumination layer you would like to get. Arbitrary unsigned number.
		//! \return Illumination layer for given layerNumber. If it doesn't exist yet, it is created.
		RRBuffer*& getLayer(unsigned layerNumber);
		RRBuffer* getLayer(unsigned layerNumber) const;
		//! \return PreImport number of vertices, length of vertex buffer for rendering.
		unsigned getNumPreImportVertices() const;

		//
		// Reflection maps, update and render supported for all objects (but used mostly by dynamic ones).
		//

		//! Diffuse reflection cube map.
		//
		//! Created by you, updated by updateEnvironmentMap(), deleted automatically. May stay NULL.
		//! Size doesn't have to be power of two.
		//! Reasonable initialization is RRBuffer::create(BT_CUBE_TEXTURE,4,4,6,BF_RGBF,true,NULL).
		RRBuffer* diffuseEnvMap;
		//! Specular reflection cube map.
		//
		//! Created by you, updated by updateEnvironmentMap(), deleted automatically. May stay NULL.
		//! Size doesn't have to be power of two.
		//! Reasonable initialization is RRBuffer::create(BT_CUBE_TEXTURE,16,16,6,BF_RGBF,true,NULL).
		RRBuffer* specularEnvMap;

		// parameters set by you and read by updateEnvironmentMap():

		//! Size of virtual cube for gathering samples, 16 by default. More = higher precision, slower.
		unsigned gatherEnvMapSize;
		//! World coordinate of object center. To be updated by you when object moves.
		RRVec3 envMapWorldCenter;

	protected:
		//
		// for buffers
		//
		//! PreImport (original) number of mesh vertices, length of vertex buffer for rendering.
		unsigned numPreImportVertices;
		//! Container with all layers.
		void* hiddenLayers;

		//
		// for reflection maps
		//
		friend class RRDynamicSolver;
		RRVec3 cachedCenter;
		unsigned cachedGatherSize;
		unsigned* cachedTriangleNumbers;
		class RRRay* ray6;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! GI precomputed in space
	//
	//! Light field contains precomputed indirect part of global illumination.
	//! It is designed for scenarios with static lights and dynamic objects -
	//! precompute lightmaps for static objects and lightfield for dynamic objects.
	//! When rendering dynamic objects in game, use your usual
	//! direct lighting together with indirect lighting from lightfield.
	//! You can use multiple lightfields for different locations in game level.
	//!
	//! Typical use:
	//! - RRDynamicSolver::buildLightField() - create new instance in game editor
	//! - RRLightField::save() - save it in game editor
	//! - RRLightField::load() - load instance in game
	//! - RRLightField::updateEnvironmentMap() - use instance in game
	class RR_API RRLightField : public RRUniformlyAllocatedNonCopyable
	{
	public:

		//! Creates light field.
		//
		//! Empty lightfield is created, use captureLighting() to fill it.
		//! \param aabbMin
		//!  Lower corner of axis aligned bounding box of space/time where light field is to be computed.
		//!  Fourth coordinate is time in your units.
		//! \param aabbSize
		//!  Size of axis aligned bounding box of space/time where light field is to be computed.
		//!  Fourth coordinate is time in your units.
		//! \param spacing
		//!  Distance between sampling points. Smaller = higher quality, but larger structure.
		//! \param diffuseSize
		//!  Size of cubemaps for diffuse reflection.
		//!  4 is usually sufficient.
		//!  Diffuse part is usually more important for indirect lighting of dynamic objects,
		//!  however, you can set size 0 to build light field without diffuse part
		//! \param specularSize
		//!  Size of cubemaps for specular reflection.
		//!  8 is usually sufficient.
		//!  Specular part is usually less important for indirect lighting of dynamic objects,
		//!  and it takes more memory (4x more by default),
		//!  so you set size 0 to build light field without specular part.
		//! \param numTimeSlots
		//!  Number of time slots in lightfield.
		//!  Time slots are used for dynamic lights, you can capture lighting in space for several
		//!  moments in time. Use 1 slot for static lighting.
		static RRLightField* create(RRVec4 aabbMin, RRVec4 aabbSize, RRReal spacing = 1, unsigned diffuseSize = 4, unsigned specularSize = 8, unsigned numTimeSlots = 1);

		//! Captures lighting in space into lightfield.
		//! Call it for all time slots, otherwise content of lightfield will be undefined.
		//! \param solver
		//!  Solver, source of information about scene and its illumination.
		//! \param timeSlot
		//!  Lighting in scene will be stored to given slot.
		//!  Time slot numbers are 0..numTimeSlots-1
		//!  where numTimeSlots was specified by create().
		virtual void captureLighting(class RRDynamicSolver* solver, unsigned timeSlot) = 0;

		//! Updates diffuse and/or specular environment maps in object illumination.
		//
		//! Unlike all other buffer update function in Lightsprint SDK, this one
		//! changes type/size/format of buffer (to RGB cubemap of size you entered
		//! to RRDynamicSolver::buildLightField()).
		//! \param  objectIllumination
		//!  Illumination you want to update.
		//! \param time
		//!  Illumination at given time is computed. (Lightfield may store dynamic lighting
		//!  for time period specified at lightfield build.)
		//!  Use the same time units you used in create().
		//! \return Number of maps updated, 0, 1 or 2.
		virtual unsigned updateEnvironmentMap(RRObjectIllumination* objectIllumination, RRReal time) const = 0;
		virtual ~RRLightField() {}

		//! Saves instance to disk.
		//
		//! \param filename File to be created. Mandatory, it is not autogenerated.
		//! \return True on success.
		virtual bool save(const char* filename) const {return false;}
		//! Loads instance from disk.
		//
		//! \param filename File to be loaded. Mandatory, it is not autogenerated.
		//! \return Created instance or NULL at failure.
		static RRLightField* load(const char* filename);
	};

} // namespace

#endif
