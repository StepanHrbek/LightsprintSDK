// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <hash_map>
#include <GL/glew.h>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "ObjectBuffers.h"
#include "PreserveState.h"
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif

namespace rr_gl
{

static unsigned g_numVBOs = 0;

//////////////////////////////////////////////////////////////////////////////
//
// MeshArraysVBOs - RRMeshArrays data stored in VBOs for faster rendering

class MeshArraysVBOs : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	MeshArraysVBOs();
	~MeshArraysVBOs();

	//! Updates mesh VBOs.
	//! Must not be called inside display list (may create VBOs).
	//! \param mesh
	//!  VBOs are created/updated for this mesh.
	//! \param indexed
	//!  False = generates triangle list, numVertices == 3*mesh->getNumTriangles().
	//!  True = generates indexed triangle list, numVertices == mesh->getNumVertices(), order specified by postimport vertex numbers
	//! \return
	//!  True if update succeeded or was not necessary. False if update failed.
	bool update(const rr::RRMeshArrays* mesh, bool indexed);

	//! Renders mesh VBOs.
	//! Must not be called inside display list (may create textures).
	void render(RendererOfRRObject::Params& params);

private:
	const void*   createdFromMesh;
	unsigned      createdFromMeshVersion;
	const void*   createdFromLightIndirectBuffer;
	unsigned      createdFromLightIndirectVersion;
	bool          createdIndexed;

	bool          privateMultiobjLightIndirectInited;
	rr::RRBuffer* privateMultiobjLightIndirectBuffer; // allocated during render if render needs it

	enum VBOIndex
	{
		VBO_index, // used only if indexed
		VBO_position,
		VBO_normal,
		VBO_lightIndirectVcolor,
		VBO_lightIndirectVcolor2, // used when blending 2 vbufs together
		VBO_texcoordForced2D,
		VBO_COUNT
	};
	GLuint VBO[VBO_COUNT];
	rr::RRVector<GLuint> texcoordVBO;
};

MeshArraysVBOs::MeshArraysVBOs()
{
	createdFromMesh = NULL;
	createdFromMeshVersion = 0;
	createdFromLightIndirectBuffer = NULL;
	createdFromLightIndirectVersion = UINT_MAX;
	createdIndexed = false;
	privateMultiobjLightIndirectInited = false;
	privateMultiobjLightIndirectBuffer = NULL;
	glGenBuffers(VBO_COUNT,VBO);
	g_numVBOs += VBO_COUNT;
}

MeshArraysVBOs::~MeshArraysVBOs()
{
	delete privateMultiobjLightIndirectBuffer;
	g_numVBOs -= VBO_COUNT;
	for (unsigned i=0;i<texcoordVBO.size();i++)
		if (texcoordVBO[i])
			g_numVBOs--;
	glDeleteBuffers(VBO_COUNT,VBO);
	glDeleteBuffers(texcoordVBO.size(),&texcoordVBO[0]);
}

