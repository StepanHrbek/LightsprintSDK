// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "ObjectBuffers.h"
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif

//#define USE_VBO // use VBO to save unnecessary CPU<->GPU data transfers
// using advanced features may trigger driver bugs, better avoid VBO for now
// as I haven't registered any significant speedup in real world scenarios

namespace rr_gl
{

ObjectBuffers::ObjectBuffers(const rr::RRObject* object, bool indexed)
{
	initedOk = false;
	rr::RRMesh* mesh = object->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	// numVerticesMax is only estimate of numPreImportVertices
	// we have enough time in constructor, what about computing it precisely? (to save memory)
	unsigned numVerticesMax = numTriangles*3; // *3 is usual worst case.. additional *2 covers uncommon situation in WoP maps without BB
	numIndices = 0;
	indices = NULL;
	if(indexed)
	{
		indices = new unsigned[numTriangles*3]; // Always allocates worst case (no vertices merged) scenario size. Only first numIndices is used.
	}
	numVertices = 0;
	// Always allocates worst case scenario size. Only first numVertices is used.
	#define NEW_ARRAY(arr,type) {arr = new rr::type[numVerticesMax]; memset(arr,0,sizeof(rr::type)*numVerticesMax);}
	NEW_ARRAY(avertex,RRVec3);
	NEW_ARRAY(anormal,RRVec3);

	unsigned hasDiffuseMap = 0;
	mesh->getChannelSize(rr::RRMesh::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV,&hasDiffuseMap,NULL);
	if(hasDiffuseMap)
		NEW_ARRAY(atexcoordDiffuse,RRVec2)
	else
		atexcoordDiffuse = NULL;

	unsigned hasEmissiveMap = 0;
	mesh->getChannelSize(rr::RRMesh::CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV,&hasEmissiveMap,NULL);
	if(hasEmissiveMap)
		NEW_ARRAY(atexcoordEmissive,RRVec2)
	else
		atexcoordEmissive = NULL;

	NEW_ARRAY(atexcoordForced2D,RRVec2);
	NEW_ARRAY(atexcoordAmbient,RRVec2);
	alightIndirectVcolor = rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVerticesMax,1,1,rr::BF_RGBF,false,NULL);

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
		if(hasDiffuseMap)
		{
			mesh->getChannelData(rr::RRMesh::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV,t,diffuseUv,sizeof(diffuseUv));
		}
		rr::RRVec2 emissiveUv[3];
		if(hasEmissiveMap)
		{
			mesh->getChannelData(rr::RRMesh::CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV,t,emissiveUv,sizeof(emissiveUv));
		}
/*		// material change? -> start new facegroup
		// a) rendering into shadowmap, check shadowing flags
		//    -> test t,light,receiver
		//       kdyz pro aspon 1 receiver true, kreslit, tj.vrhat stiny (muze byt nepresne, ale nebudu delat extra shadowmapu pro kazdy receiver)
		//       kdyz pro aspon 1 light true, kreslit, tj.vrhat stiny (muze byt nepresne, ale nebudu delat extra ObjectBuffers pro kazde svetlo)
		// b) rendering lit, check lighting flags
		//    -> test t,light,NULL
		//       kdyz pro aspon 1 light true, kreslit, tj.osvitit (muze byt nepresne, ale nebudu delat extra ObjectBuffers pro kazde svetlo)
		const rr::RRMaterial* material;
		if(!params.light // rendering indirect, so no skipping, everything is rendered
			|| !params.renderingShadowCasters) // accumulating lit renders, so skipping render=disabling lighting
		{
			material = object->getTriangleMaterial(t,params.light,NULL);
		}
		else
		{
			// rendering into shadowmap, so skipping render=disabling shadow
			// kdyz pro aspon 1 receiver true, kreslit, tj.vrhat stiny (muze byt nepresne, ale nebudu delat extra shadowmapu pro kazdy receiver)
			for(unsigned i=0;i<params.scene->getNumObjects();i++)
			{
				if(material = object->getTriangleMaterial(t,params.light,params.scene->getObject(i)))
					break;
			}
		}
		if(!material) continue; // skip rendering triangles without material
*/
		const rr::RRMaterial* material = object->getTriangleMaterial(t,NULL,NULL);
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
			RR_ASSERT(material);
			fg.renderFront = !material || material->sideBits[0].renderFrom;
			fg.renderBack = !material || material->sideBits[1].renderFrom;
			fg.numIndices = 0;
			fg.diffuseColor = material ? material->diffuseReflectance : rr::RRVec3(0);
			fg.transparency = material ? material->specularTransmittance.avg() : 0;
			fg.emissiveColor = material ? material->diffuseEmittance : rr::RRVec3(0);
			fg.diffuseTexture = NULL;
			if(hasDiffuseMap)
			{
				object->getChannelData(rr::RRMesh::CHANNEL_TRIANGLE_DIFFUSE_TEX,t,&fg.diffuseTexture,sizeof(fg.diffuseTexture));
				if(!fg.diffuseTexture)
				{
					// it's still possible that user will render without texture
					LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"RRRendererOfRRObject: Object has diffuse texcoords, but no diffuse texture.\n"));
				}
			}
			fg.emissiveTexture = NULL;
			if(hasEmissiveMap)
			{
				object->getChannelData(rr::RRMesh::CHANNEL_TRIANGLE_EMISSIVE_TEX,t,&fg.emissiveTexture,sizeof(fg.emissiveTexture));
			}
			faceGroups.push_back(fg);
			previousMaterial = material;
		}
		// generate vertices and indices into buffers
		for(unsigned v=0;v<3;v++)
		{
			unsigned currentVertex;
			if(indexed)
			{
				//numVertices = triangleVertices[v];

				// force original RRObject vertex order
				// why?
				//  RRDynamicSolver generates ambient vertex buffers for original vertex order.
				//  to render them, whole mesh must be in original vertex order
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
			if(currentVertex>=numVerticesMax)
			{
				// preimport vertex number is out of range, fail
				// warning: could happen with correct inputs, RRMesh is allowed 
				//  to have preimport indices 1,10,100(out of range!) even when postimport are 0,1,2.
				//  happens with all multiobjects
				//RR_ASSERT(currentVertex<numVerticesMax);
				return;
			}
			mesh->getVertex(triangleVertices[v],avertex[currentVertex]);
			anormal[currentVertex] = triangleNormals.vertex[v].normal;
			atexcoordAmbient[currentVertex] = triangleMapping.uv[v];
			if(hasDiffuseMap)
				atexcoordDiffuse[currentVertex] = diffuseUv[v];
			if(hasEmissiveMap)
				atexcoordEmissive[currentVertex] = emissiveUv[v];
		}
		// generate facegroups
		faceGroups[faceGroups.size()-1].numIndices += 3;
	}
	initedOk = true;

