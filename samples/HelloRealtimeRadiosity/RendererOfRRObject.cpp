// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "RRIllumination.h"
#include "DemoEngine/Texture.h"
#include "DemoEngine/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "3ds2rr.h" // CHANNEL_SURFACE_DIF_TEX
#include "RendererOfRRObject.h"


RendererOfRRObject::RendererOfRRObject(const rr::RRObject* objectImporter, const rr::RRScene* radiositySolver, const rr::RRScaler* scaler)
{
	params.object = objectImporter;
	params.scene = radiositySolver;
	params.scaler = scaler;
	//params.renderedChannels = ... set to default by constructor
	params.generateForcedUv = NULL;
	params.firstCapturedTriangle = 0;
	params.lastCapturedTriangle = objectImporter->getCollider()->getMesh()->getNumTriangles()-1;
}

void RendererOfRRObject::setRenderedChannels(RenderedChannels renderedChannels)
{
	params.renderedChannels = renderedChannels;
}

void RendererOfRRObject::setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle, unsigned alastCapturedTriangle)
{
	params.generateForcedUv = capture;
	params.otherCaptureParamsHash = capture ? capture->getHash() : 0;
	params.firstCapturedTriangle = afirstCapturedTriangle;
	params.lastCapturedTriangle = alastCapturedTriangle;
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

	bool begun = false;
	rr::RRMesh* meshImporter = params.object->getCollider()->getMesh();
	//unsigned numTriangles = meshImporter->getNumTriangles();
	unsigned oldSurfaceIdx = UINT_MAX;
	rr::RRObjectIllumination* oldIllumination = NULL;
	for(unsigned triangleIdx=params.firstCapturedTriangle;triangleIdx<=params.lastCapturedTriangle;triangleIdx++)
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
		bool setNormals = params.renderedChannels.LIGHT_DIRECT || params.renderedChannels.LIGHT_INDIRECT_ENV;
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