bool MeshArraysVBOs::update(const rr::RRMeshArrays* _mesh, bool _indexed)
{
	if (!_mesh)
	{
		// should we also delete VBOs?
		return true;
	}

	if (_mesh==createdFromMesh && _mesh->version==createdFromMeshVersion)
	{
		// looks like no change since last update
		return true;
	}
	createdFromMesh = _mesh;
	createdFromMeshVersion = _mesh->version;
	createdIndexed = _indexed;

	try
	{
		#define COPY_ARRAY_TO_VBO(sourceArray,destinationVBO) { if (sourceArray) { \
			glBindBuffer(GL_ARRAY_BUFFER, destinationVBO); \
			glBufferData(GL_ARRAY_BUFFER, _mesh->numVertices*sizeof(*(sourceArray)), sourceArray, GL_STATIC_DRAW); \
			} }
		COPY_ARRAY_TO_VBO(_mesh->position,VBO[VBO_position]);
		COPY_ARRAY_TO_VBO(_mesh->normal,VBO[VBO_normal]);
		if (texcoordVBO.size()<_mesh->texcoord.size())
		{
			texcoordVBO.resize(_mesh->texcoord.size(),0);
		}
		for (unsigned i=0;i<texcoordVBO.size();i++)
		{
			if (i<_mesh->texcoord.size() && _mesh->texcoord[i] && !texcoordVBO[i])
			{
				glGenBuffers(1,&texcoordVBO[i]);
				g_numVBOs++;
			}
			if ((i>=_mesh->texcoord.size() || !_mesh->texcoord[i]) && texcoordVBO[i])
			{
				glDeleteBuffers(1,&texcoordVBO[i]);
				texcoordVBO[i] = 0;
			}
		}
		for (unsigned i=0;i<_mesh->texcoord.size();i++)
		{
			COPY_ARRAY_TO_VBO(_mesh->texcoord[i],texcoordVBO[i]);
		}
		if (!_indexed)
		{
			// calculate mapping for FORCED_2D
			//  We have working space for 256x256 tringles, so scene with 80k triangles must be split to two passes.
			//  We do 256x157,256x157 passes, but 256x256,256x57 would work too.
			//  Here we render to texture, this calculation is repeated in RRDynamicSolverGL::detectDirectIlluminationTo where we read texture back.
			unsigned triCountX = DDI_TRIANGLES_X;
			unsigned triCountYTotal = (_mesh->numTriangles+DDI_TRIANGLES_X-1)/DDI_TRIANGLES_X;
			unsigned numPasses = (triCountYTotal+DDI_TRIANGLES_MAX_Y-1)/DDI_TRIANGLES_MAX_Y;
			unsigned triCountYInOnePass = (triCountYTotal+numPasses-1)/numPasses;
			rr::RRVec2* texcoordForced2D = new rr::RRVec2[_mesh->numVertices];
			for (unsigned t=0;t<_mesh->numTriangles;t++)
				for (unsigned v=0;v<3;v++)
				{
					texcoordForced2D[3*t+v][0] = (( t%triCountX                    )+((v==2)?1:0)-triCountX         *0.5f+0.05f)/(triCountX         *0.5f); // +0.05f makes triangle area larger [in 4x4, from 6 to 10 pixels]
					texcoordForced2D[3*t+v][1] = (((t/triCountX)%triCountYInOnePass)+((v==0)?1:0)-triCountYInOnePass*0.5f+0.05f)/(triCountYInOnePass*0.5f);
				}
			COPY_ARRAY_TO_VBO(texcoordForced2D,VBO[VBO_texcoordForced2D]);
			delete[] texcoordForced2D;
		}
		#undef COPY_ARRAY_TO_VBO
		glBindBuffer(GL_ARRAY_BUFFER,0);

		if (_indexed)
		{
			for (unsigned i=0;i<_mesh->numTriangles;i++)
			{
				RR_ASSERT(_mesh->triangle[i][0]<_mesh->numVertices);
				RR_ASSERT(_mesh->triangle[i][1]<_mesh->numVertices);
				RR_ASSERT(_mesh->triangle[i][2]<_mesh->numVertices);
			}
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO[VBO_index]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, _mesh->numTriangles*sizeof(_mesh->triangle[0]), &_mesh->triangle[0][0], GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
		}
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"MeshArraysVBOs::update() failed.\n");
		return false;
	}
	return true;
}

static GLint getBufferNumComponents(const rr::RRBuffer* buffer)
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

static GLenum getBufferComponentType(const rr::RRBuffer* buffer)
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

