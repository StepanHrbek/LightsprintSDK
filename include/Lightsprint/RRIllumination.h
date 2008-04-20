#ifndef RRILLUMINATION_H
#define RRILLUMINATION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumination.h
//! \brief LightsprintCore | storage for calculated illumination
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2008
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

} // namespace

#endif
