// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"
#include "Lightsprint/DemoEngine/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "ObjectBuffers.h"

namespace rr_gl
{

ObjectBuffers::ObjectBuffers(const rr::RRObject* object, bool indexed)
{
	initedOk = false;
	rr::RRMesh* mesh = object->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	numIndices = 0;
	indices = NULL;
	if(indexed)
	{
		indices = new unsigned[numTriangles*3]; // Always allocates worst case (no vertices merged) scenario size. Only first numIndices is used.
	}
	numVertices = 0;
	// Always allocates worst case scenario size. Only first numVertices is used.
	#define NEW_ARRAY(arr,type) arr = new rr::type[numTriangles*3]; memset(arr,0,sizeof(rr::type)*numTriangles*3);
	NEW_ARRAY(avertex,RRVec3);
	NEW_ARRAY(anormal,RRVec3);
	NEW_ARRAY(atexcoordDiffuse,RRVec2);
	NEW_ARRAY(atexcoordEmissive,RRVec2);
	NEW_ARRAY(atexcoordForced2D,RRVec2);
	NEW_ARRAY(atexcoordAmbient,RRVec2);
	#undef NEW_ARRAY
	const rr::RRMaterial* previousMaterial = NULL;
	for(unsigned t=0;t<numTriangles;t++)
	{
		// read triangle params
		rr::RRMesh::Triangle triangleVertices;
		mesh->getTriangle(t,triangleVertices);
		rr::RRMesh::TriangleNormals triangleNormals;
		mesh->getTriangleNormals(t,triangleNormals);
		rr::RRMesh::TriangleMapping triangleMapping;
		mesh->getTriangleMapping(t,triangleMapping);
		rr::RRVec2 diffuseUv[3];
		mesh->getChannelData(CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV,t,diffuseUv,sizeof(diffuseUv));
		rr::RRVec2 emissiveUv[3];
		mesh->getChannelData(CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV,t,emissiveUv,sizeof(emissiveUv));
		// material change? -> start new facegroup
		const rr::RRMaterial* material = object->getTriangleMaterial(t);
		if(!t || material!=previousMaterial)
		{
			FaceGroup fg;
			if(indexed)
			{
				fg.firstIndex = numIndices;
			}
			else
			{
				fg.firstIndex = numVertices;
			}
			fg.numIndices = 0;
			fg.diffuseColor = material ? material->diffuseReflectance : rr::RRVec3(0);
			fg.diffuseTexture = NULL;
			object->getChannelData(CHANNEL_TRIANGLE_DIFFUSE_TEX,t,&fg.diffuseTexture,sizeof(fg.diffuseTexture));
			fg.emissiveTexture = NULL;
			object->getChannelData(CHANNEL_TRIANGLE_EMISSIVE_TEX,t,&fg.emissiveTexture,sizeof(fg.emissiveTexture));
			// it's still possible that user will render without texture
			//if(!fg.diffuseTexture)
			//{
			//	LIMITED_TIMES(1,rr::RRReporter::report(rr::RRReporter::WARN,"RRRendererOfRRObject: Diffuse texture not available.\n"));
			//}
			faceGroups.push_back(fg);
			previousMaterial = material;
		}
		// generate vertices and indices into buffers
		for(unsigned v=0;v<3;v++)
		{
			unsigned currentVertex;
			if(indexed)
			{
				// force original RRObject vertex order
				//numVertices = triangleVertices[v];
				// use preimport index, because of e.g. optimizations in RRObjectMulti
				currentVertex = mesh->getPreImportVertex(triangleVertices[v],t);
				indices[numIndices++] = currentVertex;
				numVertices = MAX(numVertices,currentVertex+1);
			}
			else
			{
				currentVertex = numVertices;
				numVertices++;
			}
			if(currentVertex>=numTriangles*3)
			{
				// preimport vertex number is out of range, fail
				// warning: could happen with correct inputs, RRMesh is allowed 
				//  to have preimport indices 1,10,100(out of range!) even when postimport are 0,1,2
				//RR_ASSERT(currentVertex<numTriangles*3);
				return;
			}
			mesh->getVertex(triangleVertices[v],avertex[currentVertex]);
			anormal[currentVertex] = triangleNormals.norm[v];
			atexcoordAmbient[currentVertex] = triangleMapping.uv[v];
			atexcoordDiffuse[currentVertex] = diffuseUv[v];
			atexcoordEmissive[currentVertex] = emissiveUv[v];
		}
		// generate facegroups
		faceGroups[faceGroups.size()-1].numIndices += 3;
	}
	initedOk = true;
}

ObjectBuffers::~ObjectBuffers()
{
	delete[] atexcoordDiffuse;
	delete[] atexcoordEmissive;
	delete[] atexcoordForced2D;
	delete[] atexcoordAmbient;
	delete[] anormal;
	delete[] avertex;
	delete[] indices;
}

bool ObjectBuffers::inited()
{
	return initedOk;
}

void ObjectBuffers::render(RendererOfRRObject::Params& params)
{
	RR_ASSERT(initedOk);
	if(!initedOk) return;
	// set vertices
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, &avertex[0].x);
	// set normals
	bool setNormals = params.renderedChannels.LIGHT_DIRECT || params.renderedChannels.LIGHT_INDIRECT_ENV;
	if(setNormals)
	{
		glEnableClientState(GL_NORMAL_ARRAY);
		glNormalPointer(GL_FLOAT, 0, &anormal[0].x);
	}
	// set indirect illumination vertices
	if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR && params.indirectIllumination)
	{
		RR_ASSERT(indices); // indirectIllumination has vertices merged according to RRObject, can't be used with non-indexed trilist, needs indexed trilist
		unsigned bufferSize = params.indirectIllumination->getNumVertices();
		RR_ASSERT(numVertices<=bufferSize); // indirectIllumination buffer must be of the same size (or bigger) as our vertex buffer. It's bigger if last vertices in original vertex order are unused (it happens in .bsp).
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(3, GL_FLOAT, 0, params.indirectIllumination->lock());
	}
	// set indirect illumination texcoords + map
	if(params.renderedChannels.LIGHT_INDIRECT_MAP && params.indirectIlluminationMap)
	{
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_LIGHT_INDIRECT);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, &atexcoordAmbient[0].x);
		glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_LIGHT_INDIRECT);
		params.indirectIlluminationMap->bindTexture();
	}
	// set 2d_position texcoords
	if(params.renderedChannels.FORCE_2D_POSITION)
	{
		// this should not be executed in every frame, generated texcoords change rarely
		RR_ASSERT(!indices); // needs non-indexed trilist
		//for(unsigned i=0;i<numVertices;i++) // for all capture textures, probably not necessary
		for(unsigned i=params.firstCapturedTriangle*3;i<3*params.lastCapturedTrianglePlus1;i++) // only for our capture texture
		{
			params.generateForcedUv->generateData(i/3, i%3, &atexcoordForced2D[i].x, sizeof(atexcoordForced2D[i]));
		}
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_FORCED_2D);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, &atexcoordForced2D[0].x);
	}
	// set material diffuse texcoords
	if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_DIFFUSE);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, &atexcoordDiffuse[0].x);
	}
	// set material emissive texcoords
	if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_EMISSIVE);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, &atexcoordEmissive[0].x);
	}
	// render facegroups (facegroups differ by material)
	if(params.renderedChannels.MATERIAL_DIFFUSE_VCOLOR || params.renderedChannels.MATERIAL_DIFFUSE_MAP || params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		for(unsigned fg=0;fg<faceGroups.size();fg++)
		{
			unsigned firstIndex = faceGroups[fg].firstIndex;
			int numIndices = faceGroups[fg].numIndices;
			// limit rendered indices to capture range
			numIndices = MIN(firstIndex+numIndices,3*params.lastCapturedTrianglePlus1) - MAX(firstIndex,3*params.firstCapturedTriangle);
			firstIndex = MAX(firstIndex,3*params.firstCapturedTriangle);
			if(numIndices>0)
			{
				// set diffuse color
				if(params.renderedChannels.MATERIAL_DIFFUSE_VCOLOR)
				{
					glSecondaryColor3fv(&faceGroups[fg].diffuseColor[0]);
				}
				// set diffuse map
				if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
				{
					glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_MATERIAL_DIFFUSE);
					de::Texture* tex = faceGroups[fg].diffuseTexture;
					if(tex)
					{
						tex->bindTexture();
					}
					else
					{
						LIMITED_TIMES(1,rr::RRReporter::report(rr::RRReporter::ERRO,"RRRendererOfRRObject: Texturing requested, but diffuse texture not available, expect incorrect render.\n"));
					}
				}
				// set emissive map
				if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
				{
					glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_MATERIAL_EMISSIVE);
					de::Texture* tex = faceGroups[fg].emissiveTexture;
					if(tex)
					{
						tex->bindTexture();
					}
					else
					{
						LIMITED_TIMES(1,rr::RRReporter::report(rr::RRReporter::ERRO,"RRRendererOfRRObject: Texturing requested, but emissive texture not available, expect incorrect render.\n"));
					}
				}
				// render one facegroup
				if(indices)
				{
					for(unsigned i=firstIndex;i<firstIndex+numIndices;i++) RR_ASSERT(indices[i]<numVertices);
					glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, &indices[firstIndex]);
				}
				else
				{
					glDrawArrays(GL_TRIANGLES, firstIndex, numIndices);
				}
			}
		}
	}
	else
	{
		// limit rendered indices to capture range
		unsigned firstIndex = 3*params.firstCapturedTriangle;
		int numIndices = 3*(params.lastCapturedTrianglePlus1-params.firstCapturedTriangle);
		// render all at once
		if(indices)
		{
			for(unsigned i=firstIndex;i<firstIndex+numIndices;i++) RR_ASSERT(indices[i]<numVertices);
			glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, &indices[firstIndex]);
		}
		else
		{
			glDrawArrays(GL_TRIANGLES, firstIndex, numIndices);
		}
	}
	// unset material diffuse texcoords
	if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_DIFFUSE);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset material emissive texcoords
	if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_EMISSIVE);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset 2d_position texcoords
	if(params.renderedChannels.FORCE_2D_POSITION)
	{
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_FORCED_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset indirect illumination texcoords + map
	if(params.renderedChannels.LIGHT_INDIRECT_MAP && params.indirectIlluminationMap)
	{
		glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_LIGHT_INDIRECT);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_LIGHT_INDIRECT);
		glBindTexture(GL_TEXTURE_2D,0);
	}
	// unset indirect illumination colors
	if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR && params.indirectIllumination)
	{
		glDisableClientState(GL_COLOR_ARRAY);
		params.indirectIllumination->unlock();
	}
	// unset normals
	if(setNormals)
	{
		glDisableClientState(GL_NORMAL_ARRAY);
	}
	// unset vertices
	glEnableClientState(GL_VERTEX_ARRAY);
}


}; // namespace