static void copyBufferToVBO(rr::RRBuffer* buffer, unsigned VBO)
{
	RR_ASSERT(buffer);
	RR_ASSERT(buffer->getType()==rr::BT_VERTEX_BUFFER);
	RR_ASSERT(VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, buffer->getWidth()*buffer->getElementBits()/8, buffer->lock(rr::BL_READ), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	buffer->unlock();
}

void MeshArraysVBOs::render(RendererOfRRObject::Params& params)
{
	if (!params.object)
	{
		return;
	}

	#define DRAW_ELEMENTS(a,b,c,d)                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO[VBO_index]); glDrawElements(a,b,c,(const GLvoid*)(sizeof(unsigned)*d));                 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	#define BIND_VBO2(glName,myName)           RR_ASSERT(myName); glBindBuffer(GL_ARRAY_BUFFER, myName); gl##glName##Pointer(           GL_FLOAT, 0, 0);                                            glBindBuffer(GL_ARRAY_BUFFER, 0);
	#define BIND_VBO3(glName,numFloats,myName) RR_ASSERT(myName); glBindBuffer(GL_ARRAY_BUFFER, myName); gl##glName##Pointer(numFloats, GL_FLOAT, 0, 0);                                            glBindBuffer(GL_ARRAY_BUFFER, 0);
	#define BIND_BUFFER(glName,buffer,myName)  RR_ASSERT(myName); glBindBuffer(GL_ARRAY_BUFFER, myName); gl##glName##Pointer(getBufferNumComponents(buffer), getBufferComponentType(buffer), 0, 0); glBindBuffer(GL_ARRAY_BUFFER, 0);

	// set vertices
	BIND_VBO3(Vertex,3,VBO[VBO_position]);
	glEnableClientState(GL_VERTEX_ARRAY);
	// set normals
	bool setNormals = params.renderedChannels.NORMALS || params.renderedChannels.LIGHT_DIRECT;
	if (setNormals)
	{
		BIND_VBO2(Normal,VBO[VBO_normal]);
		glEnableClientState(GL_NORMAL_ARRAY);
	}
	// manage blending
	bool blendKnown = false;
	bool blendEnabled = false;
	// set indirect illumination vertices
	if (params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		if (createdIndexed)
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
				bool copyLayerToVBO = createdFromLightIndirectVersion!=params.lightIndirectVersion || createdFromLightIndirectBuffer!=params.availableIndirectIlluminationVColors;
				if (copyLayerToVBO)
				{
					createdFromLightIndirectVersion = params.lightIndirectVersion;
					createdFromLightIndirectBuffer = params.availableIndirectIlluminationVColors;
					copyBufferToVBO(params.availableIndirectIlluminationVColors,VBO[VBO_lightIndirectVcolor]);
				}
				glEnableClientState(GL_COLOR_ARRAY);
				BIND_BUFFER(Color,params.availableIndirectIlluminationVColors,VBO[VBO_lightIndirectVcolor]);
				if (params.renderedChannels.LIGHT_INDIRECT_VCOLOR2)
				{
					if (params.availableIndirectIlluminationVColors2)
					{
						if (copyLayerToVBO)
						{
							copyBufferToVBO(params.availableIndirectIlluminationVColors2,VBO[VBO_lightIndirectVcolor2]);
						}
						glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
						BIND_BUFFER(SecondaryColor,params.availableIndirectIlluminationVColors2,VBO[VBO_lightIndirectVcolor2]);
					}
					else
					{
						RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Render of indirect illumination buffer blend requested, but second buffer is NULL.\n"));
						RR_ASSERT(0); // render of vertex buffer requested, but vertex buffer not set
					}
				}
			}
			else
			{
				// INDEXED FROM VBUFFER THAT IS NULL
				// scene will be rendered without indirect illumination
				RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Render of indirect illumination buffer requested, but buffer is NULL.\n"));
				glDisableClientState(GL_COLOR_ARRAY);
				glColor3b(0,0,0);
			}
		}
		else
		{
			if (!privateMultiobjLightIndirectInited)
			{
				privateMultiobjLightIndirectInited = true;
				privateMultiobjLightIndirectBuffer = rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,params.scene->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles()*3,1,1,rr::BF_RGBF,false,NULL);
				if (!privateMultiobjLightIndirectBuffer)
					rr::RRReporter::report(rr::ERRO,"Failed to alloc temp indirect illumination buffer.\n");
			}
			if (privateMultiobjLightIndirectBuffer)
			{
				if (params.indirectIlluminationSource==RendererOfRRObject::SOLVER)
				{
					// NON-INDEXED FROM SOLVER
					if (params.scene)
					{
						// fill our own vertex buffer
						if (createdFromLightIndirectVersion!=params.lightIndirectVersion)
						{
							createdFromLightIndirectVersion = params.lightIndirectVersion;
							//rr::RRReportInterval report(rr::INF3,"Updating private vertex buffers of renderer...\n");
							params.scene->updateLightmap(-1,privateMultiobjLightIndirectBuffer,NULL,NULL,NULL);
							copyBufferToVBO(privateMultiobjLightIndirectBuffer,VBO[VBO_lightIndirectVcolor]);
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

				glBindBuffer(GL_ARRAY_BUFFER, VBO[VBO_lightIndirectVcolor]);
				glColorPointer(getBufferNumComponents(privateMultiobjLightIndirectBuffer), getBufferComponentType(privateMultiobjLightIndirectBuffer), 0, 0); 
				glBindBuffer(GL_ARRAY_BUFFER, 0);

				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
	}

	// set indirect illumination texcoords + map (lightmap or light detail map)
	if ((params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap) || (params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP && params.availableIndirectIlluminationLDMap))
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
		if (params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap)
			getTexture(params.availableIndirectIlluminationMap,true,false)->bindTexture(); // bind lightmap
		else
			getTexture(params.availableIndirectIlluminationLDMap)->bindTexture(); // bind light detail map
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// set indirect illumination map2
	if (params.renderedChannels.LIGHT_INDIRECT_MAP2 && params.availableIndirectIlluminationMap2)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT2);
		getTexture(params.availableIndirectIlluminationMap2,true,false)->bindTexture();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// set 2d_position texcoords
	if (params.renderedChannels.FORCE_2D_POSITION)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D);
		BIND_VBO3(TexCoord,2,VBO[VBO_texcoordForced2D]);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	// shortcut
	const rr::RRObject::FaceGroups& faceGroups = params.object->faceGroups;

	// detect blending
	bool containsBlended;
	bool containsNonBlended;
	faceGroups.getBlending(containsBlended,containsNonBlended);

	// render facegroups (facegroups differ by material or lightmap)
	if (params.renderedChannels.MATERIAL_DIFFUSE_CONST || params.renderedChannels.MATERIAL_DIFFUSE_MAP
		|| params.renderedChannels.MATERIAL_SPECULAR_CONST
		|| params.renderedChannels.MATERIAL_EMISSIVE_CONST || params.renderedChannels.MATERIAL_EMISSIVE_MAP
		|| params.renderedChannels.MATERIAL_TRANSPARENCY_CONST || params.renderedChannels.MATERIAL_TRANSPARENCY_MAP
		|| params.renderedChannels.MATERIAL_CULLING
		|| (params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap)
		|| (params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP && params.availableIndirectIlluminationLDMap)
		|| (containsNonBlended && containsBlended && params.renderNonBlended!=params.renderBlended))
	{

		// cache uv channel binding
		struct UvChannelBinding
		{
			unsigned boundUvChannel[5]; // indexed by MULTITEXCOORD_XXX
			void bindUvChannel(const rr::RRVector<unsigned>& texcoordVBO, unsigned shaderChannel, unsigned uvChannel, const rr::RRBuffer* buffer)
			{
				RR_ASSERT(shaderChannel<5);
				if (boundUvChannel[shaderChannel]==uvChannel)
				{
					// already set
					return;
				}
				if (!buffer)
				{
					// 1x1 texture is probably used and uv is irrelevant
					return;
				}
				boundUvChannel[shaderChannel] = uvChannel;
				if (uvChannel>=texcoordVBO.size())
				{
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Please use uv channels below 100 (channel %d out of range 0..%d).\n",uvChannel,texcoordVBO.size()-1));
					RR_ASSERT(0);
					return;
				}
				if (texcoordVBO[uvChannel]==0)
				{
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Attempt to bind non existing VBO, uv channel %d.\n",uvChannel));
					return;
				}
				glClientActiveTexture(GL_TEXTURE0+shaderChannel);
				BIND_VBO3(TexCoord,2,texcoordVBO[uvChannel]);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		};
		UvChannelBinding uvChannelBinding = {UINT_MAX,UINT_MAX,UINT_MAX,UINT_MAX,UINT_MAX};

		for (unsigned fg=0,fgFirstIndex=0; fg<faceGroups.size(); fgFirstIndex+=faceGroups[fg].numTriangles*3,fg++)
		{
			// shortcut
			const rr::RRMaterial* material = faceGroups[fg].material;

			if (material)
			if ((material->needsBlending() && params.renderBlended) || (!material->needsBlending() && params.renderNonBlended))
			{
				// skip whole facegroup when alpha keying with constant alpha below 0.5
				// GPU would do the same for all pixels, this is faster
				if (params.renderedChannels.MATERIAL_TRANSPARENCY_CONST && params.renderedChannels.MATERIAL_TRANSPARENCY_KEYING && material->specularTransmittance.color.avg()>0.5f)
				{
					continue;
				}

				unsigned fgSubsetNumIndices = 3*faceGroups[fg].numTriangles;
				unsigned fgSubsetFirstIndex = fgFirstIndex;
				// limit rendered indices to capture range
				fgSubsetNumIndices = RR_MIN(fgSubsetFirstIndex+fgSubsetNumIndices,3*params.lastCapturedTrianglePlus1) - RR_MAX(fgSubsetFirstIndex,3*params.firstCapturedTriangle);
				fgSubsetFirstIndex = RR_MAX(fgSubsetFirstIndex,3*params.firstCapturedTriangle);
				if (fgSubsetNumIndices>0)
				{
					// set face culling
					if (params.renderedChannels.MATERIAL_CULLING)
					{
						bool renderFront = material->sideBits[0].renderFrom;
						bool renderBack = material->sideBits[1].renderFrom;
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
					if (params.renderedChannels.MATERIAL_TRANSPARENCY_BLENDING)
					{
						bool transparency = material->specularTransmittance.color!=rr::RRVec3(0);
						if (transparency!=blendEnabled || !blendKnown)
						{
							if (transparency)
							{
								if (params.renderedChannels.MATERIAL_TRANSPARENCY_BLENDING)
								{
									// current blendfunc is used, caller is responsible for setting it
									glEnable(GL_BLEND);
								}
							}
							else
							{
								if (params.renderedChannels.MATERIAL_TRANSPARENCY_BLENDING)
								{
									glDisable(GL_BLEND);
								}
							}
							blendKnown = true;
							blendEnabled = transparency;
						}
					}

					// set material
					params.renderedChannels.useMaterial(params.program,material);
					if (params.renderedChannels.MATERIAL_DIFFUSE_MAP)
						uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_MATERIAL_DIFFUSE,material->diffuseReflectance.texcoord,material->diffuseReflectance.texture);
					if (params.renderedChannels.MATERIAL_EMISSIVE_MAP)
						uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_MATERIAL_EMISSIVE,material->diffuseEmittance.texcoord,material->diffuseEmittance.texture);
					if (params.renderedChannels.MATERIAL_TRANSPARENCY_MAP)
						uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_MATERIAL_TRANSPARENCY,material->specularTransmittance.texcoord,material->specularTransmittance.texture);
					if ((params.renderedChannels.LIGHT_INDIRECT_MAP && params.availableIndirectIlluminationMap) || (params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP && params.availableIndirectIlluminationLDMap))
						uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_LIGHT_INDIRECT,material->lightmapTexcoord,(const rr::RRBuffer*)1);

					// render one facegroup
					if (createdIndexed)
					{
						DRAW_ELEMENTS(GL_TRIANGLES, fgSubsetNumIndices, GL_UNSIGNED_INT, fgSubsetFirstIndex);
					}
					else
					{
						glDrawArrays(GL_TRIANGLES, fgSubsetFirstIndex, fgSubsetNumIndices);
					}
				}
			}
		}
	}
	else
	{
		// render all at once
		// (but only captured range)
		unsigned objSubsetFirstIndex = 3*params.firstCapturedTriangle;
		unsigned objSubsetNumIndices = 3*(params.lastCapturedTrianglePlus1-params.firstCapturedTriangle);
		if (createdIndexed)
		{
			DRAW_ELEMENTS(GL_TRIANGLES, objSubsetNumIndices, GL_UNSIGNED_INT, objSubsetFirstIndex);
		}
		else
		{
			glDrawArrays(GL_TRIANGLES, objSubsetFirstIndex, objSubsetNumIndices);
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
		if (createdIndexed && params.availableIndirectIlluminationVColors2) params.availableIndirectIlluminationVColors2->unlock();
	}
	// unset indirect illumination colors
	if (params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		glDisableClientState(GL_COLOR_ARRAY);
		if (createdIndexed && params.availableIndirectIlluminationVColors) params.availableIndirectIlluminationVColors->unlock();
	}
	// unset normals
	if (setNormals)
	{
		glDisableClientState(GL_NORMAL_ARRAY);
	}
	// unset vertices
	glDisableClientState(GL_VERTEX_ARRAY);
}


