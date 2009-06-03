// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
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
// 1.using advanced features may trigger driver bugs, better avoid VBO for now
//   as I haven't registered any significant speedup in real world scenarios
//                lmark2008 fps in first 3 scenes
//                8800 182.50
//   array+dlist  300 450..400 300 234MB
//   array        250 420..360 300 227MB
//   vbo+dlist    300 450..400 300 229MB
//   vbo          290 450..400 290 222MB
// 2.it seems that mixed VBO+vertex array renders don't work correctly(8800+17x.xx),
//   try full VBO render (predelat na VBO i indirect_vcolor, forced_2d)
//   (VBO Lightsmark looks ok, but VBO koupelna4 in SceneViewer is buggy)

namespace rr_gl
{

ObjectBuffers* ObjectBuffers::create(const rr::RRObject* object, bool indexed, bool& containsNonBlended, bool& containsBlended)
{
	if (!object) return NULL;

	// ObjectBuffers with indexed=true fail if object has preimport vertex indices so high
	// that vertex buffer with such indices can't be reasonably created.
	// It is case of multiobject.
	// Here we quickly detect whether we got multiobject and return NULL.
	// It is optional, other more reliable detection would catch this problem deeper inside init()
	// (however it would misleadingly report not enough memory)
	unsigned numTriangles = object->getCollider()->getMesh()->getNumTriangles();
	if (!numTriangles || (indexed && object->getCollider()->getMesh()->getPreImportTriangle(numTriangles-1).object))
		return NULL;

	ObjectBuffers* ob = NULL;
	try
	{
		// does lots of allocations, may fail here
		ob = new ObjectBuffers;
		ob->init(object,indexed);
	}
	catch(std::bad_alloc e)
	{
		// delete what was allocated
		RR_SAFE_DELETE(ob);
		rr::RRReporter::report(rr::WARN,"Not enough memory, using emergency rendering path, might fail.\n");
	}
	catch(...)
	{
		RR_SAFE_DELETE(ob);
	}
	if (ob)
	{
		containsNonBlended = ob->containsNonBlended;
		containsBlended = ob->containsBlended;
	}
	return ob;
}

ObjectBuffers::ObjectBuffers()
{
	// clear all pointers
	avertex = NULL;
	anormal = NULL;
	atexcoordDiffuse = NULL;
	atexcoordEmissive = NULL;
	atexcoordTransparency = NULL;
	atexcoordAmbient = NULL;
	atexcoordForced2D = NULL;
	alightIndirectVcolor = NULL;
	indices = NULL;
}

// one time initialization
void ObjectBuffers::init(const rr::RRObject* object, bool indexed)
{
	containsNonBlended = false;
	containsBlended = false;
	const rr::RRMesh* mesh = object->getCollider()->getMesh();
	unsigned numTriangles = mesh->getNumTriangles();
	unsigned numVerticesExpected = indexed
		? mesh->getNumPreImportVertices() // indexed (when rendering 1object without force_2d)
		: 3*numTriangles; // nonindexed (when rendering multiobject or force_2d)
	numIndices = 0;
	indices = NULL;
	if (indexed)
	{
		indices = new unsigned[3*numTriangles]; // exact, we always have 3*numTriangles indices
	}
	numVertices = 0;
	#define NEW_ARRAY(arr,type) {arr = new rr::type[numVerticesExpected]; memset(arr,0,sizeof(rr::type)*numVerticesExpected);}
	NEW_ARRAY(avertex,RRVec3);
	NEW_ARRAY(anormal,RRVec3);

	// work as if all data are present, generate stubs for missing data
	// this way renderer works even with partial data (missing texture, missing texcoord...)
	// (other way would be to rebuild shaders for missing data)

	unsigned hasDiffuseMap = 1;
	if (hasDiffuseMap)
		NEW_ARRAY(atexcoordDiffuse,RRVec2)
	else
		atexcoordDiffuse = NULL;

	unsigned hasEmissiveMap = 1;
	if (hasEmissiveMap)
		NEW_ARRAY(atexcoordEmissive,RRVec2)
	else
		atexcoordEmissive = NULL;

	unsigned hasTransparencyMap = 1;
	if (hasTransparencyMap)
		NEW_ARRAY(atexcoordTransparency,RRVec2)
	else
		atexcoordTransparency = NULL;

	NEW_ARRAY(atexcoordForced2D,RRVec2);
	NEW_ARRAY(atexcoordAmbient,RRVec2);
	alightIndirectVcolor = rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVerticesExpected,1,1,rr::BF_RGBF,false,NULL);

