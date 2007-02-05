// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "RRIllumination.h"
#include "RRGPUOpenGL/RendererOfRRObject.h"
#include "DemoEngine/Texture.h"
#include "DemoEngine/UberProgramSetup.h" // texture/multitexcoord id assignments

#define BUFFERS // enables faster rendering, but more memory is used

#define SAFE_DELETE(a)   {delete a;a=NULL;}

namespace rr_gl
{


//////////////////////////////////////////////////////////////////////////////
//
// ObjectBuffers - RRObject data stored in buffers for faster rendering

#ifdef BUFFERS

class ObjectBuffers
{
public:
	//! \param indexed
	//!  False = generates triangle list, numVertices == 3*numTriangles.
	//!  True = generates indexed triangle list, numIndices <= 3*numTriangles (merges identical vertices).
	ObjectBuffers(const rr::RRObject* object, bool indexed)
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
		vertices = new Vertex[numTriangles*3]; // Always allocates worst case scenario size. Only first numVertices is used.
		memset(vertices,0,sizeof(Vertex)*numTriangles*3);
		unsigned previousSurfaceIndex = 0;
		for(unsigned t=0;t<numTriangles;t++)
		{
			// read triangle params
			rr::RRMesh::Triangle triangleVertices;
			mesh->getTriangle(t,triangleVertices);
			rr::RRObject::TriangleNormals triangleNormals;
			object->getTriangleNormals(t,triangleNormals);
			rr::RRObject::TriangleMapping triangleMapping;
			object->getTriangleMapping(t,triangleMapping);
			rr::RRVec2 diffuseUv[3];
			mesh->getChannelData(CHANNEL_TRIANGLE_VERTICES_DIF_UV,t,diffuseUv,sizeof(diffuseUv));
			// surface change? -> start new facegroup
			unsigned surfaceIndex = object->getTriangleSurface(t);
			if(!t || surfaceIndex!=previousSurfaceIndex)
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
				const rr::RRSurface* surface = object->getSurface(surfaceIndex);
				fg.diffuseColor = surface ? surface->diffuseReflectance : rr::RRVec3(0);
				mesh->getChannelData(CHANNEL_SURFACE_DIF_TEX,surfaceIndex,&fg.diffuseTexture,sizeof(fg.diffuseTexture));
				faceGroups.push_back(fg);
				previousSurfaceIndex = surfaceIndex;
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
					//assert(currentVertex<numTriangles*3);
					return;
				}
				mesh->getVertex(triangleVertices[v],vertices[currentVertex].vertex);
				vertices[currentVertex].normal = triangleNormals.norm[v];
				vertices[currentVertex].texcoordAmbient = triangleMapping.uv[v];
				vertices[currentVertex].texcoordDiffuse = diffuseUv[v];
			}
			// generate facegroups
			faceGroups[faceGroups.size()-1].numIndices += 3;
		}
		initedOk = true;
	}

	~ObjectBuffers()
	{
		delete[] vertices;
		delete[] indices;
	}

	bool inited()
	{
		return initedOk;
	}

	void render(RendererOfRRObject::Params& params)
	{
		assert(initedOk);
		if(!initedOk) return;
		// set vertices
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &vertices->vertex.x);
		// set normals
		bool setNormals = params.renderedChannels.LIGHT_DIRECT || params.renderedChannels.LIGHT_INDIRECT_ENV;
		if(setNormals)
		{
			glEnableClientState(GL_NORMAL_ARRAY);
			glNormalPointer(GL_FLOAT, sizeof(Vertex), &vertices->normal.x);
		}
		// set indirect illumination vertices
		if(params.renderedChannels.LIGHT_INDIRECT_COLOR && params.indirectIllumination)
		{
			assert(indices); // indirectIllumination has vertices merged according to RRObject, can't be used with non-indexed trilist, needs indexed trilist
			unsigned bufferSize = params.indirectIllumination->getNumVertices();
			assert(numVertices<=bufferSize); // indirectIllumination buffer must be of the same size (or bigger) as our vertex buffer. It's bigger if last vertices in original vertex order are unused (it happens in .bsp).
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_FLOAT, 12, params.indirectIllumination->lock());
		}
		// set indirect illumination texcoords + map
		if(params.renderedChannels.LIGHT_INDIRECT_MAP && params.indirectIlluminationMap)
		{
			glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_LIGHT_INDIRECT);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &vertices->texcoordAmbient.x);
			glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_LIGHT_INDIRECT);
			params.indirectIlluminationMap->bindTexture();
		}
		// set 2d_position texcoords
		if(params.renderedChannels.FORCE_2D_POSITION)
		{
			// this should not be executed in every frame, generated texcoords change rarely
			assert(!indices); // needs non-indexed trilist
			//for(unsigned i=0;i<numVertices;i++) // for all capture textures, probably not necessary
			for(unsigned i=params.firstCapturedTriangle*3;i<3*params.lastCapturedTrianglePlus1;i++) // only for our capture texture
			{
				params.generateForcedUv->generateData(i/3, i%3, &vertices[i].texcoordForced2D.x, sizeof(vertices[0].texcoordForced2D));
			}
			glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_FORCED_2D);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &vertices->texcoordForced2D.x);
		}
		// set material diffuse texcoords
		if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
		{
			glClientActiveTexture(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_DIFFUSE);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &vertices->texcoordDiffuse.x);
		}
		// render facegroups (facegroups differ by material)
		if(params.renderedChannels.MATERIAL_DIFFUSE_COLOR || params.renderedChannels.MATERIAL_DIFFUSE_MAP)
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
					if(params.renderedChannels.MATERIAL_DIFFUSE_COLOR)
					{
						glSecondaryColor3fv(&faceGroups[fg].diffuseColor[0]);
					}
					// set diffuse map
					if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
					{
						glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_MATERIAL_DIFFUSE);
						faceGroups[fg].diffuseTexture->bindTexture();
					}
					// render one facegroup
					if(indices)
					{
						for(unsigned i=firstIndex;i<firstIndex+numIndices;i++) assert(indices[i]<numVertices);
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
				for(unsigned i=firstIndex;i<firstIndex+numIndices;i++) assert(indices[i]<numVertices);
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
		if(params.renderedChannels.LIGHT_INDIRECT_COLOR && params.indirectIllumination)
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

private:
	bool initedOk; // true when constructor had no problems and instance is ready to render
	struct Vertex
	{
		rr::RRVec3 vertex;
		rr::RRVec3 normal;
		rr::RRVec2 texcoordDiffuse;
		rr::RRVec2 texcoordForced2D; // is unique for each vertex
		rr::RRVec2 texcoordAmbient; // could be unique for each vertex (with default unwrap)
	};
	unsigned numVertices;
	Vertex* vertices;
	unsigned numIndices;
	unsigned* indices;
	struct FaceGroup
	{
		unsigned firstIndex;
		unsigned numIndices;
		rr::RRVec3 diffuseColor;
		de::Texture* diffuseTexture;
	};
	std::vector<FaceGroup> faceGroups;
};