//////////////////////////////////////////////////////////////////////////////
//
// 1x1 textures

typedef stdext::hash_map<unsigned,rr::RRBuffer*> Buffer1x1Cache;
Buffer1x1Cache buffers1x1;

static void bindPropertyTexture(const rr::RRMaterial::Property& property,unsigned index)
{
	rr::RRBuffer* buffer = property.texture;
	if (buffer)
	{
		getTexture(buffer)->bindTexture();
	}
	else
	{
		unsigned color = RR_FLOAT2BYTE(property.color[0])+(RR_FLOAT2BYTE(property.color[1])<<8)+(RR_FLOAT2BYTE(property.color[2])<<16);
		Buffer1x1Cache::iterator i = buffers1x1.find(color);
		if (i!=buffers1x1.end())
		{
			buffer = i->second;
			getTexture(buffer)->bindTexture();
		}
		else
		{
			buffer = rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,(index==2)?rr::BF_RGBA:rr::BF_RGB,true,NULL); // 2 = RGBA
			buffer->setElement(0,rr::RRVec4(property.color,1-property.color.avg()));
			getTexture(buffer,false,false)->reset(false,false);
			buffers1x1[color] = buffer;
		}
	}
}

static void free1x1()
{
	for (Buffer1x1Cache::const_iterator i=buffers1x1.begin();i!=buffers1x1.end();++i)
		delete i->second;
	buffers1x1.clear();
}

