// --------------------------------------------------------------------------
// Collection of objects.
// Copyright (c) 2006-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <vector>
#include "Lightsprint/RRObject.h"
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRObjects

unsigned RRObjects::allocateBuffersForRealtimeGI(int _layerLightmap, int _layerEnvironment, unsigned _diffuseEnvMapSize, unsigned _specularEnvMapSize, bool _allocateNewBuffers, bool _changeExistingBuffers, float _specularThreshold, float _depthThreshold) const
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

			// process vertex buffers for LIGHT_INDIRECT_VCOLOR
			if (_layerLightmap>=0)
			{
				RRBuffer*& buffer = illumination.getLayer(_layerLightmap);
				if (!numVertices)
					RR_SAFE_DELETE(buffer)
				else
				if (!buffer && _allocateNewBuffers)
				{
					buffer = RRBuffer::create(BT_VERTEX_BUFFER,numVertices,1,1,BF_RGBF,false,NULL);
					buffersTouched++;
				}
				else
				if (buffer && _changeExistingBuffers)
				{
					buffer->reset(BT_VERTEX_BUFFER,numVertices,1,1,BF_RGBF,false,NULL);
					buffersTouched++;
				}
			}

			// process cube maps for LIGHT_INDIRECT_CUBE
			if (_layerEnvironment>=0)
			{
				// calculate desired cube size for LIGHT_INDIRECT_CUBE
				RRBuffer*& buffer = illumination.getLayer(_layerEnvironment);
				unsigned currentEnvMapSize = buffer ? buffer->getWidth() : 0;
				unsigned desiredDiffuseSize = currentEnvMapSize;
				unsigned desiredSpecularSize = currentEnvMapSize;

				// calculate diffuse size
				if (!numVertices)
				{
					desiredDiffuseSize = 0;
				}
				else
				if ((!currentEnvMapSize && _allocateNewBuffers) || (currentEnvMapSize && _changeExistingBuffers))
				{
					desiredDiffuseSize = _diffuseEnvMapSize ? RR_MAX(_diffuseEnvMapSize,8) : 0;
				}

				// calculate specular size
				if (!numVertices || (_changeExistingBuffers && !_specularEnvMapSize))
				{
					desiredSpecularSize = 0;
				}
				else
				if ((!currentEnvMapSize && _allocateNewBuffers) || (currentEnvMapSize && _changeExistingBuffers))
				{
					// measure object's specularity
					//float maxDiffuse = 0;
					float maxSpecular = 0;
					for (unsigned g=0;g<object->faceGroups.size();g++)
					{
						const RRMaterial* material = object->faceGroups[g].material;
						if (material)
						{
							//maxDiffuse = RR_MAX(maxDiffuse,material->diffuseReflectance.color.avg());
							maxSpecular = RR_MAX(maxSpecular,material->specularReflectance.color.avg());
						}
					}
					// continue only for highly specular objects
					if (maxSpecular>RR_MAX(0.01f,_specularThreshold))
					{
						// measure object's size
						RRVec3 mini,maxi;
						mesh->getAABB(&mini,&maxi,NULL);
						RRVec3 size = maxi-mini;
						float sizeMidi = size.sum()-size.maxi()-size.mini();
						// continue only for non-planar objects, cubical reflection looks bad on plane
						// (size is in object's space, so this is not precise for non-uniform scale)
						if (_depthThreshold<1 && size.mini()>=_depthThreshold*sizeMidi) // depthThreshold=0 allows everything, depthThreshold=1 nothing
						{
							// allocate specular cube map
							RRVec3 center;
							mesh->getAABB(NULL,NULL,&center);
							const RRMatrix3x4* matrix = object->getWorldMatrix();
							if (matrix) matrix->transformPosition(center);
							illumination.envMapWorldCenter = center;
							desiredSpecularSize = _specularEnvMapSize;
							//updateEnvironmentMapCache(illumination);
						}
						else
						{
							if (_changeExistingBuffers)
								desiredSpecularSize = 0;
						}
					}
					else
					{
						if (_changeExistingBuffers)
							desiredSpecularSize = 0;
					}
				}

				// allocate cube for LIGHT_INDIRECT_CUBE
				unsigned desiredEnvMapSize = RR_MAX(desiredDiffuseSize,desiredSpecularSize);
				if (desiredEnvMapSize!=currentEnvMapSize)
				{
					if (!currentEnvMapSize)
						buffer = RRBuffer::create(BT_CUBE_TEXTURE,desiredEnvMapSize,desiredEnvMapSize,6,BF_RGBA,true,NULL);
					else if (desiredEnvMapSize)
					{
						buffer->reset(BT_CUBE_TEXTURE,desiredEnvMapSize,desiredEnvMapSize,6,BF_RGBA,true,NULL);
						buffer->version = rand()*11111; // updateEnvironmentMap() considers version++ from reset() too small change to update, upper 16bits must change
					}
					else
						RR_SAFE_DELETE(buffer);
					buffersTouched++;
				}
			}
		}
	}
	return buffersTouched;
}

