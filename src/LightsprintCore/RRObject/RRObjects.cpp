// --------------------------------------------------------------------------
// Collection of objects.
// Copyright (c) 2006-2013 Stepan Hrbek, Lightsprint. All rights reserved.
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

unsigned RRObjects::allocateBuffersForRealtimeGI(int _layerLightmap, int _layerEnvironment, unsigned _diffuseEnvMapSize, unsigned _specularEnvMapSize, unsigned _refractEnvMapSize, bool _allocateNewBuffers, bool _changeExistingBuffers, float _specularThreshold, float _depthThreshold) const
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

			// process cube maps for LIGHT_INDIRECT_ENV_
			if (_layerEnvironment>=0)
			{
				object->updateIlluminationEnvMapCenter();

				// calculate desired cube size for LIGHT_INDIRECT_ENV_
				RRBuffer*& buffer = illumination.getLayer(_layerEnvironment);
				unsigned currentEnvMapSize = buffer ? buffer->getWidth() : 0;
				unsigned desiredDiffuseSize = currentEnvMapSize;
				unsigned desiredSpecularSize = currentEnvMapSize;
				unsigned desiredRefractSize = currentEnvMapSize;

				// is it suitable for mirror rather than cube?
				bool useMirror;
				{
					RRVec3 mini,maxi;
					mesh->getAABB(&mini,&maxi,NULL);
					RRVec3 size = maxi-mini;
					float sizeMidi = size.sum()-size.maxi()-size.mini();
					// continue only for non-planar objects, cubical reflection looks bad on plane
					// (size is in object's space, so this is not precise for non-uniform scale)
					useMirror = !(_depthThreshold<1 && size.mini()>=_depthThreshold*sizeMidi); // depthThreshold=0 allows everything, depthThreshold=1 nothing
				}

				// calculate diffuse size
				if (!numVertices || useMirror || (_changeExistingBuffers && !_diffuseEnvMapSize))
				{
					desiredDiffuseSize = 0;
				}
				else
				if ((!currentEnvMapSize && _allocateNewBuffers) || (currentEnvMapSize && _changeExistingBuffers))
				{
					desiredDiffuseSize = _diffuseEnvMapSize ? RR_MAX(_diffuseEnvMapSize,8) : 0;
				}

				// calculate specular size
				if (!numVertices || useMirror || (_changeExistingBuffers && !_specularEnvMapSize))
				{
					desiredSpecularSize = 0;
				}
				else
				if ((!currentEnvMapSize && _allocateNewBuffers) || (currentEnvMapSize && _changeExistingBuffers))
				{
					// measure object's specularity
					float maxSpecular = 0;
					for (unsigned g=0;g<object->faceGroups.size();g++)
					{
						const RRMaterial* material = object->faceGroups[g].material;
						if (material)
						{
							maxSpecular = RR_MAX(maxSpecular,material->specularReflectance.color.avg());
						}
					}
					// continue only for highly specular objects
					if (maxSpecular>RR_MAX(0,_specularThreshold))
					{
						// allocate specular cube map
						desiredSpecularSize = _specularEnvMapSize;
					}
					else
					{
						if (_changeExistingBuffers)
							desiredSpecularSize = 0;
					}
				}

				// calculate refract size
				if (!numVertices || useMirror || (_changeExistingBuffers && !_refractEnvMapSize))
				{
					desiredRefractSize = 0;
				}
				else
				if ((!currentEnvMapSize && _allocateNewBuffers) || (currentEnvMapSize && _changeExistingBuffers))
				{
					// test whether refraction is possible
					for (unsigned g=0;g<object->faceGroups.size();g++)
					{
						const RRMaterial* material = object->faceGroups[g].material;
						if (material)
						{
							if (material->needsBlending())
							{
								// refraction is possible (although not necessary, we don't know if user renders with LIGHT_INDIRECT_ENV_REFRACT or not. but then he should call us with _refractEnvMapSize 0)
								desiredRefractSize = _refractEnvMapSize;
								goto refraction_done;
							}
						}
					}
					// refraction is not possible
					if (_changeExistingBuffers)
						desiredRefractSize = 0;
					refraction_done:;
				}

				// allocate cube for LIGHT_INDIRECT_ENV_
				unsigned desiredEnvMapSize = RR_MAX3(desiredDiffuseSize,desiredSpecularSize,desiredRefractSize);
				if (desiredEnvMapSize!=currentEnvMapSize)
				{
					if (!currentEnvMapSize)
						buffer = RRBuffer::create(BT_CUBE_TEXTURE,desiredEnvMapSize,desiredEnvMapSize,6,BF_RGBA,true,NULL);
					else if (desiredEnvMapSize)
					{
						buffer->reset(BT_CUBE_TEXTURE,desiredEnvMapSize,desiredEnvMapSize,6,BF_RGBA,true,NULL);
						buffer->filename.clear(); // contents was destroyed, buffer is no longer related to file on disk
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

unsigned RRObjects::optimizeFaceGroups(RRObject* object) const
{
	unsigned result = 0;
	if (!object)
	{
		for (unsigned i=0;i<size();i++)
		{
			result += optimizeFaceGroups((*this)[i]);
		}
	}
	else
	{
		const RRMeshArrays* mesh = dynamic_cast<const RRMeshArrays*>(object->getCollider()->getMesh());
		if (mesh)
		{
			unsigned numOtherInstances = 0;
			for (unsigned i=0;i<size();i++)
				if ((*this)[i]!=object && (*this)[i]->getCollider()->getMesh()==mesh)
					numOtherInstances++;
			if (!numOtherInstances) // our limitation, we don't support fixing multiple instances at once
			{
				if (object->faceGroups.size()>1) // optimization, trivial objects are optimal already
				{
					// alloc tmp
					RRMesh::Triangle* tmpTriangles = new RRMesh::Triangle[mesh->numTriangles];
					RRObject::FaceGroups tmpFaceGroups;
					// fill tmp (by reordered data from object)
					unsigned dstTriangleIndex = 0;
					for (unsigned f=0;f<object->faceGroups.size();f++)
					{
						unsigned srcTriangleIndex = 0;
						for (unsigned g=0;g<object->faceGroups.size();g++)
						{
							if (object->faceGroups[f].material==object->faceGroups[g].material)
							{
								if (g<f)
									break; // facegroup f is already in tmp
								if (g==f)
									tmpFaceGroups.push_back(RRObject::FaceGroup(object->faceGroups[f].material,0));
								else
									result = 1; // f!=g, order of data is changing
								memcpy(tmpTriangles+dstTriangleIndex,mesh->triangle+srcTriangleIndex,object->faceGroups[g].numTriangles*sizeof(RRMesh::Triangle));
								dstTriangleIndex += object->faceGroups[g].numTriangles;
								tmpFaceGroups[tmpFaceGroups.size()-1].numTriangles += object->faceGroups[g].numTriangles;
							}
							srcTriangleIndex += object->faceGroups[g].numTriangles;
						}
					}
					// copy tmp to object
					memcpy(mesh->triangle,tmpTriangles,mesh->numTriangles*sizeof(RRMesh::Triangle));
					object->faceGroups = tmpFaceGroups;
					// delete tmp
					delete[] tmpTriangles;
				}
			}
		}
	}
	return result;
}

void RRObjects::smoothAndStitch(bool splitVertices, bool mergeVertices, bool removeDegeneratedTriangles, bool generateNormals, float maxDistanceBetweenVerticesToSmooth, float maxRadiansBetweenNormalsToSmooth, float maxDistanceBetweenUvsToSmooth, bool report) const
{
	bool stitchPositions = false;
	bool stitchNormals = generateNormals && mergeVertices;

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

		// create temporary mesh that doesn't depend on original
		RRVector<unsigned> texcoords;
		mesh->getUvChannels(texcoords);
		RRMeshArrays* mesh1 = mesh->createArrays(!splitVertices,texcoords,tangents);

		// stitch positions (even if normal or uv differs too much, therefore merge is not an option. and even if we don't merge at all)
		if (stitchPositions)
		{
			// calc smooth positions from mesh1, write them back to mesh1
			const RRMesh* mesh4 = mesh1->createOptimizedVertices(maxLocalDistanceBetweenVerticesToSmooth,100,0,NULL); // stitch more, even if normals or uvs differ
			RRMeshArrays* mesh5 = mesh4->createArrays(true,texcoords,tangents);
			for (unsigned t=0;t<mesh1->numTriangles;t++)
			{
				mesh1->position[mesh1->triangle[t][0]] = mesh5->position[mesh5->triangle[t][0]];
				mesh1->position[mesh1->triangle[t][1]] = mesh5->position[mesh5->triangle[t][1]];
				mesh1->position[mesh1->triangle[t][2]] = mesh5->position[mesh5->triangle[t][2]];
			}
			if (mesh5!=mesh4) delete mesh5;
			if (mesh4!=mesh1) delete mesh4;
			// these are final positions, they should not change later, even merge should merge only what was stitched here
			// (however, when merging but not stitching, positions are not final yet)
		}

		// generate normals (1)
		//   we need to generate now, after position stitching, but before first createOptimizedVertices()
		//   however, we might need to generate again if positions change again
		if (generateNormals)
			mesh1->buildNormals(); // here we build flat normals, later createOptimizedVertices() smooths them

		// stitch normals (even if uv differs too much, therefore merge is not an option. and even if we don't merge at all)
		// (Q: what would this do if generateNormals?
		//  A: usually nothing, normals modified here are overwriten later
		//     however, if (generateNormals && stitchNormals && splitVertices && !mergeVertices) 
		if (stitchNormals)
		{
			// calc smooth normals from mesh1, write them back to mesh1
			const RRMesh* mesh4 = mesh1->createOptimizedVertices(maxLocalDistanceBetweenVerticesToSmooth,maxRadiansBetweenNormalsToSmooth,0,NULL); // stitch more, even if uvs differ
			RRMeshArrays* mesh5 = mesh4->createArrays(true,texcoords,tangents);
			// generate normals (2)
			//   positions might have changed in createOptimizedVertices(), generate again
			//   if stitchPositions, positions are already stitched and createOptimizedVertices did not change them, so this does nothing
			//   if !stitchPositions, positions in mesh4 might differ from mesh1, so generate fixes them
			if (generateNormals)
				mesh5->buildNormals();
			for (unsigned t=0;t<mesh1->numTriangles;t++)
			{
				mesh1->normal[mesh1->triangle[t][0]] = mesh5->normal[mesh5->triangle[t][0]];
				mesh1->normal[mesh1->triangle[t][1]] = mesh5->normal[mesh5->triangle[t][1]];
				mesh1->normal[mesh1->triangle[t][2]] = mesh5->normal[mesh5->triangle[t][2]];
			}
			if (mesh5!=mesh4) delete mesh5;
			if (mesh4!=mesh1) delete mesh4;
			// these are final normals, they should not change later, even merge should merge only what was stitched here
			// (however, when merging but not stitching, normals are not final yet)
		}

		// merge vertices (where all pos/norm/uv are close enough)
		// (even if not smoothing, normals of stitched vertices are stitched, i.e. mesh becomes smoother)
		const RRMesh* mesh2 = mergeVertices ? mesh1->createOptimizedVertices(maxLocalDistanceBetweenVerticesToSmooth,maxRadiansBetweenNormalsToSmooth,maxDistanceBetweenUvsToSmooth,&texcoords) : mesh1; // stitch less, preserve uvs

		// remove degenerated triangles
		const RRMesh* mesh3 = removeDegeneratedTriangles ? mesh2->createOptimizedTriangles() : mesh2;

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

		// generate normals (3)
		//   if (mergeVertices) positions might have changed in last createOptimizedVertices, we should generate normals again
		//   however, if (stitchNormals) normals were stitched, we must not modify them
		if (generateNormals && !stitchNormals)
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
		if (mergeVertices && maxDistanceBetweenVerticesToSmooth>0)
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

	// 2. clear lightmapTexcoord
	if (deleteUnwrap)
		for (unsigned i=0;i<objects.size();i++)
			for (unsigned fg=0;fg<objects[i]->faceGroups.size();fg++)
			{
				RRMaterial* material = objects[i]->faceGroups[fg].material;
				if (material)
					material->lightmapTexcoord = UINT_MAX;
			}

	// 3. delete unused uvs
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		for (unsigned j=0;j<i->first->texcoord.size();j++)
			if (i->second.find(j)==i->second.end() && i->first->texcoord[j])
			{
				i->first->texcoord[j] = NULL;
				i->first->version++;
			}
	}

	// tangents
	if (deleteTangents)
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		if (i->first->tangent || i->first->bitangent)
		{
			i->first->tangent = NULL;
			i->first->bitangent = NULL;
			i->first->version++;
		}
	}
}

void RRObjects::removeEmptyObjects()
{
	unsigned numNonEmpty = 0;
	for (unsigned i=0;i<size();i++)
		if ((*this)[i]->getCollider()->getMesh()->getNumTriangles() && (*this)[i]->getCollider()->getMesh()->getNumVertices())
			(*this)[numNonEmpty++] = (*this)[i];
	numUsed = numNonEmpty;
}

} // namespace