unsigned g_numRenderers = 0;


//////////////////////////////////////////////////////////////////////////////
//
// RenderedChannels

void RendererOfRRObject::RenderedChannels::useMaterial(Program* program, const rr::RRMaterial* material)
{
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"useMaterial(): program=NULL\n"));
		return;
	}
	if (!material)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"useMaterial(): material=NULL\n"));
		rr::RRMaterial s_material;
		RR_LIMITED_TIMES(1,s_material.reset(false));
		material = &s_material;
	}
	if (MATERIAL_DIFFUSE_CONST)
	{
		program->sendUniform("materialDiffuseConst",material->diffuseReflectance.color[0],material->diffuseReflectance.color[1],material->diffuseReflectance.color[2],1.0f);
	}

	if (MATERIAL_SPECULAR_CONST)
	{
		program->sendUniform("materialSpecularConst",material->specularReflectance.color[0],material->specularReflectance.color[1],material->specularReflectance.color[2],1.0f);
	}

	if (MATERIAL_EMISSIVE_CONST)
	{
		program->sendUniform("materialEmissiveConst",material->diffuseEmittance.color[0],material->diffuseEmittance.color[1],material->diffuseEmittance.color[2],0.0f);
	}

	if (MATERIAL_TRANSPARENCY_CONST)
	{
		program->sendUniform("materialTransparencyConst",material->specularTransmittance.color[0],material->specularTransmittance.color[1],material->specularTransmittance.color[2],1-material->specularTransmittance.color.avg());
	}

	if (MATERIAL_DIFFUSE_MAP)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_DIFFUSE);
		bindPropertyTexture(material->diffuseReflectance,0);
	}

	if (MATERIAL_EMISSIVE_MAP)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_EMISSIVE);
		bindPropertyTexture(material->diffuseEmittance,1);
	}

	if (MATERIAL_TRANSPARENCY_MAP)
	{
		glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_TRANSPARENCY);
		bindPropertyTexture(material->specularTransmittance,2); // 2 = RGBA
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// MeshVBOs