#ifdef USE_VBO
#define CREATE_VBO(array, elementType, vboType, vboId) \
	{ vboId = 0; if(array) { \
		glGenBuffers(1,&vboId); \
		glBindBuffer(GL_ARRAY_BUFFER, vboId); \
		glBufferData(GL_ARRAY_BUFFER, numVertices*sizeof(rr::elementType), array, vboType); \
		SAFE_DELETE_ARRAY(array); } }

	CREATE_VBO(avertex,RRVec3,GL_STATIC_DRAW,vertexVBO);
	CREATE_VBO(anormal,RRVec3,GL_STATIC_DRAW,normalVBO);
	CREATE_VBO(atexcoordDiffuse,RRVec2,GL_STATIC_DRAW,texcoordDiffuseVBO);
	CREATE_VBO(atexcoordEmissive,RRVec2,GL_STATIC_DRAW,texcoordEmissiveVBO);
	CREATE_VBO(atexcoordAmbient,RRVec2,GL_STATIC_DRAW,texcoordAmbientVBO);
	//CREATE_VBO(atexcoordForced2D,RRVec2,GL_STATIC_DRAW,texcoordForced2DVBO);
	//CREATE_VBO(alightIndirectVcolor,RRVec3,GL_STATIC_DRAW,lightIndirectVcolorVBO);
#endif
}

ObjectBuffers::~ObjectBuffers()
{
#ifdef USE_VBO
	// VBOs
	//glDeleteBuffers(1,&lightIndirectVcolorVBO);
	//glDeleteBuffers(1,&texcoordForced2DVBO);
	glDeleteBuffers(1,&texcoordAmbientVBO);
	glDeleteBuffers(1,&texcoordEmissiveVBO);
	glDeleteBuffers(1,&texcoordDiffuseVBO);
	glDeleteBuffers(1,&normalVBO);
	glDeleteBuffers(1,&vertexVBO);
#endif

	// arrays
	delete alightIndirectVcolor;
	delete[] atexcoordForced2D;
	delete[] atexcoordAmbient;
	delete[] atexcoordEmissive;
	delete[] atexcoordDiffuse;
	delete[] anormal;
	delete[] avertex;
	delete[] indices;
}