unsigned RRObjects::flipFrontBack(unsigned numNormalsThatMustPointBack, bool report) const
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

void RRObjects::smoothAndStitch(bool splitVertices, bool stitchVertices, bool removeDegeneratedTriangles, bool smoothNormals, float maxDistanceBetweenVerticesToSmooth, float maxRadiansBetweenNormalsToSmooth, float maxDistanceBetweenUvsToSmooth, bool report) const
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
		RRReporter::report(WARN,"smoothAndStitch() supports only RRMeshArrays meshes.\n");

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
	unsigned numMeshesDifferentScales = 0;
	unsigned numMeshesNonUniformScale = 0;
	for (boost::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		RRMeshArrays* mesh = *i;
		bool tangents = mesh->tangent!=NULL;

		// create temporary list of objects with this mesh
		std::vector<RRObject*> objects;
		for (unsigned j=0;j<size();j++)
			if ((*this)[j]->getCollider()->getMesh()==*i)
				objects.push_back((*this)[j]);

		// convert stitch distance from world to local
		RRReal maxLocalDistanceBetweenVerticesToSmooth = maxDistanceBetweenVerticesToSmooth / (objects[0]->getWorldMatrixRef().getScale().abs().avg()+1e-10f);
		// gather data for warnings
		{
			bool differentScales = false;
			bool nonUniformScale = false;
			RRVec3 scale = objects[0]->getWorldMatrixRef().getScale().abs();
			for (unsigned j=0;j<objects.size();j++)
			{
				RRVec3 scaleJ = objects[j]->getWorldMatrixRef().getScale().abs();
				differentScales |= scaleJ!=scale;
				nonUniformScale |= abs(scaleJ.x-scaleJ.y)+abs(scaleJ.x-scaleJ.z)>0.1f*scaleJ.avg(); // warn only if error gets over 10% of stitch distance
			}
			if (differentScales) numMeshesDifferentScales++;
			if (nonUniformScale) numMeshesNonUniformScale++;
		}

		// create temporary meshes that don't depend on original
		RRVector<unsigned> texcoords;
		mesh->getUvChannels(texcoords);
		RRMeshArrays* mesh1 = mesh->createArrays(!splitVertices,texcoords,tangents);
		mesh1->buildNormals(); // createOptimizedVertices() uses vertex normals, we want it to use triangle normals
		const RRMesh* mesh2 = stitchVertices ? mesh1->createOptimizedVertices(maxLocalDistanceBetweenVerticesToSmooth,maxRadiansBetweenNormalsToSmooth,maxDistanceBetweenUvsToSmooth,&texcoords) : mesh1; // stitch less, preserve uvs
		const RRMesh* mesh3 = removeDegeneratedTriangles ? mesh2->createOptimizedTriangles() : mesh2;

		// smooth when stitching
		if (smoothNormals && stitchVertices)
		{
			// calc smooth normals from mesh1, write them back to mesh1
			const RRMesh* mesh4 = mesh1->createOptimizedVertices(maxLocalDistanceBetweenVerticesToSmooth,maxRadiansBetweenNormalsToSmooth,0,NULL); // stitch more, even if uvs differ
			RRMeshArrays* mesh5 = mesh4->createArrays(true,texcoords,tangents);
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
			for (unsigned j=0;j<objects.size();j++)
			{
				RRObject::FaceGroups faceGroups;
				unsigned mesh3_numTriangles = mesh3->getNumTriangles();
				for (unsigned postImportTriangle=0;postImportTriangle<mesh3_numTriangles;postImportTriangle++)
				{
					unsigned preImportTriangle = mesh3->getPreImportTriangle(postImportTriangle).index;
					RRMaterial* m = objects[j]->getTriangleMaterial(preImportTriangle,NULL,NULL);
					if (!faceGroups.size() || faceGroups[faceGroups.size()-1].material!=m)
					{
						faceGroups.push_back(RRObject::FaceGroup(m,1));
					}
					else
					{
						faceGroups[faceGroups.size()-1].numTriangles++;
					}
				}
				objects[j]->faceGroups = faceGroups;
			}
		}

		// overwrite original mesh with temporary
		(*i)->reload(mesh3,true,texcoords,tangents);

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
	{
		RRReporter::report(INF2,"Smoothing stats: tris %+d(%d), verts %+d(%d), fgs %+d(%d)\n",numTriangles2-numTriangles,numTriangles2,numVertices2-numVertices,numVertices2,numFaceGroups2-numFaceGroups,numFaceGroups2);
		if (stitchVertices && maxDistanceBetweenVerticesToSmooth>0)
		{
			if (numMeshesDifferentScales)
				RRReporter::report(WARN,"Stitching distance approximated in %d/%d meshes: instances have different scale.\n",numMeshesDifferentScales,arrays.size());
			if (numMeshesNonUniformScale)
				RRReporter::report(WARN,"Stitching distance approximated in %d/%d meshes: non-uniform scale.\n",numMeshesNonUniformScale,arrays.size());
		}
	}
}