MeshVBOs::MeshVBOs()
{
	createdFromMesh[0] = NULL;
	createdFromMesh[1] = NULL;
	createdFromNumTriangles[0] = 0;
	createdFromNumTriangles[1] = 0;
	createdFromNumVertices[0] = 0;
	createdFromNumVertices[1] = 0;
	meshArraysVBOs[0] = NULL;
	meshArraysVBOs[1] = NULL;

	g_numRenderers++;
}

MeshArraysVBOs* MeshVBOs::getMeshArraysVBOs(const rr::RRMesh* mesh, bool indexed)
{
	if (!mesh)
		return NULL;

	unsigned index = indexed?0:1;
	unsigned numTriangles = mesh->getNumTriangles();
	unsigned numVertices = mesh->getNumVertices();

	if (!numTriangles || !numVertices)
		return NULL;
	
	// RRMesh update
	if (createdFromMesh[index]!=mesh || createdFromNumTriangles[index]!=numTriangles || createdFromNumVertices[index]!=numVertices)
	{
		createdFromMesh[index] = mesh;
		createdFromNumTriangles[index] = numTriangles;
		createdFromNumVertices[index] = numVertices;

		// when rendering multiobject (they may have vertices with different uv welded), fail indexed, allow only !indexed
		// (indexed would be used only for shadowmaps, potentially speeding them up slightly,
		//  but 25% more memory for VBOs is too expensive)
		if (!indexed || !mesh->getPreImportTriangle(numTriangles-1).object)
		{
			if (!meshArraysVBOs[index])
				meshArraysVBOs[index] = new MeshArraysVBOs;
			const rr::RRMeshArrays* meshArrays = indexed ? dynamic_cast<const rr::RRMeshArrays*>(mesh) : NULL;
			rr::RRMeshArrays meshArraysLocal;
			if (!meshArrays)
			{
				rr::RRVector<unsigned> texcoords;
				// a) find all channels used by object
				//_object->faceGroups.getTexcoords(texcoords,1,1,0,1,1);
				// b) find all channels used in this draw call
				//_object->faceGroups.getTexcoords(texcoords,
				//	params.renderedChannels.LIGHT_INDIRECT_MAP||params.renderedChannels.LIGHT_INDIRECT_MAP2||params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP,
				//	params.renderedChannels.MATERIAL_DIFFUSE_MAP,
				//	0,
				//	params.renderedChannels.MATERIAL_EMISSIVE_MAP,
				//	params.renderedChannels.MATERIAL_TRANSPARENCY_MAP);
				// c) find all channels in mesh in 0..100 range
				for (unsigned i=0;i<=100;i++)
				{
					rr::RRMesh::TriangleMapping mapping;
					if (mesh->getTriangleMapping(0,mapping,i))
						texcoords.push_back(i);
				}
				if (texcoords.size()>40)
				{
					rr::RRReporter::report(rr::WARN,"getTriangleMapping() returns true for (nearly) all uv channels, please reduce number of uv channels to save memory.\n");
				}
				meshArraysLocal.reload(mesh,indexed,texcoords);
			}
			if (!meshArraysVBOs[index]->update(meshArrays?meshArrays:&meshArraysLocal,indexed))
				RR_SAFE_DELETE(meshArraysVBOs[index]);
		}
	}

	// RRMeshArrays update
	if (indexed && meshArraysVBOs[index])
	{
		const rr::RRMeshArrays* meshArrays = dynamic_cast<const rr::RRMeshArrays*>(mesh);
		if (meshArrays)
			meshArraysVBOs[index]->update(meshArrays,indexed);
	}

	return meshArraysVBOs[index];
}