bool ObjectBuffers::inited()
{
	return initedOk;
}

GLint getBufferNumComponents(rr::RRBuffer* buffer)
{
	switch(buffer->getFormat())
	{
		case rr::BF_RGB:
		case rr::BF_RGBF:
			return 3;
		case rr::BF_RGBA:
		case rr::BF_RGBAF:
			return 4;
		default:
			return 1;
	}
}

GLenum getBufferComponentType(rr::RRBuffer* buffer)
{
	switch(buffer->getFormat())
	{
		case rr::BF_RGB:
		case rr::BF_RGBA:
			return GL_UNSIGNED_BYTE;
		case rr::BF_RGBF:
		case rr::BF_RGBAF:
			return GL_FLOAT;
		default:
			return GL_UNSIGNED_BYTE;
	}
}

void ObjectBuffers::render(RendererOfRRObject::Params& params, unsigned solutionVersion)
{
	RR_ASSERT(initedOk);
	if(!initedOk) return;
#ifdef USE_VBO
#define BIND_VBO(glName,floats,myName) glBindBuffer(GL_ARRAY_BUFFER_ARB, myName##VBO); gl##glName##Pointer(floats, GL_FLOAT, 0, 0); glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
#define BIND_VBO2(glName,floats,myName) glBindBuffer(GL_ARRAY_BUFFER_ARB, myName##VBO); gl##glName##Pointer(GL_FLOAT, 0, 0); glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
#else
#define BIND_VBO(glName,floats,myName) gl##glName##Pointer(floats, GL_FLOAT, 0, &a##myName[0].x);
#define BIND_VBO2(glName,floats,myName) gl##glName##Pointer(GL_FLOAT, 0, &a##myName[0].x);
#endif
	// set vertices
	BIND_VBO(Vertex,3,vertex);
	glEnableClientState(GL_VERTEX_ARRAY);
	// set normals
	bool setNormals = params.renderedChannels.LIGHT_DIRECT || params.renderedChannels.LIGHT_INDIRECT_ENV || params.renderedChannels.NORMALS;
	if(setNormals)
	{
		BIND_VBO2(Normal,3,normal);
		glEnableClientState(GL_NORMAL_ARRAY);
	}
	// manage blending
	bool blendKnown = false;
	bool blendEnabled = false;
	// set indirect illumination vertices
	if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		if(indices)
		{
			if(params.indirectIlluminationSource==RendererOfRRObject::SOLVER)
			{
				// INDEXED FROM SOLVER
				// should never get here, must be handled by RendererOfRRObject
				RR_ASSERT(0);
			}
			else
			if(params.indirectIlluminationSource==RendererOfRRObject::BUFFERS && params.availableIndirectIlluminationVColors)
			{
				// INDEXED FROM VBUFFER
				// use vertex buffer precomputed by RRDynamicSolver
				// indirectIllumination has vertices merged according to RRObject, can't be used with non-indexed trilist, needs indexed trilist
				unsigned bufferSize = params.availableIndirectIlluminationVColors->getWidth();
				RR_ASSERT(numVertices<=bufferSize); // indirectIllumination buffer must be of the same size (or bigger) as our vertex buffer. It's bigger if last vertices in original vertex order are unused (it happens in .bsp).
				glEnableClientState(GL_COLOR_ARRAY);
				glColorPointer(
					getBufferNumComponents(params.availableIndirectIlluminationVColors),
					getBufferComponentType(params.availableIndirectIlluminationVColors),
					0, params.availableIndirectIlluminationVColors->lock(rr::BL_READ));
				if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR2)
				{
					if(params.availableIndirectIlluminationVColors2)
					{
						unsigned bufferSize2 = params.availableIndirectIlluminationVColors2->getWidth();
						RR_ASSERT(bufferSize2==bufferSize); // indirectIllumination buffer must be of the same size (or bigger) as our vertex buffer. It's bigger if last vertices in original vertex order are unused (it happens in .bsp).
						glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
						glSecondaryColorPointer(
							getBufferNumComponents(params.availableIndirectIlluminationVColors2),
							getBufferComponentType(params.availableIndirectIlluminationVColors2),
							0, (GLvoid*)params.availableIndirectIlluminationVColors2->lock(rr::BL_READ));
					}
					else
					{
						RR_ASSERT(0); // render of vertex buffer requested, but vertex buffer not set
						glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
						glSecondaryColorPointer(
							getBufferNumComponents(alightIndirectVcolor),
							getBufferComponentType(alightIndirectVcolor),
							0, (GLvoid*)alightIndirectVcolor->lock(rr::BL_READ));
					}
				}
			}
			else
			{
				// INDEXED FROM VBUFFER THAT IS NULL
				// scene will be rendered without indirect illumination
				LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Render of indirect illumination buffer requested, but buffer is NULL.\n"));
				glDisableClientState(GL_COLOR_ARRAY);
				glColor3b(0,0,0);
			}
		}
		else
		{
			if(params.indirectIlluminationSource==RendererOfRRObject::SOLVER)
			{
				// NON-INDEXED FROM SOLVER
				if(params.scene)
				{
					// fill our own vertex buffer
					// optimization:
					//  remember params used at alightIndirectVcolor filling and refill it only when params change
					if(solutionVersion!=lightIndirectVcolorVersion || params.firstCapturedTriangle!=lightIndirectVcolorFirst || params.lastCapturedTrianglePlus1!=lightIndirectVcolorLastPlus1)
					{
						lightIndirectVcolorFirst = params.firstCapturedTriangle;
						lightIndirectVcolorLastPlus1 = params.lastCapturedTrianglePlus1;
						lightIndirectVcolorVersion = solutionVersion;

						//rr::RRReportInterval report(rr::INF3,"Updating private vertex buffers of renderer...\n");
						// refill
						params.scene->updateLightmap(-1,alightIndirectVcolor,NULL,NULL,NULL);
					}
				}
				else
				{
					// solver not set, but indirect illumination requested
					// -> scene will be rendered with random indirect illumination
					rr::RRReporter::report(rr::WARN,"Rendering object, indirect lighting is not available.\n");
				}
			}
			else
			{
				// NON-INDEXED FROM VBUFFER
				// should never get here, must be handled by RendererOfRRObject
				RR_ASSERT(0);
			}

			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(
				getBufferNumComponents(alightIndirectVcolor),
				getBufferComponentType(alightIndirectVcolor),
				0, (GLvoid*)alightIndirectVcolor->lock(rr::BL_READ));
		}
	}
	// set indirect illumination texcoords + map
	if(params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_LIGHT_INDIRECT);
		BIND_VBO(TexCoord,2,texcoordAmbient);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
		getTexture(params.availableIndirectIlluminationMap)->bindTexture();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// set indirect illumination map2
	if(params.renderedChannels.LIGHT_INDIRECT_MAP2 && params.availableIndirectIlluminationMap2)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT2);
		getTexture(params.availableIndirectIlluminationMap2)->bindTexture();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// set 2d_position texcoords
	if(params.renderedChannels.FORCE_2D_POSITION)
	{
		//!!! possible optimization
		// remember params used at atexcoordForced2D filling
		//  and refill it only when params change

		// this should not be executed in every frame, generated texcoords change rarely
		RR_ASSERT(!indices); // needs non-indexed trilist
		//for(unsigned i=0;i<numVertices;i++) // for all capture textures, probably not necessary
#pragma omp parallel for schedule(static,1)
		for(int i=params.firstCapturedTriangle*3;(unsigned)i<3*params.lastCapturedTrianglePlus1;i++) // only for our capture texture
		{
			params.generateForcedUv->generateData(i/3, i%3, &atexcoordForced2D[i].x, sizeof(atexcoordForced2D[i]));
		}
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, &atexcoordForced2D[0].x);
	}
	// set material diffuse texcoords
	if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_DIFFUSE);
		BIND_VBO(TexCoord,2,texcoordDiffuse);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// set material emissive texcoords
	if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_EMISSIVE);
		BIND_VBO(TexCoord,2,texcoordEmissive);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// render facegroups (facegroups differ by material)
	if(params.renderedChannels.MATERIAL_DIFFUSE_VCOLOR || params.renderedChannels.MATERIAL_DIFFUSE_MAP || params.renderedChannels.MATERIAL_EMISSIVE_VCOLOR || params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		for(unsigned fg=0;fg<faceGroups.size();fg++)
		{
			unsigned firstIndex = faceGroups[fg].firstIndex;
			int numIndices = faceGroups[fg].numIndices; //!!! we are in scope of ObjectBuffers::numIndices, local variable should be renamed or naming convention changed
			// limit rendered indices to capture range
			numIndices = MIN(firstIndex+numIndices,3*params.lastCapturedTrianglePlus1) - MAX(firstIndex,3*params.firstCapturedTriangle);
			firstIndex = MAX(firstIndex,3*params.firstCapturedTriangle);
			if(numIndices>0)
			{
				// set face culling
				if(params.renderedChannels.MATERIAL_CULLING)
				{
					if(faceGroups[fg].renderFront && faceGroups[fg].renderBack)
					{
						glDisable(GL_CULL_FACE);
					}
					else
					{
						glEnable(GL_CULL_FACE);
						glCullFace(faceGroups[fg].renderFront?GL_BACK:( faceGroups[fg].renderBack?GL_FRONT:GL_FRONT_AND_BACK ));
					}
				}

				// set diffuse color
				if(params.renderedChannels.MATERIAL_DIFFUSE_VCOLOR)
				{
					glSecondaryColor3fv(&faceGroups[fg].diffuseColor[0]);
				}
				// set diffuse map
				if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
				{
					glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_DIFFUSE);
					rr::RRBuffer* tex = faceGroups[fg].diffuseTexture;
					if(tex)
					{
						getTexture(tex)->bindTexture();
					}
					else
					{
						LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"RRRendererOfRRObject: Texturing requested, but diffuse texture not available, expect incorrect render.\n"));
					}
					// set blending
					if(params.renderedChannels.MATERIAL_BLENDING)
					{
						bool transparency = faceGroups[fg].transparency>0;
						if(transparency!=blendEnabled || !blendKnown)
						{
							if(transparency)
							{
								glEnable(GL_BLEND);
								// current blendfunc is used, caller is responsible for setting it
							}
							else
							{
								glDisable(GL_BLEND);
							}
							blendKnown = true;
							blendEnabled = transparency;
						}
					}
				}
				// set emissive color
				if(params.renderedChannels.MATERIAL_EMISSIVE_VCOLOR)
				{
					glMultiTexCoord3fv(MULTITEXCOORD_MATERIAL_EMISSIVE_VCOLOR,&faceGroups[fg].emissiveColor[0]); //!!! vertex shader gets zero (X1950)
				}
				// set emissive map
				if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
				{
					glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_EMISSIVE);
					rr::RRBuffer* tex = faceGroups[fg].emissiveTexture;
					if(tex)
					{
						getTexture(tex)->bindTexture();
					}
					else
					{
						LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"RRRendererOfRRObject: Texturing requested, but emissive texture not available, expect incorrect render.\n"));
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
		// render all at once
		// (but only captured range)
		unsigned firstIndex = 3*params.firstCapturedTriangle;
		int numIndices = 3*(params.lastCapturedTrianglePlus1-params.firstCapturedTriangle);
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
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset material emissive texcoords
	if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_EMISSIVE);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset 2d_position texcoords
	if(params.renderedChannels.FORCE_2D_POSITION)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset indirect illumination map2
	if(params.renderedChannels.LIGHT_INDIRECT_MAP2 && params.availableIndirectIlluminationMap2)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT2);
		glBindTexture(GL_TEXTURE_2D,0);
	}
	// unset indirect illumination texcoords + map
	if(params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_LIGHT_INDIRECT);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
		glBindTexture(GL_TEXTURE_2D,0);
	}
	if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR2)
	{
		glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
		if(indices && params.availableIndirectIlluminationVColors2) params.availableIndirectIlluminationVColors2->unlock();
	}
	// unset indirect illumination colors
	if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		glDisableClientState(GL_COLOR_ARRAY);
		if(indices && params.availableIndirectIlluminationVColors) params.availableIndirectIlluminationVColors->unlock();
	}
	// unset normals
	if(setNormals)
	{
		glDisableClientState(GL_NORMAL_ARRAY);
	}
	// unset vertices
	glDisableClientState(GL_VERTEX_ARRAY);
}


}; // namespace