#endif


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject

RendererOfRRObject::RendererOfRRObject(const rr::RRObject* objectImporter, const rr::RRScene* radiositySolver, const rr::RRScaler* scaler)
{
	params.object = objectImporter;
	params.scene = radiositySolver;
	params.scaler = scaler;
	//params.renderedChannels = ... set to default by constructor
	params.generateForcedUv = NULL;
	params.firstCapturedTriangle = 0;
	params.lastCapturedTrianglePlus1 = objectImporter->getCollider()->getMesh()->getNumTriangles();
	params.indirectIllumination = NULL;
	params.indirectIlluminationMap = NULL;

#ifdef BUFFERS
	indexedYes = new ObjectBuffers(objectImporter,true);
	indexedNo = new ObjectBuffers(objectImporter,false);
	if(!indexedYes->inited() || !indexedNo->inited())
	{
		SAFE_DELETE(indexedYes);
		SAFE_DELETE(indexedNo);
	}
#endif
}

RendererOfRRObject::~RendererOfRRObject()
{
#ifdef BUFFERS
	delete indexedNo;
	delete indexedYes;
#endif
}

void RendererOfRRObject::setRenderedChannels(RenderedChannels renderedChannels)
{
	params.renderedChannels = renderedChannels;
}

void RendererOfRRObject::setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle, unsigned alastCapturedTrianglePlus1)
{
	params.generateForcedUv = capture;
	params.otherCaptureParamsHash = capture ? capture->getHash() : 0;
	params.firstCapturedTriangle = afirstCapturedTriangle;
	params.lastCapturedTrianglePlus1 = alastCapturedTrianglePlus1;
}

void RendererOfRRObject::setIndirectIllumination(rr::RRIlluminationVertexBuffer* vertexBuffer,rr::RRIlluminationPixelBuffer* ambientMap)
{
	params.indirectIllumination = vertexBuffer;
	params.indirectIlluminationMap = ambientMap;
}

const void* RendererOfRRObject::getParams(unsigned& length) const
{
	length = sizeof(params);
	return &params;
}

