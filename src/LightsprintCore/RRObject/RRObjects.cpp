// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Collection of objects.
// --------------------------------------------------------------------------

#include <cstdio>
#include <vector>
#include "Lightsprint/RRObject.h"
#include "RRObjectMulti.h"
#include <unordered_set>
#include <unordered_map>
#ifdef RR_LINKS_BOOST
	#include <boost/filesystem.hpp>
	namespace bf = boost::filesystem;
#else
	#include "Lightsprint/RRFileLocator.h"
#endif

#define REALTIME_VBUF_FORMAT BF_RGBF
#define REALTIME_VBUF_SCALED false
#define REALTIME_ENV_FORMAT BF_RGBA
#define REALTIME_ENV_SCALED true // false is faster, but it works better with RGBF and very ancient GPUs don't support such textures

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
					RR_SAFE_DELETE(buffer);
				else
				if (!buffer && _allocateNewBuffers)
				{
					buffer = RRBuffer::create(BT_VERTEX_BUFFER,numVertices,1,1,REALTIME_VBUF_FORMAT,REALTIME_VBUF_SCALED,nullptr);
					buffersTouched++;
				}
				else
				if (buffer && _changeExistingBuffers)
				{
					buffer->reset(BT_VERTEX_BUFFER,numVertices,1,1,REALTIME_VBUF_FORMAT,REALTIME_VBUF_SCALED,nullptr);
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
					mesh->getAABB(&mini,&maxi,nullptr);
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
						buffer = RRBuffer::create(BT_CUBE_TEXTURE,desiredEnvMapSize,desiredEnvMapSize,6,REALTIME_ENV_FORMAT,REALTIME_ENV_SCALED,nullptr);
					else if (desiredEnvMapSize)
					{
						buffer->reset(BT_CUBE_TEXTURE,desiredEnvMapSize,desiredEnvMapSize,6,REALTIME_ENV_FORMAT,REALTIME_ENV_SCALED,nullptr);
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
	if (numNormalsThatMustPointBack>3)
		return 0;
	// gather unique meshes (only mesharrays, basic mesh does not have API for editing)
	unsigned numMeshesWithoutArrays = 0;
	typedef std::unordered_map<RRMeshArrays*,std::vector<RRObject*> > Map;
	Map arrays;
	for (unsigned i=0;i<size();i++)
	{
		RRObject* object = (*this)[i];
		RRMeshArrays* mesh = const_cast<RRMeshArrays*>(dynamic_cast<const RRMeshArrays*>(object->getCollider()->getMesh()));
		if (mesh)
			arrays[mesh].push_back(object);
		else
			numMeshesWithoutArrays++;
	}

	// warn when working with non-arrays
	if (report && numMeshesWithoutArrays)
		RRReporter::report(WARN,"flipFrontBack() supports only RRMeshArrays meshes.\n");

	// flip
	unsigned numFlips = 0;
	for (Map::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		RRMeshArrays* mesh = i->first;
		const std::vector<RRObject*>& objects = i->second;
		// detect scale
		unsigned numObjectWithPositiveScale = 0;
		unsigned numObjectWithNegativeScale = 0;
		for (std::vector<RRObject*>::const_iterator j=objects.begin();j!=objects.end();++j)
		{
			RRVec3 scale3 = (*j)->getWorldMatrixRef().getScale();
			float scale1 = scale3.x*scale3.y*scale3.z;
			if (scale1>0) numObjectWithPositiveScale++;
			if (scale1<0) numObjectWithNegativeScale++;
		}
		bool negativeScale;
		if (numObjectWithPositiveScale && !numObjectWithNegativeScale) negativeScale = false; else
		if (!numObjectWithPositiveScale && numObjectWithNegativeScale) negativeScale = true; else
		{
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"flipFrontBack(): not flipping mesh that has instances with both positive and negative scale.\n"));
			continue;
		}
		// flip
		numFlips += mesh->flipFrontBack(numNormalsThatMustPointBack,negativeScale);
	}
	if (report && numFlips)
	{
		RRReporter::report(WARN,"flipFrontBack(): %d faces flipped.\n",numFlips);
	}
	return numFlips;
}

