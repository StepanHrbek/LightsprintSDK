// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"
#include "Lightsprint/DemoEngine/Texture.h"
#include "Lightsprint/DemoEngine/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "ObjectBuffers.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject

RendererOfRRObject::RendererOfRRObject(const rr::RRObject* objectImporter, const rr::RRStaticSolver* radiositySolver, const rr::RRScaler* scaler, bool useBuffers)
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

	indexedYes = NULL;
	indexedNo = NULL;
	if(useBuffers)
	{
		indexedYes = new ObjectBuffers(objectImporter,true);
		if(!indexedYes->inited())
			SAFE_DELETE(indexedYes);
		indexedNo = new ObjectBuffers(objectImporter,false);
		if(!indexedNo->inited())
			SAFE_DELETE(indexedNo);
	}
}

RendererOfRRObject::~RendererOfRRObject()
{
	delete indexedNo;
	delete indexedYes;
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

void RendererOfRRObject::setIndirectIllumination(rr::RRIlluminationVertexBuffer* vertexBuffer,const rr::RRIlluminationPixelBuffer* ambientMap,unsigned asolutionVersion)
{
	params.indirectIllumination = vertexBuffer;
	params.indirectIlluminationMap = ambientMap;
	solutionVersion = asolutionVersion;
}

const void* RendererOfRRObject::getParams(unsigned& length) const
{
	length = sizeof(params);
	return &params;
}

void RendererOfRRObject::render()
{
	RR_ASSERT(params.object);
	if(!params.object) return;

	glColor4ub(0,0,0,255);

	bool setNormals = params.renderedChannels.LIGHT_DIRECT || params.renderedChannels.LIGHT_INDIRECT_ENV;

	// BUFFERS
	// general and faster code, but can't handle objects with big preimport vertex numbers (e.g. multiobject)
	// -> automatic fallback to !BUFFERS

	bool indexedYesNeeded = 
		//params.renderedChannels.LIGHT_INDIRECT_VCOLOR; // buffer generated by RRDynamicSolver for original (preimport) vertex order
		0; // LIGHT_INDIRECT_VCOLOR is now supported by everyone (indexed uses vbuffer, !indexed and !buffered read data from staticsolver)
	bool indexedNoNeeded = 
		params.renderedChannels.FORCE_2D_POSITION // vertices always unique, needs nonindexed buffer
		|| params.renderedChannels.LIGHT_INDIRECT_MAP; // vertices sometimes unique (with default unwrap), better use nonindexed buffer
	bool nonBufferedNeeded =
		// for all multiobjects (and possibly for few other innocent objects that failed creation of indexedYes),
		// avoid buffered render of indirect vcolor or map, force nonbuffered
		// because setIndirectIllumination() API allows for entering only 1 buffer
		!indexedYes && // true for all multiobjects
		(//params.renderedChannels.LIGHT_INDIRECT_VCOLOR || // LIGHT_INDIRECT_VCOLOR is now supported by everyone (indexed uses vbuffer, !indexed and !buffered read data from staticsolver)
		  params.renderedChannels.LIGHT_INDIRECT_MAP);

	if(indexedYes && !indexedNoNeeded && !nonBufferedNeeded)
	{
		// indexed is faster, use always when possible
		indexedYes->render(params,solutionVersion);
	}
	else
	if(indexedNo && !indexedYesNeeded && !nonBufferedNeeded)
	{
		// non-indexed is slower
		indexedNo->render(params,solutionVersion);
	}
	else
	{
		// !BUFFERS 
		// general, but slower code, used for FORCE_2D_POSITION

		bool begun = false;
		rr::RRMesh* meshImporter = params.object->getCollider()->getMesh();
		//unsigned numTriangles = meshImporter->getNumTriangles();
		const rr::RRMaterial* oldMaterial = NULL;
		rr::RRObjectIllumination* oldIllumination = NULL;
		for(unsigned triangleIdx=params.firstCapturedTriangle;triangleIdx<params.lastCapturedTrianglePlus1;triangleIdx++)
		{
			rr::RRMesh::Triangle tri;
			meshImporter->getTriangle(triangleIdx,tri);

			if(params.renderedChannels.MATERIAL_DIFFUSE_VCOLOR || params.renderedChannels.MATERIAL_DIFFUSE_MAP || params.renderedChannels.MATERIAL_EMISSIVE_MAP)
			{
				const rr::RRMaterial* material = params.object->getTriangleMaterial(triangleIdx);
				if(material!=oldMaterial)
				{
					oldMaterial = material;

					// material diffuse color
					if(params.renderedChannels.MATERIAL_DIFFUSE_VCOLOR)
					{
						if(material)
						{
							glSecondaryColor3fv(&material->diffuseReflectance.x);
						}
						else
						{
							RR_ASSERT(0); // expected data are missing
						}
					}

					// material diffuse map - bind texture
					if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
					{
						de::Texture* tex = NULL;
						params.object->getChannelData(CHANNEL_TRIANGLE_DIFFUSE_TEX,triangleIdx,&tex,sizeof(tex));
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
							RR_ASSERT(0); // expected data are missing
						}
					}

					// material emissive map - bind texture
					if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
					{
						de::Texture* tex = NULL;
						params.object->getChannelData(CHANNEL_TRIANGLE_EMISSIVE_TEX,triangleIdx,&tex,sizeof(tex));
						if(tex)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_MATERIAL_EMISSIVE);
							tex->bindTexture();
						}
						else
						{			
							RR_ASSERT(0); // expected data are missing
						}
					}
				}
			}

			// mesh normals - prepare data
			rr::RRMesh::TriangleNormals triangleNormals;
			if(setNormals)
			{
				meshImporter->getTriangleNormals(triangleIdx,triangleNormals);
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
				//  drzi RRDynamicSolver.
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
				//        zde v M3dsImporter; a tamtez ho natlacit do RRDynamicSolver, tamtez ho deletovat.
				//     Demultiobjektizovat lokalne zde:
				//      - trocha prace zde
				//      - nutne dotahnout sem RRDynamicSolver (jako zdroj orig objektu a ambient map)
				//        coz vytvari oboustrannou vazbu mezi RRDynamicSolver a nami(DemoEngine)
				//  Q: Jak bude renderovat klient ktery ma ke hre linkle jen RRIllumination?
				//  A: Pouzije vlastni unwrap a mnou serializovany RRObjectIllumination
				//     (nebo si i sam muze ukladat ambient mapy).
				//  Q: Nemel by tedy Model_3ds (jako priklad externiho rendereru) zaviset jen na RRIllumination?
				//  A: Ano, mel. RRDynamicSolver pouziva jen pro pristup ke spravnym RRObjectIllumination,
				//     neni tezke to predelat.

				rr::RRObjectIllumination* objectIllumination = NULL;
				if(params.object->getChannelData(CHANNEL_TRIANGLE_OBJECT_ILLUMINATION,triangleIdx,&objectIllumination,sizeof(objectIllumination))
					&& objectIllumination!=oldIllumination)
				{
					oldIllumination = objectIllumination;
					// setup light indirect texture
					rr::RRIlluminationPixelBuffer* pixelBuffer = objectIllumination->getChannel(params.renderedChannels.LIGHT_MAP_CHANNEL)->pixelBuffer;
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
						RR_ASSERT(0);
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
				if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
				{
					if(params.scene)
					{
						rr::RRColor color;				
						params.scene->getTriangleMeasure(triangleIdx,v,RM_IRRADIANCE_CUSTOM_INDIRECT,params.scaler,color);
						glColor3fv(&color.x);
					}
					else
					{
						// solver not set, but indirect illumination requested
						// -> scene will be rendered without indirect illumination
						RR_ASSERT(0);
					}
				}

				// light indirect map uv
				if(params.renderedChannels.LIGHT_INDIRECT_MAP)
				{
					rr::RRMesh::TriangleMapping tm;
					//!!! optimize, get once, not three times per triangle
					meshImporter->getTriangleMapping(triangleIdx,tm);
					glMultiTexCoord2f(GL_TEXTURE0+de::MULTITEXCOORD_LIGHT_INDIRECT,tm.uv[v][0],tm.uv[v][1]);
				}

				// material diffuse map - uv
				if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
				{
					rr::RRVec2 uv[3];
					//!!! optimize, get once, not three times per triangle
					if(params.object->getCollider()->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV,triangleIdx,&uv,sizeof(uv)))
						glMultiTexCoord2f(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_DIFFUSE,uv[v][0],uv[v][1]);
					else
						RR_ASSERT(0); // expected data are missing
				}

				// material emissive map - uv
				if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
				{
					rr::RRVec2 uv[3];
					//!!! optimize, get once, not three times per triangle
					if(params.object->getCollider()->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV,triangleIdx,&uv,sizeof(uv)))
						glMultiTexCoord2f(GL_TEXTURE0+de::MULTITEXCOORD_MATERIAL_EMISSIVE,uv[v][0],uv[v][1]);
					else
						RR_ASSERT(0); // expected data are missing
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