	#undef NEW_ARRAY
	const rr::RRMaterial* previousMaterial = NULL;
	for (unsigned t=0;t<numTriangles;t++)
	{
		// read triangle params
		const rr::RRMaterial* material = object->getTriangleMaterial(t,NULL,NULL);
		rr::RRMesh::Triangle triangleVertices;
		mesh->getTriangle(t,triangleVertices);
		rr::RRMesh::TriangleNormals triangleNormals;
		mesh->getTriangleNormals(t,triangleNormals);
		rr::RRMesh::TriangleMapping triangleMappingLightmap;
		mesh->getTriangleMapping(t,triangleMappingLightmap,material?material->lightmapTexcoord:0);
		rr::RRMesh::TriangleMapping triangleMappingDiffuse;
		mesh->getTriangleMapping(t,triangleMappingDiffuse,material?material->diffuseReflectance.texcoord:0);
		rr::RRMesh::TriangleMapping triangleMappingEmissive;
		mesh->getTriangleMapping(t,triangleMappingEmissive,material?material->diffuseEmittance.texcoord:0);
		rr::RRMesh::TriangleMapping triangleMappingTransparent;
		mesh->getTriangleMapping(t,triangleMappingTransparent,material?material->specularTransmittance.texcoord:0);
/*		// material change? -> start new facegroup
		// a) rendering into shadowmap, check shadowing flags
		//    -> test t,light,receiver
		//       kdyz pro aspon 1 receiver true, kreslit, tj.vrhat stiny (muze byt nepresne, ale nebudu delat extra shadowmapu pro kazdy receiver)
		//       kdyz pro aspon 1 light true, kreslit, tj.vrhat stiny (muze byt nepresne, ale nebudu delat extra ObjectBuffers pro kazde svetlo)
		// b) rendering lit, check lighting flags
		//    -> test t,light,NULL
		//       kdyz pro aspon 1 light true, kreslit, tj.osvitit (muze byt nepresne, ale nebudu delat extra ObjectBuffers pro kazde svetlo)
		const rr::RRMaterial* material;
		if (!params.light // rendering indirect, so no skipping, everything is rendered
			|| !params.renderingShadowCasters) // accumulating lit renders, so skipping render=disabling lighting
		{
			material = object->getTriangleMaterial(t,params.light,NULL);
		}
		else
		{
			// rendering into shadowmap, so skipping render=disabling shadow
			// kdyz pro aspon 1 receiver true, kreslit, tj.vrhat stiny (muze byt nepresne, ale nebudu delat extra shadowmapu pro kazdy receiver)
			for (unsigned i=0;i<params.scene->getNumObjects();i++)
			{
				if (material = object->getTriangleMaterial(t,params.light,params.scene->getObject(i)))
					break;
			}
		}
		if (!material) continue; // skip rendering triangles without material
*/
		if (!t || material!=previousMaterial)
		{
			FaceGroup fg;
			if (indexed)
			{
				fg.firstIndex = numIndices;
			}
			else
			{
				fg.firstIndex = numVertices;
			}
			if (!material) LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Rendering at least one face with NULL material.\n"));
			fg.numIndices = 0;
			if (material)
				fg.material = *material;
			else
				fg.material.reset(false);
			if (fg.needsBlend())
				containsBlended = true;
			else
				containsNonBlended = true;

			// create 1x1 stub textures
			fg.material.createTexturesFromColors();

			// remember created textures for destruction time
			if (!material || !material->diffuseReflectance.texture) tempTextures.push_back(fg.material.diffuseReflectance.texture);
			if (!material || !material->specularReflectance.texture) tempTextures.push_back(fg.material.specularReflectance.texture);
			if (!material || !material->diffuseEmittance.texture) tempTextures.push_back(fg.material.diffuseEmittance.texture);
			if (!material || !material->specularTransmittance.texture) tempTextures.push_back(fg.material.specularTransmittance.texture);

			// preload textures to GPU so we don't do it in display list
			getTexture(fg.material.diffuseReflectance.texture);
			getTexture(fg.material.specularReflectance.texture);
			getTexture(fg.material.diffuseEmittance.texture);
			getTexture(fg.material.specularTransmittance.texture);

			faceGroups.push_back(fg);
			previousMaterial = material;
		}
		// generate vertices and indices into buffers
		for (unsigned v=0;v<3;v++)
		{
			unsigned currentVertex;
			if (indexed)
			{
				//numVertices = triangleVertices[v];

				// force original RRObject vertex order
				// why?
				//  RRDynamicSolver generates vertex buffers for original vertex order.
				//  to render them, whole mesh must be in original vertex order
				// use preimport index, because of e.g. optimizations in RRObjectMulti
				currentVertex = mesh->getPreImportVertex(triangleVertices[v],t).index;
				RR_ASSERT(currentVertex<numVerticesExpected);
				RR_ASSERT(numIndices<3*numTriangles);
				indices[numIndices++] = currentVertex;
				numVertices = RR_MAX(numVertices,currentVertex+1);
			}
			else
			{
				currentVertex = numVertices;
				numVertices++;
			}
			if (currentVertex>=numVerticesExpected)
			{
				// preimport vertex number is out of range, fail
				// warning: could happen with correct inputs, RRMesh is allowed 
				//  to have preimport indices 1,10,100(out of range!) even when postimport are 0,1,2.
				//  happens with all multiobjects
				// Multiobjects should not get here - catched by test in create().
				rr::RRReporter::report(rr::WARN,"Object has strange vertex numbers, aborting render.\n");
				throw 1;
			}
			mesh->getVertex(triangleVertices[v],avertex[currentVertex]);
			anormal[currentVertex] = triangleNormals.vertex[v].normal;
			atexcoordAmbient[currentVertex] = triangleMappingLightmap.uv[v];
			if (hasDiffuseMap)
				atexcoordDiffuse[currentVertex] = triangleMappingDiffuse.uv[v];
			if (hasEmissiveMap)
				atexcoordEmissive[currentVertex] = triangleMappingEmissive.uv[v];
			if (hasTransparencyMap)
				atexcoordTransparency[currentVertex] = triangleMappingTransparent.uv[v];
			unsigned triCountX = DDI_TRIANGLES_X; // number of triangles in one row
			unsigned triCountY = RR_MIN(DDI_TRIANGLES_MAX_Y,(numTriangles%(triCountX*DDI_TRIANGLES_MAX_Y)+triCountX-1)/triCountX); // number of triangles in one column
			atexcoordForced2D[currentVertex][0] = (( t%triCountX           )+((v==2)?1:0)-triCountX*0.5f+0.05f)/(triCountX*0.5f); // +0.05f makes triangle area larger [in 4x4, from 6 to 10 pixels]
			atexcoordForced2D[currentVertex][1] = (((t/triCountX)%triCountY)+((v==0)?1:0)-triCountY*0.5f+0.05f)/(triCountY*0.5f);
		}
		// generate facegroups
		faceGroups[faceGroups.size()-1].numIndices += 3;
	}
#ifdef USE_VBO
#define COPY_ARRAY_TO_VBO(name) \
	{ if (a##name) { \
		glBindBuffer(GL_ARRAY_BUFFER, VBO[VBO_##name]); \
		glBufferData(GL_ARRAY_BUFFER, numVertices*sizeof(*a##name), a##name, GL_STATIC_DRAW); \
		RR_SAFE_DELETE_ARRAY(a##name); } }

	glGenBuffers(VBO_COUNT,VBO);
	COPY_ARRAY_TO_VBO(vertex);
	COPY_ARRAY_TO_VBO(normal);
	COPY_ARRAY_TO_VBO(texcoordDiffuse);
	COPY_ARRAY_TO_VBO(texcoordEmissive);
	COPY_ARRAY_TO_VBO(texcoordTransparency);
	COPY_ARRAY_TO_VBO(texcoordAmbient);
	COPY_ARRAY_TO_VBO(texcoordForced2D);
	glBindBuffer(GL_ARRAY_BUFFER,0);
#endif
	if (indexed)
		for (unsigned i=0;i<numTriangles*3;i++)
			RR_ASSERT(indices[i]<numVerticesExpected);
}

ObjectBuffers::~ObjectBuffers()
{
#ifdef USE_VBO
	// VBOs
	glDeleteBuffers(VBO_COUNT,VBO);
#endif

	// arrays
	delete alightIndirectVcolor;
	delete[] atexcoordForced2D;
	delete[] atexcoordAmbient;
	delete[] atexcoordTransparency;
	delete[] atexcoordEmissive;
	delete[] atexcoordDiffuse;
	delete[] anormal;
	delete[] avertex;
	delete[] indices;

	// temp 1x1 textures
	for (unsigned i=0;i<tempTextures.size();i++) delete tempTextures[i];
}

GLint getBufferNumComponents(const rr::RRBuffer* buffer)
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

GLenum getBufferComponentType(const rr::RRBuffer* buffer)
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

static void copyBufferToVBO(rr::RRBuffer* buffer, unsigned numVerticesToCopy, unsigned VBO)
{
	RR_ASSERT(buffer);
	RR_ASSERT(buffer->getType()==rr::BT_VERTEX_BUFFER);
	unsigned bufferWidth = buffer->getWidth();
	RR_ASSERT(numVerticesToCopy<=bufferWidth);
#ifdef USE_VBO
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, numVerticesToCopy*buffer->getElementBits()/8, buffer->lock(rr::BL_READ), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	buffer->unlock();
#endif
}

void ObjectBuffers::render(RendererOfRRObject::Params& params, unsigned solutionVersion)
{
#ifdef USE_VBO
	#define BIND_VBO2(glName,myName)           glBindBuffer(GL_ARRAY_BUFFER, VBO[VBO_##myName]); gl##glName##Pointer(GL_FLOAT, 0, 0); glBindBuffer(GL_ARRAY_BUFFER, 0);
	#define BIND_VBO3(glName,numFloats,myName) glBindBuffer(GL_ARRAY_BUFFER, VBO[VBO_##myName]); gl##glName##Pointer(numFloats, GL_FLOAT, 0, 0); glBindBuffer(GL_ARRAY_BUFFER, 0);
	#define BIND_BUFFER(glName,buffer,myName)  glBindBuffer(GL_ARRAY_BUFFER, VBO[VBO_##myName]); gl##glName##Pointer(getBufferNumComponents(buffer), getBufferComponentType(buffer), 0, 0); glBindBuffer(GL_ARRAY_BUFFER, 0);
#else
	#define BIND_VBO2(glName,myName)           gl##glName##Pointer(GL_FLOAT, 0, &a##myName[0].x);
	#define BIND_VBO3(glName,numFloats,myName) gl##glName##Pointer(numFloats, GL_FLOAT, 0, &a##myName[0].x);
	#define BIND_BUFFER(glName,buffer,myName)  gl##glName##Pointer(getBufferNumComponents(buffer), getBufferComponentType(buffer), 0, buffer->lock(rr::BL_READ)); buffer->unlock();
#endif
	// set vertices
	BIND_VBO3(Vertex,3,vertex);
	glEnableClientState(GL_VERTEX_ARRAY);
	// set normals
	bool setNormals = params.renderedChannels.NORMALS || params.renderedChannels.LIGHT_DIRECT;
	if (setNormals)
	{
		BIND_VBO2(Normal,normal);
		glEnableClientState(GL_NORMAL_ARRAY);
	}
	// manage blending
	bool blendKnown = false;
	bool blendEnabled = false;
	// set indirect illumination vertices
	if (params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		if (indices)
		{
			if (params.indirectIlluminationSource==RendererOfRRObject::SOLVER)
			{
				// INDEXED FROM SOLVER
				// should never get here, must be handled by RendererOfRRObject
				RR_ASSERT(0);
			}
			else
			if (params.indirectIlluminationSource==RendererOfRRObject::BUFFERS && params.availableIndirectIlluminationVColors)
			{
				// INDEXED FROM VBUFFER
				// use vertex buffer precomputed by RRDynamicSolver
				// indirectIllumination has vertices merged according to RRObject, can't be used with non-indexed trilist, needs indexed trilist
				copyBufferToVBO(params.availableIndirectIlluminationVColors,numVertices,VBO[VBO_lightIndirectVcolor]);
				glEnableClientState(GL_COLOR_ARRAY);
				BIND_BUFFER(Color,params.availableIndirectIlluminationVColors,lightIndirectVcolor);
				if (params.renderedChannels.LIGHT_INDIRECT_VCOLOR2)
				{
					if (params.availableIndirectIlluminationVColors2)
					{
						copyBufferToVBO(params.availableIndirectIlluminationVColors2,numVertices,VBO[VBO_lightIndirectVcolor2]);
						glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
						BIND_BUFFER(SecondaryColor,params.availableIndirectIlluminationVColors2,lightIndirectVcolor2);
					}
					else
					{
						LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Render of indirect illumination buffer blend requested, but second buffer is NULL.\n"));
						RR_ASSERT(0); // render of vertex buffer requested, but vertex buffer not set
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
			if (params.indirectIlluminationSource==RendererOfRRObject::SOLVER)
			{
				// NON-INDEXED FROM SOLVER
				if (params.scene)
				{
					// fill our own vertex buffer
					if (solutionVersion!=lightIndirectVcolorVersion)
					{
						lightIndirectVcolorVersion = solutionVersion;

						//rr::RRReportInterval report(rr::INF3,"Updating private vertex buffers of renderer...\n");
						// refill
						params.scene->updateLightmap(-1,alightIndirectVcolor,NULL,NULL,NULL);
						copyBufferToVBO(alightIndirectVcolor,numVertices,VBO[VBO_lightIndirectVcolor]);
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

#ifdef USE_VBO
			BIND_VBO3(Color,3,lightIndirectVcolor);
#else
			glColorPointer(
				getBufferNumComponents(alightIndirectVcolor),
				getBufferComponentType(alightIndirectVcolor),
				0, (GLvoid*)alightIndirectVcolor->lock(rr::BL_READ));
#endif
			glEnableClientState(GL_COLOR_ARRAY);
		}
	}
	// set indirect illumination texcoords + map (lightmap or light detail map)
	if ((params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap) || (params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP && params.availableIndirectIlluminationLDMap))
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_LIGHT_INDIRECT);
		BIND_VBO3(TexCoord,2,texcoordAmbient);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
		if (params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap)
			getTexture(params.availableIndirectIlluminationMap)->bindTexture(); // bind lightmap
		else
			getTexture(params.availableIndirectIlluminationLDMap)->bindTexture(); // bind light detail map
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// set indirect illumination map2
	if (params.renderedChannels.LIGHT_INDIRECT_MAP2 && params.availableIndirectIlluminationMap2)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT2);
		getTexture(params.availableIndirectIlluminationMap2)->bindTexture();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// set 2d_position texcoords
	if (params.renderedChannels.FORCE_2D_POSITION)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D);
		BIND_VBO3(TexCoord,2,texcoordForced2D);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// set material diffuse texcoords
	if (params.renderedChannels.MATERIAL_DIFFUSE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_DIFFUSE);
		BIND_VBO3(TexCoord,2,texcoordDiffuse);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// set material emissive texcoords
	if (params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_EMISSIVE);
		BIND_VBO3(TexCoord,2,texcoordEmissive);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// set material transparency texcoords
	if (params.renderedChannels.MATERIAL_TRANSPARENCY_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_TRANSPARENCY);
		BIND_VBO3(TexCoord,2,texcoordTransparency);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// render facegroups (facegroups differ by material)
	if (params.renderedChannels.MATERIAL_DIFFUSE_CONST || params.renderedChannels.MATERIAL_DIFFUSE_MAP
		|| params.renderedChannels.MATERIAL_SPECULAR_CONST
		|| params.renderedChannels.MATERIAL_EMISSIVE_CONST || params.renderedChannels.MATERIAL_EMISSIVE_MAP
		|| params.renderedChannels.MATERIAL_TRANSPARENCY_CONST || params.renderedChannels.MATERIAL_TRANSPARENCY_MAP
		|| params.renderedChannels.MATERIAL_CULLING
		|| (containsNonBlended && containsBlended && params.renderNonBlended!=params.renderBlended))
	{
		for (unsigned fg=0;fg<faceGroups.size();fg++) if ((faceGroups[fg].needsBlend() && params.renderBlended) || (!faceGroups[fg].needsBlend() && params.renderNonBlended))
		{
			// skip whole facegroup when alpha keying with constant alpha below 0.5
			// GPU would do the same for all pixels, this is faster
			if (params.renderedChannels.MATERIAL_TRANSPARENCY_CONST && params.renderedChannels.MATERIAL_TRANSPARENCY_KEYING && faceGroups[fg].material.specularTransmittance.color.avg()>0.5f)
			{
				continue;
			}

			unsigned firstIndex = faceGroups[fg].firstIndex;
			int numIndices = faceGroups[fg].numIndices; //!!! we are in scope of ObjectBuffers::numIndices, local variable should be renamed or naming convention changed
			// limit rendered indices to capture range
			numIndices = RR_MIN(firstIndex+numIndices,3*params.lastCapturedTrianglePlus1) - RR_MAX(firstIndex,3*params.firstCapturedTriangle);
			firstIndex = RR_MAX(firstIndex,3*params.firstCapturedTriangle);
			if (numIndices>0)
			{
				// set face culling
				if (params.renderedChannels.MATERIAL_CULLING)
				{
					bool renderFront = faceGroups[fg].material.sideBits[0].renderFrom;
					bool renderBack = faceGroups[fg].material.sideBits[1].renderFrom;
					if (renderFront && renderBack)
					{
						glDisable(GL_CULL_FACE);
					}
					else
					{
						glEnable(GL_CULL_FACE);
						glCullFace(renderFront?GL_BACK:( renderBack?GL_FRONT:GL_FRONT_AND_BACK ));
					}
				}

				// set blending
				if (params.renderedChannels.MATERIAL_TRANSPARENCY_KEYING || params.renderedChannels.MATERIAL_TRANSPARENCY_BLENDING)
				{
					bool transparency = faceGroups[fg].material.specularTransmittance.color!=rr::RRVec3(0);
					if (transparency!=blendEnabled || !blendKnown)
					{
						if (transparency)
						{
							if (params.renderedChannels.MATERIAL_TRANSPARENCY_BLENDING)
							{
								// current blendfunc is used, caller is responsible for setting it
								glEnable(GL_BLEND);
							}
							else
							{
								// current alpha func+ref is used, caller is responsible for setting it
								glEnable(GL_ALPHA_TEST); // alpha test is used so we don't have to sort objects
							}
						}
						else
						{
							if (params.renderedChannels.MATERIAL_TRANSPARENCY_BLENDING)
							{
								glDisable(GL_BLEND);
							}
							else
							{
								glDisable(GL_ALPHA_TEST);
							}
						}
						blendKnown = true;
						blendEnabled = transparency;
					}
				}
				// set material
				params.renderedChannels.useMaterial(params.program,&faceGroups[fg].material);
				// render one facegroup
				if (indices)
				{
					for (unsigned i=firstIndex;i<firstIndex+numIndices;i++) RR_ASSERT(indices[i]<numVertices);
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
		if (indices)
		{
			for (unsigned i=firstIndex;i<firstIndex+numIndices;i++) RR_ASSERT(indices[i]<numVertices);
			glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, &indices[firstIndex]);
		}
		else
		{
			glDrawArrays(GL_TRIANGLES, firstIndex, numIndices);
		}
	}
	// unset material diffuse texcoords
	if (params.renderedChannels.MATERIAL_DIFFUSE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset material transparency texcoords
	if (params.renderedChannels.MATERIAL_TRANSPARENCY_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_TRANSPARENCY);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset material emissive texcoords
	if (params.renderedChannels.MATERIAL_EMISSIVE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_EMISSIVE);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset 2d_position texcoords
	if (params.renderedChannels.FORCE_2D_POSITION)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset indirect illumination map2
	if (params.renderedChannels.LIGHT_INDIRECT_MAP2 && params.availableIndirectIlluminationMap2)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT2);
		glBindTexture(GL_TEXTURE_2D,0);
	}
	// unset indirect illumination texcoords + map (lightmap or light detail map)
	if ((params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap) || (params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP && params.availableIndirectIlluminationLDMap))
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_LIGHT_INDIRECT);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
		glBindTexture(GL_TEXTURE_2D,0);
	}
	if (params.renderedChannels.LIGHT_INDIRECT_VCOLOR2)
	{
		glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
		if (indices && params.availableIndirectIlluminationVColors2) params.availableIndirectIlluminationVColors2->unlock();
	}
	// unset indirect illumination colors
	if (params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		glDisableClientState(GL_COLOR_ARRAY);
		if (indices && params.availableIndirectIlluminationVColors) params.availableIndirectIlluminationVColors->unlock();
	}
	// unset normals
	if (setNormals)
	{
		glDisableClientState(GL_NORMAL_ARRAY);
	}
	// unset vertices
	glDisableClientState(GL_VERTEX_ARRAY);
}


}; // namespace