unsigned RRObjects::buildTangents(bool overwriteExistingTangents) const
{
	unsigned numBuilds = 0;
	for (unsigned i=0;i<size();i++)
	{
		unsigned uvChannel = 0;
		const rr::RRObject::FaceGroups& fgs = (*this)[i]->faceGroups;
		for (unsigned fg=0;fg<fgs.size();fg++)
			if (fgs[fg].material)
			{
				uvChannel = fgs[fg].material->bumpMap.texcoord;
				if (fgs[fg].material->bumpMap.texture)
					break;
			}
		rr::RRMeshArrays* arrays = dynamic_cast<rr::RRMeshArrays*>(const_cast<rr::RRMesh*>((*this)[i]->getCollider()->getMesh()));
		if (arrays && (overwriteExistingTangents || !arrays->tangent))
		{
			arrays->buildTangents(uvChannel);
			numBuilds++;
		}
	}
	return numBuilds;
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

unsigned updateColliders(const RRObjects& objects, bool& aborting)
{
	RRReportInterval report(INF3,"Updating colliders...\n");

	//const RRObjects& objects = *this;
	std::unordered_set<const RRCollider*> updatedColliders; // optimization: prevents updating shared collider more than once
	for (unsigned i=0;i<objects.size();i++)
	if (!aborting)
	{
		const RRObject* object = objects[i];
		if (object)
		{
			RRCollider* collider = object->getCollider();
			if (collider && updatedColliders.find(collider)==updatedColliders.end())
			{
				const RRMesh* mesh = collider->getMesh();
				if (mesh)
				{
					RRMeshArrays* arrays = dynamic_cast<RRMeshArrays*>(const_cast<RRMesh*>(mesh));
					if (arrays)
					{
						RRCollider::IntersectTechnique it = collider->getTechnique();
						if (it!=RRCollider::IT_LINEAR)
						{
							collider->setTechnique(RRCollider::IT_LINEAR,aborting);
							collider->setTechnique(it,aborting);
							updatedColliders.insert(collider);
						}
					}
				}
			}
		}
	}
	return (unsigned)updatedColliders.size();
}

RRObject* RRObjects::createMultiObject(RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, bool optimizeTriangles, unsigned speed, const char* cacheLocation) const
{
	if (!size())
		return nullptr;
	switch(speed)
	{
		case 0: return RRObjectMultiSmall::create(&(*this)[0],size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		case 1: return RRObjectMultiFast::create(&(*this)[0],size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,false,cacheLocation);
		default: return RRObjectMultiFast::create(&(*this)[0],size(),intersectTechnique,aborting,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch,optimizeTriangles,true,cacheLocation);
	}
		
}

RRObjects RRObjects::mergeObjects(bool splitByMaterial) const
{
	unsigned numDynamicObjects = 0;
	for (unsigned i=0;i<size();i++)
		if ((*this)[i]->isDynamic)
			numDynamicObjects++;

	// don't merge tangents if sources clearly have no tangents
	bool tangents = false;
	{
		for (unsigned i=0;i<size();i++)
		{
			const rr::RRObject* object = (*this)[i];
			const rr::RRMeshArrays* meshArrays = dynamic_cast<const rr::RRMeshArrays*>(object->getCollider()->getMesh());
			if (!meshArrays || meshArrays->tangent)
				tangents = true;
		}
	}

	rr::RRReporter::report(rr::INF2,"Merging...\n");
	bool aborting = false;
	rr::RRObject* oldObject = createMultiObject(rr::RRCollider::IT_LINEAR,aborting,-1,-1,false,0,nullptr);

	// convert oldObject with Multi* into newObject with RRMeshArrays
	// if we don't do it
	// - solver->getMultiObject() preimport numbers would point to many 1objects, although there is only one 1object now
	// - attempt to smooth scene would fail, it needs arrays
	const rr::RRCollider* oldCollider = oldObject->getCollider();
	const rr::RRMesh* oldMesh = oldCollider->getMesh();
	rr::RRVector<unsigned> texcoords;
	oldMesh->getUvChannels(texcoords);
	rr::RRMeshArrays* newMesh = oldMesh->createArrays(true,texcoords,tangents);
	rr::RRCollider* newCollider = rr::RRCollider::create(newMesh,nullptr,rr::RRCollider::IT_LINEAR,aborting);
	rr::RRObject* newObject = new rr::RRObject;
	newObject->faceGroups = oldObject->faceGroups;
	newObject->setCollider(newCollider);
	newObject->isDynamic = numDynamicObjects>size()/2;
	delete oldObject;

	rr::RRObjects newList;
	newList.push_back(newObject); // memleak, newObject is never freed
	// ensure that there is one facegroup per material
	rr::RRReporter::report(rr::INF2,"Optimizing...\n");
	newList.optimizeFaceGroups(newObject); // we know there are no other instances of newObject's mesh

	if (splitByMaterial)
	{
		rr::RRReporter::report(rr::INF2,"Splitting...\n");
		// split by facegroups=materials
		newList.clear();
		unsigned firstTriangleIndex = 0;
		for (unsigned f=0;f<newObject->faceGroups.size();f++)
		{
			// create splitObject from newObject->faceGroups[f]
			rr::RRMeshArrays* splitMesh = nullptr;
			{
				// temporarily hide triangles with other materials
				unsigned tmpNumTriangles = newMesh->numTriangles;
				newMesh->numTriangles = newObject->faceGroups[f].numTriangles;
				newMesh->triangle += firstTriangleIndex;
				// filter out unused vertices
				const rr::RRMesh* optimizedMesh = newMesh->createOptimizedVertices(0,0,0,&texcoords);
				// read result into new mesh
				splitMesh = optimizedMesh->createArrays(true,texcoords,tangents);
				// delete temporaries
				delete optimizedMesh;
				// unhide triangles
				newMesh->triangle -= firstTriangleIndex;
				newMesh->numTriangles = tmpNumTriangles;
			}
			rr::RRCollider* splitCollider = rr::RRCollider::create(splitMesh,nullptr,rr::RRCollider::IT_LINEAR,aborting);
			rr::RRObject* splitObject = new rr::RRObject;
			splitObject->faceGroups.push_back(newObject->faceGroups[f]);
			splitObject->setCollider(splitCollider);
			splitObject->isDynamic = newObject->isDynamic;
			splitObject->name = splitObject->faceGroups[0].material->name; // name object by material
			newList.push_back(splitObject); // memleak, splitObject is never freed
			firstTriangleIndex += newObject->faceGroups[f].numTriangles;
		}
	}

	return newList;
}

void RRObjects::smoothAndStitch(bool splitVertices, bool mergeVertices, bool removeUnusedVertices, bool removeDegeneratedTriangles, bool stitchPositions, bool stitchNormals, bool generateNormals, float maxDistanceBetweenVerticesToSmooth, float maxRadiansBetweenNormalsToSmooth, float maxDistanceBetweenUvsToSmooth, bool report) const
{
	// gather unique meshes (only mesharrays, basic mesh does not have API for editing)
	unsigned numMeshesWithoutArrays = 0;
	std::unordered_set<RRMeshArrays*> arrays;
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
	for (std::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		numTriangles += (*i)->numTriangles;
		numVertices += (*i)->numVertices;
	}
	for (unsigned i=0;i<size();i++)
		numFaceGroups += (*this)[i]->faceGroups.size();

	// stitch
	unsigned numMeshesDifferentScales = 0;
	unsigned numMeshesNonUniformScale = 0;
	for (std::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
	{
		RRMeshArrays* mesh = *i;
		bool tangents = mesh->tangent!=nullptr;

		// create temporary list of objects with this mesh
		RRObjects objects;
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
			const RRMesh* mesh4 = mesh1->createOptimizedVertices(maxLocalDistanceBetweenVerticesToSmooth,100,0,nullptr); // stitch more, even if normals or uvs differ
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
			const RRMesh* mesh4 = mesh1->createOptimizedVertices(maxLocalDistanceBetweenVerticesToSmooth,maxRadiansBetweenNormalsToSmooth,0,nullptr); // stitch more, even if uvs differ
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
		bool removedDegeneratedTriangles = mesh3!=mesh2;

		// remove unused vertices (previously used in degenerated triangles)
		const RRMesh* mesh4 = removeUnusedVertices ? mesh3->createOptimizedVertices(-1,-1,-1,nullptr) : mesh3;

		// fix facegroups in objects
		if (removeDegeneratedTriangles) // facegroups should be unchanged if we did not remove triangles
		{
			for (unsigned j=0;j<objects.size();j++)
			{
				RRObject::FaceGroups faceGroups;
				unsigned mesh4_numTriangles = mesh4->getNumTriangles();
				for (unsigned postImportTriangle=0;postImportTriangle<mesh4_numTriangles;postImportTriangle++)
				{
					unsigned preImportTriangle = mesh4->getPreImportTriangle(postImportTriangle).index;
					RRMaterial* m = objects[j]->getTriangleMaterial(preImportTriangle,nullptr,nullptr);
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
		(*i)->reload(mesh4,true,texcoords,tangents);

		// generate normals (3)
		//   if (mergeVertices) positions might have changed in last createOptimizedVertices, we should generate normals again
		//   however, if (stitchNormals) normals were stitched, we must not modify them
		if (generateNormals && !stitchNormals)
			(*i)->buildNormals();

		// delete temporary meshes
		if (mesh4!=mesh3) delete mesh4;
		if (mesh3!=mesh2) delete mesh3;
		if (mesh2!=mesh1) delete mesh2;
		if (mesh1!=mesh) delete mesh1;

		// update colliders (only necessary if triangle bodies change)
		if (removedDegeneratedTriangles
			// here we test whether vertex positions COULD have changed. testing whether positions DID change would be beter but more complicated
			|| ((mergeVertices || stitchPositions) && maxDistanceBetweenVerticesToSmooth))
		{
			bool neverAbort = false;
			updateColliders(objects,neverAbort);
		}
	}

	// stats
	unsigned numTriangles2 = 0;
	unsigned numVertices2 = 0;
	unsigned numFaceGroups2 = 0;
	for (std::unordered_set<RRMeshArrays*>::iterator i=arrays.begin();i!=arrays.end();++i)
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
	RRMaterials materials;
	getAllMaterials(materials);
	// multiply
	for (unsigned i=0;i<materials.size();i++)
	{
		if (materials[i])
		{
			materials[i]->diffuseEmittance.multiplyAdd(RRVec4(emissiveMultiplier),RRVec4(0));
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
	typedef std::unordered_set<unsigned> UvChannels;
	typedef std::unordered_map<RRMeshArrays*,UvChannels> Meshes;
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
									if (!deleteUnwrap) INSERT(lightmap.texcoord);
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
						RRReporter::report(WARN,"Texcoords not removed, scene contains non-RRMeshArrays meshes. This affects 3ds, mgf, try changing format.\n");
						return;
					}
				}
			}
		}
	}

	// 2. clear lightmap.texcoord
	if (deleteUnwrap)
		for (unsigned i=0;i<objects.size();i++)
			for (unsigned fg=0;fg<objects[i]->faceGroups.size();fg++)
			{
				RRMaterial* material = objects[i]->faceGroups[fg].material;
				if (material)
					material->lightmap.texcoord = UINT_MAX;
			}

	// 3. delete unused uvs, clear unwrapChannel
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		// delete unused uvs
		for (unsigned j=0;j<i->first->texcoord.size();j++)
			if (i->second.find(j)==i->second.end() && i->first->texcoord[j])
			{
				i->first->texcoord[j] = nullptr;
				i->first->version++;
			}
		// clear unwrapChannel
		i->first->unwrapChannel = UINT_MAX;
	}

	// tangents
	if (deleteTangents)
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		if (i->first->tangent || i->first->bitangent)
		{
			i->first->tangent = nullptr;
			i->first->bitangent = nullptr;
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

void RRObjects::getAllMaterials(RRMaterials& materials) const
{
	typedef std::unordered_set<RRMaterial*> Set;
	Set set;
	// fill set
	for (unsigned i=materials.size();i--;) // iteration from 0 would flip order of materials in RRMaterials. we don't promise to preserve order, but at least we try to, this mostly works (although it probably depends on implementation of unordered_set)
		set.insert(materials[i]);
	for (unsigned i=0;i<size();i++)
	{
		RRObject::FaceGroups& faceGroups = (*this)[i]->faceGroups;
		for (unsigned g=0;g<faceGroups.size();g++)
		{
			set.insert(faceGroups[g].material);
		}
	}
	// copy set to buffers
	materials.clear();
	for (Set::const_iterator i=set.begin();i!=set.end();++i)
		if (*i)
			materials.push_back(*i);
}

void RRObjects::updateColorLinear(const RRColorSpace* colorSpace) const
{
	typedef std::unordered_set<RRMaterial*> Set;
	Set set;
	// gather all materials (it has to be fast, using getAllMaterials would be slow)
	for (unsigned i=0;i<size();i++)
	{
		RRObject::FaceGroups& faceGroups = (*this)[i]->faceGroups;
		for (unsigned g=0;g<faceGroups.size();g++)
		{
			set.insert(faceGroups[g].material);
		}
	}
	// color->colorLinear
	for (Set::const_iterator i=set.begin();i!=set.end();++i)
		if (*i)
			(*i)->convertToLinear(colorSpace);
}

static std::wstring filenamized(RRString& name)
{
	std::wstring filename;
	filename = name.w_str();
	for (unsigned i=0;i<filename.size();i++)
		if (wcschr(L"<>:\"/\\|?*",filename[i]))
			filename[i] = '_';
	return filename;
}

void splitName(std::wstring& prefix, unsigned long long& number)
{
	number = 0;
	unsigned long long base = 1;
	while (prefix.size() && prefix[prefix.size()-1]>='0' && prefix[prefix.size()-1]<='9')
	{
		number += base * (prefix[prefix.size()-1]-'0');
		prefix.pop_back();
		base *= 10;
	}
}

std::wstring mergedName(const std::wstring& prefix, unsigned long long number)
{
	return prefix + ((number<10)?L"0":L"") + std::to_wstring(number);
}

void RRObjects::makeNamesUnique() const
{
	// naming scheme 1
	// turns names xxx,xxx,xxx,xxx5,xxx5 into xxx,xxx01,xxx02,xxx5,xxx06
	typedef std::unordered_set<std::wstring> Set;
	typedef std::unordered_map<std::wstring,unsigned long long> Map;
	Map filenamizedOriginals; // value = the highest number so far assigned to this filename, we skip lower numbers when searching for unused one
	Set filenamizedAccepted;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		RRObject* object = (*this)[objectIndex];
		filenamizedOriginals[filenamized(object->name)] = 0;
	}
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		RRObject* object = (*this)[objectIndex];
		std::wstring filenamizedOriginal = filenamized(object->name);
		std::wstring filenamizedCandidate = filenamizedOriginal;
		if (filenamizedAccepted.find(filenamizedCandidate)==filenamizedAccepted.end())
		{
			// 1. accept "so far" unique names
			filenamizedAccepted.insert(filenamizedCandidate);
		}
		else
		{
			// 2. split non-unique name to prefix + number
			std::wstring filenamizedPrefix = filenamizedCandidate;
			unsigned long long number;
			splitName(filenamizedPrefix,number);
			// optimization step1: skip numbers that are definitely not free
			if (filenamizedOriginals[filenamizedOriginal])
				number = filenamizedOriginals[filenamizedOriginal];
			// 3. try increasing number until it becomes "globally" unique
			do
			{
				number++;
				filenamizedCandidate = mergedName(filenamizedPrefix,number);
			}
			while (filenamizedAccepted.find(filenamizedCandidate)!=filenamizedAccepted.end() || filenamizedOriginals.find(filenamizedCandidate)!=filenamizedOriginals.end());
			filenamizedAccepted.insert(filenamizedCandidate);
			// optimization step2: remember numbers that are definitely not free
			filenamizedOriginals[filenamizedOriginal] = number;
			// 4. construct final non-filenamized name
			std::wstring rawPrefix = RR_RR2STDW(object->name);
			unsigned long long rawNumber;
			splitName(rawPrefix,rawNumber);
			object->name = RR_STDW2RR(mergedName(rawPrefix,number));
		}
	}
#if 0
	// naming scheme 2
	// turns names xxx,xxx,xxx,xxx.3,xxx.3 into xxx,xxx.2,xxx.4,xxx.3,xxx.3.2
again:
	bool modified = false;
	typedef std::unordered_multimap<std::wstring,RRObject*> Map;
	Map map;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		RRObject* object = (*this)[objectIndex];
		map.insert(Map::value_type(filenamized(object->name),object));
	}
	for (Map::iterator i=map.begin();i!=map.end();)
	{
		Map::iterator j = i;
		unsigned numEquals = 0;
		while (j!=map.end() && i->first==j->first)
		{
			numEquals++;
			j++;
		}
		if (numEquals>1)
		{
			unsigned numDigits = 0;
			while (numEquals) {numDigits++; numEquals/=10;}
			modified = true;
			unsigned index = 1;
			for (j=i;j!=map.end() && i->first==j->first;++j)
			{
				if (index>1)
				{
					// 1: try to format new name
					// if it is already taken, increase index and goto 1
					try_index:
					RRString newNameCandidate;
					newNameCandidate.format(L"%ls%hs%0*d",j->second->name.w_str(),j->second->name.empty()?"":".",numDigits,index);
					if (map.find(filenamized(newNameCandidate))!=map.end())
					{
						index++;
						goto try_index;
					}
					j->second->name = newNameCandidate;
				}
				index++;
			}
		}
		i = j;
	}
	if (modified)
	{
		// "name" was changed to "name.1", but we did not yet check that "name.1" is free, let's run all checks again
		goto again;
	}
#endif
}


unsigned RRObjects::loadLayer(int layerNumber, const RRString& path, const RRString& ext) const
{
	unsigned result = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObject* object = (*this)[objectIndex];
			// first try to load per-pixel format
			RRBuffer* buffer = nullptr;
			RRObject::LayerParameters layerParameters;
			layerParameters.suggestedPath = path;
			layerParameters.suggestedExt = ext;
			object->recommendLayerParameters(layerParameters);
#ifdef RR_LINKS_BOOST
			boost::system::error_code ec;
			if ( !bf::exists(RR_RR2PATH(layerParameters.actualFilename),ec) || !(buffer=RRBuffer::load(layerParameters.actualFilename,nullptr)) )
#else
			RRFileLocator fl;
			if ( !fl.exists(layerParameters.actualFilename) || !(buffer=RRBuffer::load(layerParameters.actualFilename,nullptr)) )
#endif
			{
				// if it fails, try to load per-vertex format
				layerParameters.suggestedMapWidth = layerParameters.suggestedMapHeight = 0;
				object->recommendLayerParameters(layerParameters);
#ifdef RR_LINKS_BOOST
				if (bf::exists(RR_RR2PATH(layerParameters.actualFilename),ec))
#else
				if (fl.exists(layerParameters.actualFilename))
#endif
					buffer = RRBuffer::load(layerParameters.actualFilename.c_str());
			}
			if (buffer && buffer->getType()==BT_VERTEX_BUFFER && buffer->getWidth()!=object->getCollider()->getMesh()->getNumVertices())
			{
				RR_LIMITED_TIMES(5,RRReporter::report(ERRO,"%ls has wrong size.\n",layerParameters.actualFilename.w_str()));
				RR_SAFE_DELETE(buffer);
			}
			if (buffer)
			{
				delete object->illumination.getLayer(layerNumber);
				object->illumination.getLayer(layerNumber) = buffer;
				result++;
				RRReporter::report(INF3,"Loaded %ls.\n",layerParameters.actualFilename.w_str());
			}
			else
			{
				RRReporter::report(INF3,"Not loaded %ls.\n",layerParameters.actualFilename.w_str());
			}
		}
		RRReporter::report(INF2,"Loaded %d/%d buffers from %ls<object name>%ls to layer %d.\n",result,size(),path.w_str(),ext.w_str(),layerNumber);
	}
	return result;
}

