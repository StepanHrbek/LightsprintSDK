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
							if (size.mini()>0.1*sizeMidi)
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
	if (report && numMeshesWithoutArrays)
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

void RRObjects::stitchAndSmooth(bool splitVertices, bool stitchVertices, bool removeDegeneratedTriangles, bool smoothNormals, float maxDistanceBetweenVerticesToSmooth, float maxRadiansBetweenNormalsToSmooth, bool report) const
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
	if (report && numMeshesWithoutArrays)
		RRReporter::report(WARN,"smoothNormals() supports only RRMeshArrays meshes.\n");

	// stats
	unsigned numTriangles = 0;
	unsigned numVertices = 0;
	unsigned numFaceGroups = 0;
	for (boost::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		numTriangles += (*i)->numTriangles;
		numVertices += (*i)->numVertices;
	}
	for (unsigned i=0;i<size();i++)
		numFaceGroups += (*this)[i]->faceGroups.size();

	// stitch
	for (boost::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		RRMeshArrays* mesh = *i;

		// create temporary meshes that don't depend on original
		RRVector<unsigned> texcoords;
		mesh->getUvChannels(texcoords);
		RRMeshArrays* mesh1 = mesh->createArrays(!splitVertices,texcoords);
		mesh1->buildNormals(); // createOptimizedVertices() uses vertex normals, we want it to use triangle normals
		const RRMesh* mesh2 = stitchVertices ? mesh1->createOptimizedVertices(maxDistanceBetweenVerticesToSmooth,maxRadiansBetweenNormalsToSmooth,&texcoords) : mesh1; // stitch less, preserve uvs
		const RRMesh* mesh3 = removeDegeneratedTriangles ? mesh2->createOptimizedTriangles() : mesh2;

		// smooth when stitching
		if (smoothNormals && stitchVertices)
		{
			// calc smooth normals from mesh1, write them back to mesh1
			const RRMesh* mesh4 = mesh1->createOptimizedVertices(maxDistanceBetweenVerticesToSmooth,maxRadiansBetweenNormalsToSmooth,NULL); // stitch more, even if uvs differ
			RRMeshArrays* mesh5 = mesh4->createArrays(true,texcoords);
			mesh5->buildNormals();
			for (unsigned t=0;t<mesh1->numTriangles;t++)
			{
				mesh1->normal[mesh1->triangle[t][0]] = mesh5->normal[mesh5->triangle[t][0]];
				mesh1->normal[mesh1->triangle[t][1]] = mesh5->normal[mesh5->triangle[t][1]];
				mesh1->normal[mesh1->triangle[t][2]] = mesh5->normal[mesh5->triangle[t][2]];
			}
			if (mesh5!=mesh4) delete mesh5;
			if (mesh4!=mesh1) delete mesh4;
		}

		// fix facegroups in objects
		if (removeDegeneratedTriangles) // facegroups should be unchanged if we did not remove triangles
		{
			for (unsigned j=0;j<size();j++)
			{
				RRObject* object = (*this)[j];
				if (object->getCollider()->getMesh()==*i)
				{
					RRObject::FaceGroups faceGroups;
					for (unsigned postImportTriangle=0;postImportTriangle<mesh->numTriangles;postImportTriangle++)
					{
						unsigned preImportTriangle = mesh3->getPreImportTriangle(postImportTriangle).index;
						RRMaterial* m = object->getTriangleMaterial(preImportTriangle,NULL,NULL);
						if (!faceGroups.size() || faceGroups[faceGroups.size()-1].material!=m)
						{
							faceGroups.push_back(RRObject::FaceGroup(m,1));
						}
						else
						{
							faceGroups[faceGroups.size()-1].numTriangles++;
						}
					}
					object->faceGroups = faceGroups;
				}
			}
		}

		// overwrite original mesh with temporary
		(*i)->reload(mesh3,true,texcoords);

		// smooth when not stitching
		if (smoothNormals && !stitchVertices)
			(*i)->buildNormals();

		// delete temporary meshes
		if (mesh3!=mesh2) delete mesh3;
		if (mesh2!=mesh1) delete mesh2;
		if (mesh1!=mesh) delete mesh1;
	}

	// stats
	unsigned numTriangles2 = 0;
	unsigned numVertices2 = 0;
	unsigned numFaceGroups2 = 0;
	for (boost::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		numTriangles2 += (*i)->numTriangles;
		numVertices2 += (*i)->numVertices;
	}
	for (unsigned i=0;i<size();i++)
		numFaceGroups2 += (*this)[i]->faceGroups.size();
	if (report)
		RRReporter::report(INF2,"Smoothing stats: tris %+d(%d), verts %+d(%d), fgs %+d(%d)\n",numTriangles2-numTriangles,numTriangles2,numVertices2-numVertices,numVertices2,numFaceGroups2-numFaceGroups,numFaceGroups2);
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
