// --------------------------------------------------------------------------
// Collection of objects.
// Copyright (c) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include "Lightsprint/RRObject.h"
#include <boost/unordered_set.hpp>

namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRObjects

unsigned RRObjects::allocateBuffersForRealtimeGI(int lightmapLayerNumber, unsigned diffuseEnvMapSize, unsigned specularEnvMapSize, int gatherEnvMapSize, bool allocateNewBuffers, bool changeExistingBuffers) const
{
	unsigned buffersTouched = 0;
	for (unsigned i=0;i<size();i++)
	{
		RRObject* object = (*this)[i];
		if (object)
		{
			RRObjectIllumination& illumination = object->illumination;
			const RRMesh* mesh = object->getCollider()->getMesh();
			unsigned numVertices = mesh->getNumVertices();
			if (numVertices)
			{
				// allocate vertex buffers for LIGHT_INDIRECT_VCOLOR
				// (this should be called also if lightmapLayerNumber changes... for now it never changes during solver existence)
				if (lightmapLayerNumber>=0)
				{
					RRBuffer*& buffer = illumination.getLayer(lightmapLayerNumber);
					if (!buffer && allocateNewBuffers)
					{
						buffer = RRBuffer::create(BT_VERTEX_BUFFER,numVertices,1,1,BF_RGBF,false,NULL);
						buffersTouched++;
					}
					else
					if (buffer && changeExistingBuffers)
					{
						buffer->reset(BT_VERTEX_BUFFER,numVertices,1,1,BF_RGBF,false,NULL);
						buffersTouched++;
					}
				}
				// allocate diffuse cubes for LIGHT_INDIRECT_CUBE_DIFFUSE
				if (diffuseEnvMapSize)
				{
					if (!illumination.diffuseEnvMap && allocateNewBuffers)
					{
						illumination.diffuseEnvMap = RRBuffer::create(BT_CUBE_TEXTURE,diffuseEnvMapSize,diffuseEnvMapSize,6,BF_RGBA,true,NULL);
						buffersTouched++;
					}
					else
					if (illumination.diffuseEnvMap && changeExistingBuffers)
					{
						illumination.diffuseEnvMap->reset(BT_CUBE_TEXTURE,diffuseEnvMapSize,diffuseEnvMapSize,6,BF_RGBA,true,NULL);
						illumination.diffuseEnvMap->version = rand()*11111; // updateEnvironmentMap() considers version++ from reset() too small change to update, upper 16bits must change
						buffersTouched++;
					}
				}
				else
				{
					if (changeExistingBuffers)
						RR_SAFE_DELETE(illumination.diffuseEnvMap);
				}
				// allocate specular cubes for LIGHT_INDIRECT_CUBE_SPECULAR
				if (specularEnvMapSize)
				{
					if ((!illumination.specularEnvMap && allocateNewBuffers) || (illumination.specularEnvMap && changeExistingBuffers))
					{
						// measure object's specularity
						float maxDiffuse = 0;
						float maxSpecular = 0;
						for (unsigned g=0;g<object->faceGroups.size();g++)
						{
							const RRMaterial* material = object->faceGroups[g].material;
							if (material)
							{
								maxDiffuse = RR_MAX(maxDiffuse,material->diffuseReflectance.color.avg());
								maxSpecular = RR_MAX(maxSpecular,material->specularReflectance.color.avg());
							}
						}
						// continue only for highly specular objects
						if (maxSpecular>RR_MAX(0.01f,maxDiffuse*0.5f))
						{
							// measure object's size
							RRVec3 mini,maxi;
							mesh->getAABB(&mini,&maxi,NULL);
							RRVec3 size = maxi-mini;
							float sizeMidi = size.sum()-size.maxi()-size.mini();
							// continue only for non-planar objects, cubical reflection looks bad on plane
							// (size is in object's space, so this is not precise for non-uniform scale)
							if (size.mini()>0.3*sizeMidi)
							{
								// allocate specular cube map
								RRVec3 center;
								mesh->getAABB(NULL,NULL,&center);
								const RRMatrix3x4* matrix = object->getWorldMatrix();
								if (matrix) matrix->transformPosition(center);
								illumination.envMapWorldCenter = center;
								if (!illumination.specularEnvMap && allocateNewBuffers)
								{
									illumination.specularEnvMap = RRBuffer::create(BT_CUBE_TEXTURE,specularEnvMapSize,specularEnvMapSize,6,BF_RGBA,true,NULL);
									buffersTouched++;
								}
								else
								if (illumination.specularEnvMap && changeExistingBuffers)
								{
									illumination.specularEnvMap->reset(BT_CUBE_TEXTURE,specularEnvMapSize,specularEnvMapSize,6,BF_RGBA,true,NULL);
									illumination.specularEnvMap->version = rand()*11111; // updateEnvironmentMap() considers version++ from reset() too small change to update, upper 16bits must change
									buffersTouched++;
								}
								//updateEnvironmentMapCache(illumination);
							}
							else
							{
								if (changeExistingBuffers)
									RR_SAFE_DELETE(illumination.specularEnvMap);
							}
						}
						else
						{
							if (changeExistingBuffers)
								RR_SAFE_DELETE(illumination.specularEnvMap);
						}
					}
				}
				// change gatherEnvMapSize
				if (gatherEnvMapSize>=0)
					illumination.gatherEnvMapSize = gatherEnvMapSize;
			}
		}
	}
	return buffersTouched;
}

unsigned RRObjects::flipFrontBack(unsigned numNormalsThatMustPointBack, bool report)
{
	// gather unique meshes (only mesharrays, basic mesh does not have API for editing)
	unsigned numMeshesWithoutArrays = 0;
	boost::unordered_set<RRMeshArrays*> arrays;
	for (unsigned i=0;i<size();i++)
	{
		RRMeshArrays* mesh = const_cast<RRMeshArrays*>(dynamic_cast<const RRMeshArrays*>((*this)[i]->getCollider()->getMesh()));
		if (mesh)
			arrays.insert(mesh);
		else
			numMeshesWithoutArrays++;
	}
	// warn when working with non-arrays
	if (numMeshesWithoutArrays)
		RRReporter::report(WARN,"flipFrontBack() supports only RRMeshArrays meshes.\n");
	// flip
	unsigned numFlips = 0;
	for (boost::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		numFlips += (*i)->flipFrontBack(numNormalsThatMustPointBack);
	}
	if (report && numFlips)
	{
		RRReporter::report(WARN,"flipFrontBack(): %d faces flipped.\n",numFlips);
	}
	return numFlips;
}

void RRObjects::multiplyEmittance(float emissiveMultiplier)
{
	if (emissiveMultiplier==1)
		return;
	// gather unique materials
	boost::unordered_set<RRMaterial*> materials;
	for (unsigned i=0;i<size();i++)
	{
		RRObject* object = (*this)[i];
		if (object)
			for (unsigned fg=0;fg<object->faceGroups.size();fg++)
				materials.insert(object->faceGroups[fg].material);
	}
	// multiply
	for (boost::unordered_set<RRMaterial*>::iterator i=materials.begin();i!=materials.end();++i)
	{
		if (*i)
		{
			(*i)->diffuseEmittance.color *= emissiveMultiplier;
			if ((*i)->diffuseEmittance.texture)
				(*i)->diffuseEmittance.texture->multiplyAdd(RRVec4(emissiveMultiplier),RRVec4(0));
		}
	}
}

} // namespace