MeshVBOs::~MeshVBOs()
{
	delete meshArraysVBOs[1];
	delete meshArraysVBOs[0];

	if (!--g_numRenderers) free1x1();
}


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfMesh

void RendererOfMesh::render(RendererOfRRObject& _params)
{
	RendererOfRRObject::Params& params = _params.params;
	RR_ASSERT(params.object);
	if (!params.object) return;

	// vcolor2 allowed only if vcolor is set
	RR_ASSERT(params.renderedChannels.LIGHT_INDIRECT_VCOLOR || !params.renderedChannels.LIGHT_INDIRECT_VCOLOR2);
	// map2 allowed only if map is set
	RR_ASSERT(params.renderedChannels.LIGHT_INDIRECT_MAP || !params.renderedChannels.LIGHT_INDIRECT_MAP2);
	// light detail map allowed only if map is not set
	RR_ASSERT(!params.renderedChannels.LIGHT_INDIRECT_MAP || !params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP);

	// indirect illumination source - solver, buffers or none?
	bool renderIndirect = params.renderedChannels.LIGHT_INDIRECT_VCOLOR || params.renderedChannels.LIGHT_INDIRECT_MAP;
	bool readIndirectFromSolver = renderIndirect && params.indirectIlluminationSource==RendererOfRRObject::SOLVER;
	bool readIndirectFromBuffers = renderIndirect && params.indirectIlluminationSource==RendererOfRRObject::BUFFERS;
	bool readIndirectFromNone = renderIndirect && params.indirectIlluminationSource==RendererOfRRObject::NONE;

	bool setNormals = params.renderedChannels.NORMALS || params.renderedChannels.LIGHT_DIRECT;

	// BUFFERS
	// general and faster code, but can't handle objects with big preimport vertex numbers (e.g. multiobject)
	// -> automatic fallback to !BUFFERS

	bool dontUseIndexed = 0;
	bool dontUseNonIndexed = 0;
	bool dontUseIndexedBecauseOfUnwrap = 0;

	// buffer generated by RRDynamicSolver need original (preimport) vertex order, it is only in indexed
	if (readIndirectFromBuffers && params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		dontUseNonIndexed = true;
	}
	// MeshArraysVBOs can't create indexed buffer from solver
	if (readIndirectFromSolver)
	{
		dontUseIndexed = true;
	}

	bool unwrapSplitsVertices = false; //!!! breaks autogenerated unwrap

	// unwrap may be incompatible with indexed render (fallback uv generated by us)
	if ((params.renderedChannels.LIGHT_INDIRECT_MAP || params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP) && unwrapSplitsVertices)
	{
		dontUseIndexedBecauseOfUnwrap = true;
	}
	// indirectFromSolver can't generate ambient maps
	if (readIndirectFromSolver && params.renderedChannels.LIGHT_INDIRECT_MAP)
	{
		RR_ASSERT(0);
		return;
	}
	// FORCE_2D_POSITION vertices must be unique, incompatible with indexed buffer
	if (params.renderedChannels.FORCE_2D_POSITION)
	{
		dontUseIndexed = true;
	}
	// error, you forgot to call setIndirectIlluminationXxx()
	if (readIndirectFromNone)
	{
		RR_ASSERT(0);
		return;
	}

	/////////////////////////////////////////////////////////////////////////

	PreserveCullFace p1;
	PreserveCullMode p2;
	PreserveBlend p3;

	glColor4ub(0,0,0,255);

	/////////////////////////////////////////////////////////////////////////

	const rr::RRMesh* mesh = params.object->getCollider()->getMesh();

	if (!dontUseIndexed && !dontUseIndexedBecauseOfUnwrap)
	{
		MeshArraysVBOs* indexedYes = getMeshArraysVBOs(mesh,true);
		if (indexedYes)
		{
			// INDEXED
			// indexed is faster, use always when possible
			indexedYes->render(params);
			return;
		}
	}
	if (!dontUseNonIndexed)
	{
		MeshArraysVBOs* indexedNo = getMeshArraysVBOs(mesh,false);
		if (indexedNo)
		{
			// NON INDEXED
			// non-indexed is slower
			indexedNo->render(params);
			return;
		}
	}
	if (!dontUseIndexed)
	{
		MeshArraysVBOs* indexedYes = getMeshArraysVBOs(mesh,true);
		if (indexedYes)
		{
			// INDEXED
			// with unwrap errors
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Unwrap may be rendered incorrectly (circumstances: %s%s%s%s%s%s%s).\n", unwrapSplitsVertices?"unwrapSplitsVertices+":"", readIndirectFromBuffers?"renderingFromBuffers+":"", readIndirectFromSolver?"readIndirectFromSolver+":"", params.renderedChannels.LIGHT_INDIRECT_VCOLOR?"LIGHT_INDIRECT_VCOLOR+":"", params.renderedChannels.LIGHT_INDIRECT_MAP?"LIGHT_INDIRECT_MAP+":"", params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP?"LIGHT_INDIRECT_DETAIL_MAP":"", params.renderedChannels.FORCE_2D_POSITION?"FORCE_2D_POSITION":"" ));
			indexedYes->render(params);
			return;
		}
	}
	{
		// NONE
		// none of rendering techniques can do this job
		MeshArraysVBOs* indexedYes = getMeshArraysVBOs(mesh,true);
		MeshArraysVBOs* indexedNo = getMeshArraysVBOs(mesh,false);
		if (indexedYes||indexedNo)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Scene is not rendered (circumstances: %s%s%s%s%s%s%s%s%s).\n", indexedYes?"indexed+":"", indexedNo?"nonindexed":"", unwrapSplitsVertices?"unwrapSplitsVertices+":"", readIndirectFromBuffers?"renderingFromBuffers+":"", readIndirectFromSolver?"readIndirectFromSolver+":"", params.renderedChannels.LIGHT_INDIRECT_VCOLOR?"LIGHT_INDIRECT_VCOLOR+":"", params.renderedChannels.LIGHT_INDIRECT_MAP?"LIGHT_INDIRECT_MAP+":"", params.renderedChannels.LIGHT_INDIRECT_DETAIL_MAP?"LIGHT_INDIRECT_DETAIL_MAP":"", params.renderedChannels.FORCE_2D_POSITION?"FORCE_2D_POSITION":"" ));
		}
		else
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Scene is not rendered (not enough memory).\n" ));
		}
	}
}

}; // namespace