void RRObjects::multiplyEmittance(float emissiveMultiplier) const
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

void RRObjects::deleteComponents(bool deleteTangents, bool deleteUnwrap, bool deleteUnusedUvChannels, bool deleteEmptyFacegroups) const
{
	// empty facegroups must go first, otherwise some unused uv channels might seem in use
	if (deleteEmptyFacegroups)
	for (unsigned i=0;i<size();i++)
	{
		RRObject::FaceGroups& fg = (*this)[i]->faceGroups;
		for (unsigned i=fg.size();i--;)
		{
			if (!fg[i].numTriangles)
				fg.erase(i);
		}
	}

	// 1. gather unique meshes and used uv channels
	typedef boost::unordered_set<unsigned> UvChannels;
	typedef boost::unordered_map<RRMeshArrays*,UvChannels> Meshes;
	Meshes meshes;
	const RRObjects& objects = *this;
	for (unsigned i=0;i<objects.size();i++)
	{
		const RRObject* object = objects[i];
		if (object)
		{
			const RRCollider* collider = object->getCollider();
			if (collider)
			{
				const RRMesh* mesh = collider->getMesh();
				if (mesh)
				{
					// editing mesh without rebuilding collider is safe because we won't touch triangles,
					// we will only add/remove uv channels and split vertices
					RRMeshArrays* arrays = dynamic_cast<RRMeshArrays*>(const_cast<RRMesh*>(mesh));
					if (arrays)
					{
						// inserts arrays. without this, meshes without texcoords would not be inserted and unwrap not built
						meshes[arrays].begin();

						if (deleteUnusedUvChannels)
						{
							for (unsigned fg=0;fg<object->faceGroups.size();fg++)
							{
								RRMaterial* material = object->faceGroups[fg].material;
								if (material)
								{
									// we must insert only existing channels, unwrapper expects valid inputs
									#define INSERT(prop) if (material->prop<arrays->texcoord.size() && arrays->texcoord[material->prop]) meshes[arrays].insert(material->prop);
									#define INSERT_PROP(prop) if (material->prop.texture) INSERT(prop.texcoord)
									INSERT_PROP(diffuseReflectance);
									INSERT_PROP(specularReflectance);
									INSERT_PROP(diffuseEmittance);
									INSERT_PROP(specularTransmittance);
									if (!deleteUnwrap) INSERT(lightmapTexcoord);
									#undef INSERT
								}
							}
						}
						else
						{
							for (unsigned channel=0;channel<arrays->texcoord.size();channel++)
							{
								if (arrays->texcoord[channel])
									meshes[arrays].insert(channel);
							}
						}
					}
					else 
					{
						RRReporter::report(WARN,"Texcoords not removed, scene contains non-RRMeshArrays meshes. This affects 3ds, gamebryo, mgf, try changing format.\n");
						return;
					}
				}
			}
		}
	}

	// 2. change all lightmapTexcoord to unwrapChannel
	if (deleteUnwrap)
		for (unsigned i=0;i<objects.size();i++)
			for (unsigned fg=0;fg<objects[i]->faceGroups.size();fg++)
			{
				RRMaterial* material = objects[i]->faceGroups[fg].material;
				if (material)
					material->lightmapTexcoord = UINT_MAX;
			}

	// 3. delete
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		for (unsigned j=0;j<i->first->texcoord.size();j++)
			if (i->second.find(j)==i->second.end())
				i->first->texcoord[j] = NULL;
	}

	// tangents
	if (deleteTangents)
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		i->first->tangent = NULL;
		i->first->bitangent = NULL;
	}
}

} // namespace
