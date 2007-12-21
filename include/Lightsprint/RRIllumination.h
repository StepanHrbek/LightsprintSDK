#ifndef RRILLUMINATION_H
#define RRILLUMINATION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumination.h
//! \brief LightsprintCore | storage for calculated illumination
//! \author Copyright (C) Stepan Hrbek, Lightsprint
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
	//! Storage for object's indirect illumination.
	//
	//! Editor stores calculated illumination here.
	//! Renderer reads illumination from here.
	//! Add one instance to every object in your scene.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjectIllumination
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

		//! One layer of illumination (irradiance) values for whole object.
		//
		//! Illumination can be stored in vertex array, ambient map or both.
		//! Multiple layers (e.g. one layer per light source) can be mixed by renderer.
		struct Layer
		{
			//! Custom vertex array with illumination levels for all object vertices.
			//! May be NULL in which case vertex arrays are not generated and not used by render.
			RRBuffer* vertexBuffer;
			//! Custom ambient map with illumination levels for object surface.
			//! May be NULL in which case ambient maps are not generated and not used by render.
			RRBuffer* pixelBuffer;
			//! Constructs new layer, always empty.
			//! You can insert vertex array or ambient map or both later.
			Layer()
			{
				vertexBuffer = NULL;
				pixelBuffer = NULL;
			}
			//! Destructs layer, deleting remaining vertex array or ambient map.
			~Layer()
			{
				delete vertexBuffer;
				delete pixelBuffer;
			}
		};
		//! \param layerNumber
		//!  Index of layer you would like to get. Arbitrary unsigned number.
		//! \return Layer of layerNumber. If it doesn't exist yet, it is created.
		Layer* getLayer(unsigned layerNumber);
		const Layer* getLayer(unsigned layerNumber) const;
		//! \return PreImport number of vertices, length of vertex buffer for rendering.
		unsigned getNumPreImportVertices() const;

		//
		// Reflection maps, update and render supported for all objects (but used mostly by dynamic ones).
		//

		//! Diffuse reflection cube map. Created by you, updated by updateEnvironmentMap(), deleted automatically. May stay NULL.
		RRBuffer* diffuseEnvMap;
		//! Specular reflection cube map. Created by you, updated by updateEnvironmentMap(), deleted automatically. May stay NULL.
		RRBuffer* specularEnvMap;

		// parameters set by you and read by updateEnvironmentMap():

		//! Size of virtual cube for gathering samples, 16 by default. More = higher precision, slower.
		unsigned gatherEnvMapSize;
		//! Size of diffuse reflection map, 0 for none, 4 by default. More = higher precision, slower.
		unsigned diffuseEnvMapSize;
		//! Size of specular reflection map, 0 for none, 16 by default. More = higher precision, slower.
		unsigned specularEnvMapSize;
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
