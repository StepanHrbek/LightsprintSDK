// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "RendererOfMesh.h"
#ifdef _OPENMP
#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// MeshArraysVBOs - RRMeshArrays data stored in VBOs for faster rendering

MeshArraysVBOs::MeshArraysVBOs()
{
	createdFromMesh = NULL;
	createdFromMeshVersion = 0;
	createdFromLightIndirectBuffer = NULL;
	createdFromLightIndirectVersion = UINT_MAX;
	createdIndexed = false;
	createdOk = false;
	glGenBuffers(VBO_COUNT,VBO);
}

MeshArraysVBOs::~MeshArraysVBOs()
{
	glDeleteBuffers(VBO_COUNT,VBO);
	if (texcoordVBO.size())
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
			glBufferData(GL_ARRAY_BUFFER, _mesh->numVertices*sizeof((sourceArray)[0]), sourceArray, GL_STATIC_DRAW); \
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
		case rr::BF_BGR:
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
		case rr::BF_BGR:
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
	const unsigned char* data = buffer->lock(rr::BL_READ);
	if (data)
	{
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, buffer->getBufferBytes(), data, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		buffer->unlock();
	}
	else
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Rendering empty (or unlockable) vertex buffer.\n"));
	}
}

void MeshArraysVBOs::render(
	Program* _program,
	const rr::RRObject* _object,
	const FaceGroupRange* _faceGroupRange,
	unsigned _numFaceGroupRanges,
	const UberProgramSetup& _uberProgramSetup,
	bool _renderingFromLight,
	rr::RRBuffer* _lightIndirectBuffer,
	const rr::RRBuffer* _lightDetailMap)
{
	if (!_object)
	{
		return;
	}

	#define DRAW_ELEMENTS(a,b,c,d)                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO[VBO_index]); glDrawElements(a,b,c,(const GLvoid*)(sizeof(unsigned)*d));
	#define BIND_VBO2(glName,myName)           RR_ASSERT(myName); glBindBuffer(GL_ARRAY_BUFFER, myName); gl##glName##Pointer(           GL_FLOAT, 0, 0);
	#define BIND_VBO3(glName,numFloats,myName) RR_ASSERT(myName); glBindBuffer(GL_ARRAY_BUFFER, myName); gl##glName##Pointer(numFloats, GL_FLOAT, 0, 0);
	#define BIND_BUFFER(glName,buffer,myName)  RR_ASSERT(myName); glBindBuffer(GL_ARRAY_BUFFER, myName); gl##glName##Pointer(getBufferNumComponents(buffer), getBufferComponentType(buffer), 0, 0);

	// set vertices
	BIND_VBO3(Vertex,3,VBO[VBO_position]);
	glEnableClientState(GL_VERTEX_ARRAY);
	// set normals
	bool setNormals = _uberProgramSetup.LIGHT_DIRECT || _uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || _uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR || _uberProgramSetup.POSTPROCESS_NORMALS;
	if (setNormals)
	{
		BIND_VBO2(Normal,VBO[VBO_normal]);
		glEnableClientState(GL_NORMAL_ARRAY);
	}
	// set indirect illumination vertices
	if (_uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
	{
		if (_lightIndirectBuffer)
		{
			// INDEXED FROM VBUFFER
			// use vertex buffer precomputed by RRDynamicSolver
			// indirectIllumination has vertices merged according to RRObject, can't be used with non-indexed trilist, needs indexed trilist
			bool copyLayerToVBO = createdFromLightIndirectVersion!=_lightIndirectBuffer->version || createdFromLightIndirectBuffer!=_lightIndirectBuffer;
			if (copyLayerToVBO)
			{
				createdFromLightIndirectVersion = _lightIndirectBuffer->version;
				createdFromLightIndirectBuffer = _lightIndirectBuffer;
				copyBufferToVBO(_lightIndirectBuffer,VBO[VBO_lightIndirectVcolor]);
			}
			glEnableClientState(GL_COLOR_ARRAY);
			BIND_BUFFER(Color,_lightIndirectBuffer,VBO[VBO_lightIndirectVcolor]);
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

	// set indirect illumination texcoords + map (lightmap or light detail map)
	if ((_uberProgramSetup.LIGHT_INDIRECT_MAP && _lightIndirectBuffer) || (_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP && _lightDetailMap))
	{
		_program->sendTexture("lightIndirectMap",(_uberProgramSetup.LIGHT_INDIRECT_MAP && _lightIndirectBuffer) ? getTexture(_lightIndirectBuffer,true,false) : getTexture(_lightDetailMap), TEX_CODE_2D_LIGHT_INDIRECT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// set 2d_position texcoords
	if (_uberProgramSetup.FORCE_2D_POSITION)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D);
		BIND_VBO3(TexCoord,2,VBO[VBO_texcoordForced2D]);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	// shortcut
	const rr::RRObject::FaceGroups& faceGroups = _object->faceGroups;

	// render by FaceGroup or FaceGroupRange?
	// facegroups in range differ only by material
	if (_uberProgramSetup.MATERIAL_DIFFUSE_CONST || _uberProgramSetup.MATERIAL_DIFFUSE_MAP
		|| _uberProgramSetup.MATERIAL_SPECULAR // even if specular color=1, setMaterial() must be called to set shininess, shininess does not have any default
		|| _uberProgramSetup.MATERIAL_EMISSIVE_CONST || _uberProgramSetup.MATERIAL_EMISSIVE_MAP
		|| _uberProgramSetup.MATERIAL_TRANSPARENCY_CONST || _uberProgramSetup.MATERIAL_TRANSPARENCY_MAP
		|| _uberProgramSetup.MATERIAL_CULLING
		|| (_uberProgramSetup.LIGHT_INDIRECT_MAP && _lightIndirectBuffer)
		|| (_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP && _lightDetailMap))
	{
		// render by FaceGroup

		// cache uv channel binding
		struct UvChannelBinding
		{
			unsigned boundUvChannel[5]; // indexed by MULTITEXCOORD_XXX
			void bindUvChannel(const rr::RRVector<unsigned>& _texcoordVBO, unsigned _shaderChannel, unsigned _uvChannel, const rr::RRBuffer* _buffer, const rr::RRString& _objectName, const rr::RRString& _materialName)
			{
				RR_ASSERT(_shaderChannel<5);
				if (boundUvChannel[_shaderChannel]==_uvChannel)
				{
					// already set
					return;
				}
				if (!_buffer)
				{
					// 1x1 texture is probably used and uv is irrelevant
					return;
				}
				boundUvChannel[_shaderChannel] = _uvChannel;
				if (_uvChannel>=_texcoordVBO.size() || _texcoordVBO[_uvChannel]==0)
				{
					RR_LIMITED_TIMES(20,rr::RRReporter::report(rr::WARN,"Material '%s' in object '%s' needs non existing uv channel %d (texcoord.size=%d).\n",_materialName.c_str(),_objectName.c_str(),_uvChannel,_texcoordVBO.size()));
					return;
				}
				glClientActiveTexture(GL_TEXTURE0+_shaderChannel);
				BIND_VBO3(TexCoord,2,_texcoordVBO[_uvChannel]);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		};
		UvChannelBinding uvChannelBinding = {UINT_MAX,UINT_MAX,UINT_MAX,UINT_MAX,UINT_MAX};

		for (unsigned r=0;r<_numFaceGroupRanges;r++)
		{
			unsigned fgFirstIndex = 3*_faceGroupRange[r].triangleFirst;
			for (unsigned fg=_faceGroupRange[r].faceGroupFirst; fg<=_faceGroupRange[r].faceGroupLast; fg++)
			{
				const rr::RRMaterial* material = faceGroups[fg].material;
				if (material)
				{
					// skip whole facegroup when alpha keying with constant alpha below 0.5
					// GPU would do the same for all pixels, this is faster
					//  MATERIAL_TRANSPARENCY_BLEND = alpha blending, not alpha keying
					//  MATERIAL_TRANSPARENCY_TO_RGB = rendering blended material to rgb shadowmap or rgb blending, not alpha keying
					if (!(_uberProgramSetup.MATERIAL_TRANSPARENCY_CONST && !_uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && !_uberProgramSetup.MATERIAL_TRANSPARENCY_TO_RGB && material->specularTransmittance.color.avg()>0.5f))
					{
						unsigned fgSubsetNumIndices = 3*faceGroups[fg].numTriangles;
						unsigned fgSubsetFirstIndex = fgFirstIndex;
						if (fgSubsetNumIndices>0)
						{
							// set face culling
							if (_uberProgramSetup.MATERIAL_CULLING)
							{
								bool renderFront = material->sideBits[0].renderFrom || (_renderingFromLight && material->sideBits[0].catchFrom);
								bool renderBack = material->sideBits[1].renderFrom || (_renderingFromLight && material->sideBits[1].catchFrom);
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
							else
							{
								glDisable(GL_CULL_FACE);
							}

							// set material
							_uberProgramSetup.useMaterial(_program,material);
							if (_uberProgramSetup.MATERIAL_DIFFUSE_MAP)
								uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_MATERIAL_DIFFUSE,material->diffuseReflectance.texcoord,material->diffuseReflectance.texture,_object->name,material->name);
							if (_uberProgramSetup.MATERIAL_EMISSIVE_MAP)
								uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_MATERIAL_EMISSIVE,material->diffuseEmittance.texcoord,material->diffuseEmittance.texture,_object->name,material->name);
							if (_uberProgramSetup.MATERIAL_TRANSPARENCY_MAP)
								uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_MATERIAL_TRANSPARENCY,material->specularTransmittance.texcoord,material->specularTransmittance.texture,_object->name,material->name);
							if ((_uberProgramSetup.LIGHT_INDIRECT_MAP && _lightIndirectBuffer) || (_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP && _lightDetailMap))
								uvChannelBinding.bindUvChannel(texcoordVBO,MULTITEXCOORD_LIGHT_INDIRECT,material->lightmapTexcoord,(const rr::RRBuffer*)1,_object->name,material->name);

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
				fgFirstIndex += faceGroups[fg].numTriangles*3;
			}
		}
	}
	else
	{
		// render by FaceGroupRange
		for (unsigned r=0;r<_numFaceGroupRanges;r++)
		{
			unsigned objSubsetFirstIndex = 3*_faceGroupRange[r].triangleFirst;
			unsigned objSubsetNumIndices = 3*(_faceGroupRange[r].triangleLastPlus1-_faceGroupRange[r].triangleFirst);
			if (createdIndexed)
			{
				DRAW_ELEMENTS(GL_TRIANGLES, objSubsetNumIndices, GL_UNSIGNED_INT, objSubsetFirstIndex);
			}
			else
			{
				glDrawArrays(GL_TRIANGLES, objSubsetFirstIndex, objSubsetNumIndices);
			}
		}
	}
	// unset material diffuse texcoords
	if (_uberProgramSetup.MATERIAL_DIFFUSE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset material transparency texcoords
	if (_uberProgramSetup.MATERIAL_TRANSPARENCY_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_TRANSPARENCY);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset material emissive texcoords
	if (_uberProgramSetup.MATERIAL_EMISSIVE_MAP)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_EMISSIVE);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset 2d_position texcoords
	if (_uberProgramSetup.FORCE_2D_POSITION)
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	// unset indirect illumination texcoords + map (lightmap or light detail map)
	if ((_uberProgramSetup.LIGHT_INDIRECT_MAP && _lightIndirectBuffer) || (_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP && _lightDetailMap))
	{
		glClientActiveTexture(GL_TEXTURE0+MULTITEXCOORD_LIGHT_INDIRECT);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		//_program->sendTexture("lightIndirectMap",NULL, TEX_CODE_2D_LIGHT_INDIRECT);
		//glBindTexture(GL_TEXTURE_2D,0);
	}
	// unset indirect illumination colors
	if (_uberProgramSetup.LIGHT_INDIRECT_VCOLOR)
	{
		glDisableClientState(GL_COLOR_ARRAY);
	}
	// unset normals
	if (setNormals)
	{
		glDisableClientState(GL_NORMAL_ARRAY);
	}
	// unset vertices
	glDisableClientState(GL_VERTEX_ARRAY);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


//////////////////////////////////////////////////////////////////////////////
//
// Helpers: temporaries for VBO update

class Helpers
{
public:
	rr::RRVector<unsigned> texcoords; // using this [in MeshVBOs::getMeshArraysVBOs] instead of local variable saves 1 temp allocation
	rr::RRMeshArrays meshArrays; // using this [in MeshVBOs::getMeshArraysVBOs] instead of local variable saves 2 temp allocations
};

static Helpers s_helpers;


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
	updatedOk[0] = false;
	updatedOk[1] = false;
}

MeshArraysVBOs* MeshVBOs::getMeshArraysVBOs(const rr::RRMesh* _mesh, bool _indexed)
{
	if (!_mesh)
		return NULL;

	unsigned index = _indexed?0:1;
	unsigned numTriangles = _mesh->getNumTriangles();
	unsigned numVertices = _mesh->getNumVertices();

	if (!numTriangles || !numVertices)
		return NULL;
	
	// RRMesh update
	if (createdFromMesh[index]!=_mesh || createdFromNumTriangles[index]!=numTriangles || createdFromNumVertices[index]!=numVertices)
	{
		createdFromMesh[index] = _mesh;
		createdFromNumTriangles[index] = numTriangles;
		createdFromNumVertices[index] = numVertices;

		{
			const rr::RRMeshArrays* meshArrays = _indexed ? dynamic_cast<const rr::RRMeshArrays*>(_mesh) : NULL;
			if (!meshArrays)
			{
				s_helpers.texcoords.clear();
				_mesh->getUvChannels(s_helpers.texcoords);
				if (s_helpers.texcoords.size()>20)
				{
					rr::RRReporter::report(rr::WARN,"Mesh has %d uv channels, consider removing some to save memory.\n",s_helpers.texcoords.size());
				}
				// copy data to arrays
				s_helpers.meshArrays.reload(_mesh,_indexed,s_helpers.texcoords,false);
			}
			updatedOk[index] = meshArraysVBOs[index].update(meshArrays?meshArrays:&s_helpers.meshArrays,_indexed);
		}
	}

	// RRMeshArrays update
	if (_indexed && updatedOk[index])
	{
		const rr::RRMeshArrays* meshArrays = dynamic_cast<const rr::RRMeshArrays*>(_mesh);
		if (meshArrays)
			meshArraysVBOs[index].update(meshArrays,_indexed);
	}

	return updatedOk[index] ? &meshArraysVBOs[index] : NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfMesh

void RendererOfMesh::render(
	Program* _program,
	const rr::RRObject* _object,
	const FaceGroupRange* _faceGroupRange,
	unsigned _numFaceGroupRanges,
	const UberProgramSetup& _uberProgramSetup,
	bool _renderingFromLight,
	rr::RRBuffer* _lightIndirectBuffer,
	const rr::RRBuffer* _lightDetailMap)
{
	RR_ASSERT(_object);
	if (!_object) return;

	// light detail map allowed only if map is not set
	RR_ASSERT(!_uberProgramSetup.LIGHT_INDIRECT_MAP || !_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP);

	bool dontUseIndexed = 0;
	bool dontUseNonIndexed = 0;

	// we have two VBO sizes, indexed and non-indexed.
	// vertex buffer size usually matches only one of them, disable the other one
	if (_lightIndirectBuffer && _uberProgramSetup.LIGHT_INDIRECT_VCOLOR && _lightIndirectBuffer->getWidth()!=_object->getCollider()->getMesh()->getNumVertices())
	{
		dontUseIndexed = true;
	}
	if (_lightIndirectBuffer && _uberProgramSetup.LIGHT_INDIRECT_VCOLOR && _lightIndirectBuffer->getWidth()!=_object->getCollider()->getMesh()->getNumTriangles()*3)
	{
		dontUseNonIndexed = true;
	}
	// FORCE_2D_POSITION vertices must be unique, incompatible with indexed buffer
	if (_uberProgramSetup.FORCE_2D_POSITION)
	{
		dontUseIndexed = true;
	}

	/////////////////////////////////////////////////////////////////////////

	glColor4ub(0,0,0,255);

	/////////////////////////////////////////////////////////////////////////

	const rr::RRMesh* mesh = _object->getCollider()->getMesh();
	MeshArraysVBOs* meshArraysVBOs = NULL;

	if (!dontUseIndexed)
	{
		meshArraysVBOs = getMeshArraysVBOs(mesh,true);
	}
	if (!meshArraysVBOs && !dontUseNonIndexed)
	{
		meshArraysVBOs = getMeshArraysVBOs(mesh,false);
	}
	if (meshArraysVBOs)
	{
		meshArraysVBOs->render(
			_program,
			_object,
			_faceGroupRange,
			_numFaceGroupRanges,
			_uberProgramSetup,
			_renderingFromLight,
			_lightIndirectBuffer,
			_lightDetailMap);
	}
	else
	if (mesh)
	{
		// none of rendering techniques can do this job
		MeshArraysVBOs* indexedYes = getMeshArraysVBOs(mesh,true);
		MeshArraysVBOs* indexedNo = getMeshArraysVBOs(mesh,false);
		unsigned numTriangles = mesh->getNumTriangles();
		unsigned numVertices = mesh->getNumVertices();
		RR_LIMITED_TIMES(10,rr::RRReporter::report(rr::WARN,"Mesh is not rendered (circumstances: %dtri+%dvert+%s%s%s%s%s%s).\n", numTriangles, numVertices, indexedYes?"indexed+":"", indexedNo?"nonindexed+":"", _uberProgramSetup.LIGHT_INDIRECT_VCOLOR?"LIGHT_INDIRECT_VCOLOR+":"", _uberProgramSetup.LIGHT_INDIRECT_MAP?"LIGHT_INDIRECT_MAP+":"", _uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP?"LIGHT_INDIRECT_DETAIL_MAP":"", _uberProgramSetup.FORCE_2D_POSITION?"FORCE_2D_POSITION":"" ));
	}
}

}; // namespace