void RendererOfRRObject::render()
{
	assert(params.object);
	if(!params.object) return;

	glColor4ub(0,0,0,255);
	glEnable(GL_CULL_FACE);

	bool setNormals = params.renderedChannels.LIGHT_DIRECT || params.renderedChannels.LIGHT_INDIRECT_ENV;

#ifdef BUFFERS
	if(indexedYes && indexedNo)
	{
		// BUFFERS
		// general and faster code, but can't handle objects with big preimport vertex numbers (e.g. multiobject)
		// -> automatic fallback to !BUFFERS

		bool indexedYesNeeded = 
			params.renderedChannels.LIGHT_INDIRECT_COLOR; // buffer generated by RRRealtimeRadiosity for original (preimport) vertex order
		bool indexedNoNeeded = 
			params.renderedChannels.FORCE_2D_POSITION // vertices always unique, needs nonindexed buffer
			|| params.renderedChannels.LIGHT_INDIRECT_MAP; // vertices sometimes unique (with default unwrap), better use nonindexed buffer

		if(!indexedNoNeeded)
		{
			// indexed is faster, use always when possible
			indexedYes->render(params);
		}
		if(indexedNoNeeded && !indexedYesNeeded)
		{
			// non-indexed is slower
			indexedNo->render(params);
		}
		if(indexedNoNeeded && indexedYesNeeded)
		{
			// invalid combination of render channels, not supported
			assert(0);
		}
	}
	else
#endif
	{
		// !BUFFERS 
		// general, but slower code, used for FORCE_2D_POSITION

		bool begun = false;
		rr::RRMesh* meshImporter = params.object->getCollider()->getMesh();
		//unsigned numTriangles = meshImporter->getNumTriangles();
		unsigned oldSurfaceIdx = UINT_MAX;
		rr::RRObjectIllumination* oldIllumination = NULL;
		for(unsigned triangleIdx=params.firstCapturedTriangle;triangleIdx<params.lastCapturedTrianglePlus1;triangleIdx++)
		{
			rr::RRMesh::Triangle tri;
			meshImporter->getTriangle(triangleIdx,tri);

			if(params.renderedChannels.MATERIAL_DIFFUSE_COLOR || params.renderedChannels.MATERIAL_DIFFUSE_MAP)
			{
				unsigned surfaceIdx = params.object->getTriangleSurface(triangleIdx);
				if(surfaceIdx!=oldSurfaceIdx)
				{
					oldSurfaceIdx = surfaceIdx;

					// material diffuse color
					if(params.renderedChannels.MATERIAL_DIFFUSE_COLOR)
					{
						const rr::RRSurface* surface = params.object->getSurface(surfaceIdx);
						if(surface)
						{
							glSecondaryColor3fv(&surface->diffuseReflectance.x);
						}
						else
						{
							assert(0); // expected data are missing
						}
					}

					// material diffuse map - bind texture
					if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
					{
						de::Texture* tex = NULL;
						params.object->getCollider()->getMesh()->getChannelData(CHANNEL_SURFACE_DIF_TEX,surfaceIdx,&tex,sizeof(tex));
						if(tex)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_MATERIAL_DIFFUSE);
							tex->bindTexture();
						}
						else
						{			
							assert(0); // expected data are missing
						}
					}
				}
			}

			// mesh normals - prepare data
			rr::RRObject::TriangleNormals triangleNormals;
			if(setNormals)
			{
				params.object->getTriangleNormals(triangleIdx,triangleNormals);
			}

			// light indirect map
			if(params.renderedChannels.LIGHT_INDIRECT_MAP)
			{
				// ask object/face for ambient map
				// why this design:
				//  Jdu po trianglech multiobjektu, ale ambient mapy jsou kvuli externim rendererum
				//  vyrabeny pro orig objekty, takze musim 
				//  1. z indexu do multiobjektu ziskat orig objekt a index do nej
				//  2. sahnout pro ambient mapu a uv do orig objektu
				//  Bohuzel neni samozrejme ze by vse transparentne zaridil multiobjekt a 
				//  tady bych o multiobjektizaci vubec nevedel,
				//  protoze objekty samy o svych ambient mapach nevedi, ty podle indexu objektu
				//  drzi RRRealtimeRadiosity.
				//  Q: Muzu ambient mapy dodatecne prilepit k multiobjektu (do ChanneledData),
				//     takze by to preci jen poresila multiobjektizace?
				//  A: Ne, ambient map muze byt mnoho na jeden objekt.
				//     Je ukol RRObjectIllumination je nejak zmixovat a poskytnout vysledek.
				//  Q: Muzu tedy prilepit RRObjectIllumination k objektu?
				//  A: Ano. Ale je to vyhodne?
				//     Prilepeny RRObjectIllumination k objektu: YES
				//      + tento renderer bude univerzalni 
				//      - trocha prace s lepenim
				//        Lepsi nez lepici filtr ktery by vsechno zpomalil o 1 indirect call
				//        je vyrobit RRObjectIllumination primo v me implementaci RRObjectu,
				//        zde v M3dsImporter; a tamtez ho natlacit do RRRealtimeRadiosity, tamtez ho deletovat.
				//     Demultiobjektizovat lokalne zde:
				//      - trocha prace zde
				//      - nutne dotahnout sem RRRealtimeRadiosity (jako zdroj orig objektu a ambient map)
				//        coz vytvari oboustrannou vazbu mezi RRRealtimeRadiosity a nami(DemoEngine)
				//  Q: Jak bude renderovat klient ktery ma ke hre linkle jen RRIllumination?
				//  A: Pouzije vlastni unwrap a mnou serializovany RRObjectIllumination
				//     (nebo si i sam muze ukladat ambient mapy).
				//  Q: Nemel by tedy Model_3ds (jako priklad externiho rendereru) zaviset jen na RRIllumination?
				//  A: Ano, mel. RRRealtimeRadiosity pouziva jen pro pristup ke spravnym RRObjectIllumination,
				//     neni tezke to predelat.

				rr::RRObjectIllumination* objectIllumination = NULL;
				if(params.object->getCollider()->getMesh()->getChannelData(CHANNEL_TRIANGLE_OBJECT_ILLUMINATION,triangleIdx,&objectIllumination,sizeof(objectIllumination))
					&& objectIllumination!=oldIllumination)
				{
					oldIllumination = objectIllumination;
					// setup light indirect texture
					//!!! later use channel mixer instead of channel 0
					rr::RRIlluminationPixelBuffer* pixelBuffer = objectIllumination->getChannel(0)->pixelBuffer;
					if(pixelBuffer)
					{
						if(begun)
						{
							glEnd();
							begun = false;
						}
						glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_LIGHT_INDIRECT);
						pixelBuffer->bindTexture();
					}
					else
					{
						//!!! kdyz se dostane sem a vysledek zacachuje, bude to uz vzdycky renderovat blbe
						assert(0);
					}
				}
			}

			if(!begun)
			{
				glBegin(GL_TRIANGLES);
				begun = true;
			}

			for(int v=0;v<3;v++)
			{
				// mesh normals - set
				if(setNormals)
				{
					glNormal3fv(&triangleNormals.norm[v].x);
				}

				// light indirect color
				if(params.renderedChannels.LIGHT_INDIRECT_COLOR)
				{
					rr::RRColor color;				
					params.scene->getTriangleMeasure(triangleIdx,v,rr::RM_IRRADIANCE_CUSTOM_INDIRECT,params.scaler,color);
					glColor3fv(&color.x);
				}

				// light indirect map uv
				if(params.renderedChannels.LIGHT_INDIRECT_MAP)
				{
					rr::RRObject::TriangleMapping tm;
					//!!! optimize, get once, not three times per triangle
					params.object->getTriangleMapping(triangleIdx,tm);
					glMultiTexCoord2f(GL_TEXTURE0+de::MULTITEXCOORD_LIGHT_INDIRECT,tm.uv[v][0],tm.uv[v][1]);
				}

				// material diffuse map - uv
				if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
				{
					rr::RRVec2 uv[3];
					//!!! optimize, get once, not three times per triangle
					if(params.object->getCollider()->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_DIF_UV,triangleIdx,&uv,sizeof(uv)))
						glMultiTexCoord2f(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_DIFFUSE,uv[v][0],uv[v][1]);
					else
						assert(0); // expected data are missing
				}

				// forced 2d position
				if(params.renderedChannels.FORCE_2D_POSITION)
				{
					if(params.generateForcedUv)
					{
						GLfloat xy[2];
						params.generateForcedUv->generateData(triangleIdx, v, xy, sizeof(xy));
						glMultiTexCoord2f(GL_TEXTURE0+de::MULTITEXCOORD_FORCED_2D,xy[0],xy[1]);
					}
				}

				// mesh positions
				rr::RRMesh::Vertex vertex;
				meshImporter->getVertex(tri.m[v],vertex);
				glVertex3fv(&vertex.x);
			}
		}
		if(begun)
		{
			glEnd();
		}
	}
}

}; // namespace
