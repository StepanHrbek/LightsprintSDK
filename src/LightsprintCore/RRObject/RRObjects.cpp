// --------------------------------------------------------------------------
// Collection of objects.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include "Lightsprint/RRObject.h"

namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRObjects

static bool exists(const char* filename)
{
	FILE* f = fopen(filename,"rb");
	if (!f) return false;
	fclose(f);
	return true;
}


unsigned RRObjects::loadLayer(int layerNumber, const char* path, const char* ext) const
{
	unsigned result = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObject* object = (*this)[objectIndex];
			// first try to load per-pixel format
			RRBuffer* buffer = NULL;
			RRObject::LayerParameters layerParameters;
			layerParameters.objectIndex = objectIndex;
			layerParameters.suggestedPath = path;
			layerParameters.suggestedExt = ext;
			layerParameters.suggestedMapSize = 256;
			object->recommendLayerParameters(layerParameters);
			if ( !exists(layerParameters.actualFilename) || !(buffer=RRBuffer::load(layerParameters.actualFilename,NULL)) )
			{
				// if it fails, try to load per-vertex format
				layerParameters.suggestedMapSize = 0;
				object->recommendLayerParameters(layerParameters);
				if (exists(layerParameters.actualFilename))
					buffer = RRBuffer::load(layerParameters.actualFilename);
			}
			if (buffer && buffer->getType()==BT_VERTEX_BUFFER && buffer->getWidth()!=object->getCollider()->getMesh()->getNumVertices())
			{
				RR_LIMITED_TIMES(5,RRReporter::report(ERRO,"%s has wrong size.\n",layerParameters.actualFilename));
				RR_SAFE_DELETE(buffer);
			}
			if (buffer)
			{
				delete object->illumination.getLayer(layerNumber);
				object->illumination.getLayer(layerNumber) = buffer;
				result++;
				RRReporter::report(INF3,"Loaded %s.\n",layerParameters.actualFilename);
			}
			else
			{
				RRReporter::report(INF3,"Not loaded %s.\n",layerParameters.actualFilename);
			}
		}
		RRReporter::report(INF2,"Loaded layer %d, %d/%d buffers into %s.\n",layerNumber,result,size(),path);
	}
	return result;
}

unsigned RRObjects::saveLayer(int layerNumber, const char* path, const char* ext) const
{
	unsigned result = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObject* object = (*this)[objectIndex];
			RRBuffer* buffer = object->illumination.getLayer(layerNumber);
			if (buffer)
			{
				RRObject::LayerParameters layerParameters;
				layerParameters.objectIndex = objectIndex;
				layerParameters.suggestedPath = path;
				layerParameters.suggestedExt = ext;
				layerParameters.suggestedMapSize = (buffer->getType()==BT_VERTEX_BUFFER) ? 0 : 256;
				object->recommendLayerParameters(layerParameters);
				if (buffer->save(layerParameters.actualFilename))
				{
					result++;
					RRReporter::report(INF3,"Saved %s.\n",layerParameters.actualFilename);
				}
				else
				if (layerParameters.actualFilename)
					RRReporter::report(WARN,"Not saved %s.\n",layerParameters.actualFilename);
			}
		}
		if (result)
			RRReporter::report(INF2,"Saved layer %d, %d/%d buffers into %s.\n",layerNumber,result,size(),path);
		else
			RRReporter::report(WARN,"Failed to save layer %d (%d buffers) into %s.\n",layerNumber,size(),path);
	}
	return result;
}

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

} // namespace
