#ifndef RRILLUMINATION_H
#define RRILLUMINATION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumination.h
//! \brief LightsprintCore | storage for calculated illumination
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2013
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cstring> // NULL
#include "RRBuffer.h"

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
		RRObjectIllumination();
		~RRObjectIllumination();

		//
		// Vertex and pixel buffers, update and render supported for static objects.
		//

		//! Returns illumination buffer from given layer.
		//
		//! Illumination can be stored in various types of buffers
		//! - vertex colors (vertex buffer)
		//! - lightmap (2d texture)
		//! - reflection map (cube texture).
		//!
		//! At the beginning, all layers are empty, buffers are NULL. To initialize layer, do one of
		//! - do nothing (keep them NULL) for no illumination
		//! - allocate buffers automatically by allocateBuffersForRealtimeGI()
		//! - allocate buffers manually by getLayer() = RRBuffer::create().
		//!   For cubemap, reasonable initialization is RRBuffer::create(BT_CUBE_TEXTURE,16,16,6,BF_RGBA,true,NULL).
		//!   Note that BF_RGBF would preserves intensities above 1 better, but very old GPUs don't support float filtering
		//!   and GF7100 even switches to sw render (extremely slow).
		//!   BF_RGBA usually looks ok and it doesn't have any compatibility problems.
		//!   Size doesn't have to be power of two.
		//!
		//! To update buffers in layer, do one of
		//! - manually by updateLightmap() or updateLightmaps()
		//! - manually by updateEnvironmentMap()
		//! - automatically by calling renderer with updateLightIndirect=true
		//!
		//! Multiple layers (e.g. one layer per light source) can be mixed by renderer.
		//!
		//! Assigned buffers will be deleted automatically in ~RRObjectIllumination.
		//!
		//! \param layerNumber
		//!  Index of illumination buffer you would like to get. Arbitrary unsigned number.
		//! \return
		//!  Illumination buffer for given layerNumber. If it doesn't exist yet, it is created.
		RRBuffer*& getLayer(unsigned layerNumber);
		RRBuffer* getLayer(unsigned layerNumber) const;

		//! World coordinate of object center. To be updated by you when object moves.
		RRVec3 envMapWorldCenter;
		RRReal envMapWorldRadius;

	protected:
		//
		// for buffers
		//
		//! Container with all layers.
		class LayersMap* layersMap;

		//
		// for reflection maps
		//
		friend class RRDynamicSolver;
		RRVec3 cachedCenter;
		unsigned short cachedGatherSize;
		unsigned short cachedNumTriangles; // number of triangles in multiobject, invalidates cache when multiobject changes
		unsigned* cachedTriangleNumbers;
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
		//! \param envMapSize
		//!  Size of reflection envmaps. 8 or 16 is usually sufficient.
		//! \param numTimeSlots
		//!  Number of time slots in lightfield.
		//!  Time slots are used for dynamic lights, you can capture lighting in space for several
		//!  moments in time. Use 1 slot for static lighting.
		static RRLightField* create(RRVec4 aabbMin, RRVec4 aabbSize, RRReal spacing = 1, unsigned envMapSize = 8, unsigned numTimeSlots = 1);

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
		//! \param illumination
		//!  Illumination you want to update.
		//! \param layerEnvironment
		//!  Number of layer with environment map, it is addressed by illumination->getLayer(layerEnvironment).
		//! \param time
		//!  Illumination at given time is computed. (Lightfield may store dynamic lighting
		//!  for time period specified at lightfield build.)
		//!  Use the same time units you used in create().
		//! \return Number of maps updated, 0, 1 or 2.
		virtual unsigned updateEnvironmentMap(RRObjectIllumination* illumination, unsigned layerEnvironment, RRReal time) const = 0;
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