unsigned RRObjects::saveLayer(int layerNumber, const RRString& path, const RRString& ext) const
{
	bool directoryCreated = false;
	unsigned numBuffers = 0;
	unsigned numSaved = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObject* object = (*this)[objectIndex];
			RRBuffer* buffer = object->illumination.getLayer(layerNumber);
			if (buffer)
			{
				// create destination directories
#ifdef RR_LINKS_BOOST
				if (!directoryCreated)
				{
					bf::path prefix(RR_RR2PATH(path));
					prefix.remove_filename();
					boost::system::error_code ec;
					bf::exists(prefix,ec) || bf::create_directories(prefix,ec);
					directoryCreated = true;
				}
#endif

				numBuffers++;
				RRObject::LayerParameters layerParameters;
				layerParameters.suggestedPath = path;
				layerParameters.suggestedExt = ext;
				layerParameters.suggestedMapWidth = layerParameters.suggestedMapHeight = (buffer->getType()==BT_VERTEX_BUFFER) ? 0 : 256;
				object->recommendLayerParameters(layerParameters);
				if (buffer->save(layerParameters.actualFilename))
				{
					numSaved++;
					RRReporter::report(INF3,"Saved %ls.\n",layerParameters.actualFilename.w_str());
				}
				else
				if (!layerParameters.actualFilename.empty())
					RRReporter::report(WARN,"Not saved %ls.\n",layerParameters.actualFilename.w_str());
			}
		}
		if (!numBuffers)
			; // don't report saving empty layer
		else
		if (numSaved)
			RRReporter::report(INF2,"Saved %d/%d buffers into %ls<object name>.%ls from layer %d.\n",numSaved,numBuffers,path.w_str(),ext.w_str(),layerNumber);
		else
			RRReporter::report(WARN,"Failed to save layer %d (%d buffers) into %ls.\n",layerNumber,numBuffers,path.w_str());
	}
	return numSaved;
}

unsigned RRObjects::layerExistsInMemory(int layerNumber) const
{
	unsigned result = 0;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		if ((*this)[objectIndex]->illumination.getLayer(layerNumber))
			result++;
	return result;
}

unsigned RRObjects::layerDeleteFromMemory(int layerNumber) const
{
	unsigned result = 0;
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		if ((*this)[objectIndex]->illumination.getLayer(layerNumber))
		{
			RR_SAFE_DELETE((*this)[objectIndex]->illumination.getLayer(layerNumber));
			result++;
		}
	return result;
}

unsigned RRObjects::layerDeleteFromDisk(const RRString& path, const RRString& ext) const
{
	unsigned result = 0;
#ifdef RR_LINKS_BOOST
	for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
	{
		rr::RRObject::LayerParameters layerParameters;
		layerParameters.suggestedPath = path;
		layerParameters.suggestedExt = ext;
		(*this)[objectIndex]->recommendLayerParameters(layerParameters);
		bf::path path(RR_RR2PATH(layerParameters.actualFilename));
		boost::system::error_code ec;
		if (bf::exists(path,ec))
		{
			if (bf::remove(path,ec))
				result++;
		}
	}
#endif
	return result;
}

} // namespace
