// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/GL/UberProgramSetup.h" // texture/multitexcoord id assignments
#include "ObjectBuffers.h"
#include "PreserveState.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject

RendererOfRRObject* RendererOfRRObject::create(const rr::RRObject* object, rr::RRDynamicSolver* solver, const rr::RRScaler* scaler, bool useBuffers)
{
	if(object && object->getCollider()->getMesh()->getNumTriangles())
		return new RendererOfRRObject(object,solver,scaler,useBuffers);
	else
		return NULL;
}

RendererOfRRObject::RendererOfRRObject(const rr::RRObject* _object, rr::RRDynamicSolver* _solver, const rr::RRScaler* _scaler, bool _useBuffers)
{
	RR_ASSERT(_object);
	params.program = NULL;
	params.object = _object;
	params.scene = _solver;
	params.scaler = _scaler;
	//params.renderedChannels = ... set to default by constructor
	params.generateForcedUv = NULL;
	params.firstCapturedTriangle = 0;
	params.lastCapturedTrianglePlus1 = _object->getCollider()->getMesh()->getNumTriangles();
	params.indirectIlluminationSource = NONE;
	params.indirectIlluminationLayer = 0;
	params.indirectIlluminationLayer2 = 0;
	params.indirectIlluminationBlend = 0;
	params.indirectIlluminationLayerFallback = 0;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	params.renderingFromThisLight = NULL;
	params.renderingLitByThisLight = NULL;
	params.honourExpensiveLightingShadowingFlags = false;

	indexedYes = NULL;
	indexedNo = NULL;
	if(_useBuffers)
	{
		indexedYes = ObjectBuffers::create(_object,true);
		indexedNo = ObjectBuffers::create(_object,false);
	}
}

RendererOfRRObject::~RendererOfRRObject()
{
	delete indexedNo;
	delete indexedYes;
}

void RendererOfRRObject::setProgram(Program* program)
{
	params.program = program;
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

void RendererOfRRObject::setIndirectIlluminationBuffers(rr::RRBuffer* vertexBuffer,const rr::RRBuffer* ambientMap)
{
	params.indirectIlluminationSource = BUFFERS;
	params.indirectIlluminationLayer = 0;
	params.indirectIlluminationLayer2 = 0;
	params.indirectIlluminationBlend = 0;
	params.indirectIlluminationLayerFallback = 0;
	params.availableIndirectIlluminationVColors = vertexBuffer;
	params.availableIndirectIlluminationMap = ambientMap;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	solutionVersion = 0;
}

void RendererOfRRObject::setIndirectIlluminationBuffersBlend(rr::RRBuffer* vertexBuffer, const rr::RRBuffer* ambientMap, rr::RRBuffer* vertexBuffer2, const rr::RRBuffer* ambientMap2)
{
	params.indirectIlluminationSource = BUFFERS;
	params.indirectIlluminationLayer = 0;
	params.indirectIlluminationLayer2 = 0;
	params.indirectIlluminationBlend = 0;
	params.indirectIlluminationLayerFallback = 0;
	params.availableIndirectIlluminationVColors = vertexBuffer;
	params.availableIndirectIlluminationMap = ambientMap;
	params.availableIndirectIlluminationVColors2 = vertexBuffer2;
	params.availableIndirectIlluminationMap2 = ambientMap2;
	solutionVersion = 0;
}

void RendererOfRRObject::setIndirectIlluminationLayer(unsigned layerNumber)
{
	params.indirectIlluminationSource = LAYER;
	params.indirectIlluminationLayer = layerNumber;
	params.indirectIlluminationLayer2 = 0;
	params.indirectIlluminationBlend = 0;
	params.indirectIlluminationLayerFallback = 0;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	solutionVersion = 0;
}

void RendererOfRRObject::setIndirectIlluminationLayerBlend(unsigned layerNumber, unsigned layerNumber2, float layerBlend, unsigned layerNumberFallback)
{
	// never used yet
	params.indirectIlluminationSource = LAYER;
	params.indirectIlluminationLayer = layerNumber;
	params.indirectIlluminationLayer2 = layerNumber2;
	params.indirectIlluminationBlend = layerBlend;
	params.indirectIlluminationLayerFallback = layerNumberFallback;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	solutionVersion = 0;
}

void RendererOfRRObject::setIndirectIlluminationFromSolver(unsigned asolutionVersion)
{
	params.indirectIlluminationSource = SOLVER;
	params.indirectIlluminationLayer = 0;
	params.indirectIlluminationLayer2 = 0;
	params.indirectIlluminationBlend = 0;
	params.indirectIlluminationLayerFallback = 0;
	params.availableIndirectIlluminationVColors = NULL;
	params.availableIndirectIlluminationMap = NULL;
	params.availableIndirectIlluminationVColors2 = NULL;
	params.availableIndirectIlluminationMap2 = NULL;
	solutionVersion = asolutionVersion;
}

void RendererOfRRObject::setLightingShadowingFlags(const rr::RRLight* renderingFromThisLight, const rr::RRLight* renderingLitByThisLight, bool honourExpensiveLightingShadowingFlags)
{
	params.renderingFromThisLight = renderingFromThisLight;
	params.renderingLitByThisLight = renderingLitByThisLight;
	params.honourExpensiveLightingShadowingFlags = honourExpensiveLightingShadowingFlags;
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

	// vcolor2 allowed only if vcolor is set
	// map2 allowed only if map is set
	RR_ASSERT(params.renderedChannels.LIGHT_INDIRECT_VCOLOR || !params.renderedChannels.LIGHT_INDIRECT_VCOLOR2);
	RR_ASSERT(params.renderedChannels.LIGHT_INDIRECT_MAP || !params.renderedChannels.LIGHT_INDIRECT_MAP2);

	// indirect illumination source - solver, buffers or none?
	bool renderIndirect = params.renderedChannels.LIGHT_INDIRECT_VCOLOR || params.renderedChannels.LIGHT_INDIRECT_MAP;
	bool readIndirectFromSolver = renderIndirect && params.indirectIlluminationSource==SOLVER;
	bool readIndirectFromBuffers = renderIndirect && params.indirectIlluminationSource==BUFFERS;
	bool readIndirectFromLayer = renderIndirect && params.indirectIlluminationSource==LAYER;
	bool readIndirectFromNone = renderIndirect && params.indirectIlluminationSource==NONE;

	bool setNormals = params.renderedChannels.LIGHT_DIRECT || params.renderedChannels.LIGHT_INDIRECT_ENV || params.renderedChannels.NORMALS;

	// BUFFERS
	// general and faster code, but can't handle objects with big preimport vertex numbers (e.g. multiobject)
	// -> automatic fallback to !BUFFERS

	bool dontUseIndexed = 0;
	bool dontUseNonIndexed = 0;
	bool dontUseNonBuffered = 0;

	if(params.honourExpensiveLightingShadowingFlags)
	{
		dontUseIndexed = true;
		dontUseNonIndexed = true;
	}
	// buffer generated by RRDynamicSolver need original (preimport) vertex order, it is only in indexed
	if(readIndirectFromBuffers && params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
	{
		dontUseNonIndexed = true;
		dontUseNonBuffered = true;
	}
	// ObjectBuffers can't create indexed buffer from solver
	if(readIndirectFromSolver)
	{
		dontUseIndexed = true;
	}
	// ambient maps are sometimes incompatible with indexed render (fallback uv generated by us)
	//!!! if possible, ask RRMesh about mapping/unwrap compatibility with indexed render. our unwrap is incompatible, others should be compatible
	if(readIndirectFromBuffers && params.renderedChannels.LIGHT_INDIRECT_MAP)
	{
		dontUseIndexed = true;
	}
	// indirectFromSolver can't generate ambient maps
	if(readIndirectFromSolver && params.renderedChannels.LIGHT_INDIRECT_MAP)
	{
		RR_ASSERT(0);
		return;
	}
	// FORCE_2D_POSITION vertices must be unique, incompatible with indexed buffer
	if(params.renderedChannels.FORCE_2D_POSITION)
	{
		dontUseIndexed = true;
	}
	// only nonbuffered can read from arbitrary layer
	if(readIndirectFromLayer)
	{
		dontUseIndexed = true;
		dontUseNonIndexed = true;
	}
	// error, you forgot to call setIndirectIlluminationXxx()
	if(readIndirectFromNone)
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

	if(indexedYes && !dontUseIndexed)
	{
		// INDEXED
		// indexed is faster, use always when possible
		indexedYes->render(params,solutionVersion);
	}
	else
	if(indexedNo && !dontUseNonIndexed)
	{
		// NON INDEXED
		// non-indexed is slower
		indexedNo->render(params,solutionVersion);
	}
	else
	if(!dontUseNonBuffered)
	{
		// NON BUFFERED
		// general, but slower code
		// reads indirect vertex illumination always from solver, indirect maps always from layer

		bool begun = false;
		const rr::RRMesh* meshImporter = params.object->getCollider()->getMesh();
		//unsigned numTriangles = meshImporter->getNumTriangles();
		const rr::RRMaterial* oldMaterial = NULL;
		rr::RRObjectIllumination* oldIllumination = NULL;
		GLint materialDiffuseVColorIndex;
		GLint materialEmissiveVColorIndex;
		{
			GLint program;
			glGetIntegerv(GL_CURRENT_PROGRAM,&program);
			materialDiffuseVColorIndex = glGetAttribLocation(program,"materialDiffuseVColor");
			materialEmissiveVColorIndex = glGetAttribLocation(program,"materialEmissiveVColor");
		}
		//rr::RRReporter::report(rr::INF1,"------------------------------------------------------------------------------------------------------------");
		for(unsigned triangleIdx=params.firstCapturedTriangle;triangleIdx<params.lastCapturedTrianglePlus1;triangleIdx++)
		{
			rr::RRMesh::Triangle tri;
			meshImporter->getTriangle(triangleIdx,tri);

			{
				const rr::RRMaterial* material;
				if(params.renderingFromThisLight)
				{
					// rendering into shadowmap, so skipping render=disabling shadow
					// kdyz pro aspon 1 receiver true, kreslit, tj.vrhat stiny (muze byt nepresne, ale nebudu delat extra shadowmapu pro kazdy receiver)
					for(unsigned i=0;i<params.scene->getNumObjects();i++)
					{
						if(material = params.object->getTriangleMaterial(triangleIdx,params.renderingFromThisLight,params.scene->getObject(i)))
							break;
					}
				}
				else
				{
					// a) rendering indirect, so no skipping, everything is rendered
					// b) accumulating lit renders, so skipping render=disabling lighting (skip allowed in detection, background is cleared to black so skip=render unlit)
					material = params.object->getTriangleMaterial(triangleIdx,params.renderingLitByThisLight,NULL);
				}
				//rr::RRMesh::PreImportNumber preTriangle = meshImporter->getPreImportTriangle(triangleIdx);
				//rr::RRReporter::report(rr::INF1,"%s %s tri%d(obj%d)",params.renderingLitByThisLight?"SHADOWMAP":(params.renderedChannels.FORCE_2D_POSITION?"DETECT":"FINAL_LIT"),material?"render":"skip",triangleIdx,preTriangle.object);
				if(!material) continue; // skip rendering triangles without material

				if(material!=oldMaterial)
				{
					oldMaterial = material;

					// face culling
					if(params.renderedChannels.MATERIAL_CULLING)
					{
						if(begun)
						{
							glEnd();
							begun = false;
						}
						if(!material || (material->sideBits[0].renderFrom && material->sideBits[1].renderFrom))
						{
							glDisable(GL_CULL_FACE);
						}
						else
						{
							glEnable(GL_CULL_FACE);
							glCullFace(material->sideBits[0].renderFrom?GL_BACK:( material->sideBits[1].renderFrom?GL_FRONT:GL_FRONT_AND_BACK ));
						}
					}

					// blending
					if(params.renderedChannels.MATERIAL_TRANSPARENCY_CONST || params.renderedChannels.MATERIAL_TRANSPARENCY_MAP)
					{
						if(begun)
						{
							glEnd();
							begun = false;
						}
						if(material && material->specularTransmittance.avg())
						{
							// current blendfunc is used, caller is responsible for setting it
//							glEnable(GL_BLEND);
							// current alpha func+ref is used, caller is responsible for setting it
							glEnable(GL_ALPHA_TEST); // alpha test is used so we don't have to sort objects
						}
						else
						{
//							glDisable(GL_BLEND);
							glDisable(GL_ALPHA_TEST);
						}
					}

					// material diffuse color
					if(params.renderedChannels.MATERIAL_DIFFUSE_CONST)
					{
						if(material && params.program)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							params.program->sendUniform("materialDiffuseConst",material->diffuseReflectance[0],material->diffuseReflectance[1],material->diffuseReflectance[2],1-material->specularTransmittance.avg());
						}
						else
						{
							RR_ASSERT(0); // expected data are missing
						}
					}

					// material diffuse vcolor
					if(params.renderedChannels.MATERIAL_DIFFUSE_VCOLOR)
					{
						if(material)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glVertexAttrib4f(materialDiffuseVColorIndex,material->diffuseReflectance[0],material->diffuseReflectance[1],material->diffuseReflectance[2],1-material->specularTransmittance.avg());
						}
						else
						{
							RR_ASSERT(0); // expected data are missing
						}
					}

					// material diffuse map - bind texture
					if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
					{
						rr::RRBuffer* tex = NULL;
						params.object->getChannelData(rr::RRObject::CHANNEL_TRIANGLE_DIFFUSE_TEX,triangleIdx,&tex,sizeof(tex));
						if(tex)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_DIFFUSE);
							getTexture(tex)->bindTexture();
						}
						else
						{			
							RR_ASSERT(0); // expected data are missing
						}
					}

					// material emissive color
					if(params.renderedChannels.MATERIAL_EMISSIVE_CONST)
					{
						if(material && params.program)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							params.program->sendUniform("materialEmissiveConst",material->diffuseEmittance[0],material->diffuseEmittance[1],material->diffuseEmittance[2],0.0f);
						}
						else
						{
							RR_ASSERT(0); // expected data are missing
						}
					}


					// material emissive vcolor
					if(params.renderedChannels.MATERIAL_EMISSIVE_VCOLOR)
					{
						if(material)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glVertexAttrib4f(materialEmissiveVColorIndex,material->diffuseEmittance[0],material->diffuseEmittance[1],material->diffuseEmittance[2],0);
						}
						else
						{
							RR_ASSERT(0); // expected data are missing
						}
					}
					// material emissive map - bind texture
					if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
					{
						Texture* tex = NULL;
						params.object->getChannelData(rr::RRObject::CHANNEL_TRIANGLE_EMISSIVE_TEX,triangleIdx,&tex,sizeof(tex));
						if(tex)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_EMISSIVE);
							tex->bindTexture();
						}
						else
						{			
							RR_ASSERT(0); // expected data are missing
						}
					}
					// material transparency rgb map - bind texture
					if(params.renderedChannels.MATERIAL_TRANSPARENCY_MAP)
					{
						Texture* tex = NULL;
						params.object->getChannelData(rr::RRObject::CHANNEL_TRIANGLE_TRANSPARENCY_TEX,triangleIdx,&tex,sizeof(tex));
						if(tex)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_TRANSPARENCY);
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

				rr::RRObjectIllumination* objectIllumination = NULL;
				if(params.object->getChannelData(rr::RRObject::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION,triangleIdx,&objectIllumination,sizeof(objectIllumination))
					&& objectIllumination!=oldIllumination)
				{
					oldIllumination = objectIllumination;
					// setup light indirect texture
					rr::RRBuffer* pixelBuffer = objectIllumination->getLayer(params.indirectIlluminationLayer);
					if(pixelBuffer && pixelBuffer->getType()==rr::BT_2D_TEXTURE)
					{
						if(begun)
						{
							glEnd();
							begun = false;
						}
						glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
						getTexture(pixelBuffer)->bindTexture();
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
					}
					else
					{
						//!!! kdyz se dostane sem a vysledek zacachuje, bude to uz vzdycky renderovat blbe
						RR_ASSERT(0);
					}
					// setup light indirect texture2
					if(params.renderedChannels.LIGHT_INDIRECT_MAP2)
					{
						rr::RRBuffer* pixelBuffer2 = objectIllumination->getLayer(params.indirectIlluminationLayer2);
						if(pixelBuffer2 && pixelBuffer2->getType()==rr::BT_2D_TEXTURE)
						{
							if(begun)
							{
								glEnd();
								begun = false;
							}
							glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT2);
							getTexture(pixelBuffer2)->bindTexture();
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						}
						else
						{
							//!!! kdyz se dostane sem a vysledek zacachuje, bude to uz vzdycky renderovat blbe
							RR_ASSERT(0);
						}
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
					glNormal3fv(&triangleNormals.vertex[v].normal.x);
				}

				// light indirect color
				if(params.renderedChannels.LIGHT_INDIRECT_VCOLOR)
				{
					if(params.scene)
					{
						rr::RRVec3 color;				
						params.scene->getTriangleMeasure(triangleIdx,v,RM_IRRADIANCE_PHYSICAL_INDIRECT,color);
						glColor3fv(&color.x);
					}
					else
					{
						// solver not set, but indirect illumination requested
						// -> scene will be rendered without indirect illumination
						RR_ASSERT(0);
						glColor3b(0,0,0);
					}
				}

				// light indirect map uv
				if(params.renderedChannels.LIGHT_INDIRECT_MAP)
				{
					rr::RRMesh::TriangleMapping tm;
					//!!! optimize, get once, not three times per triangle
					meshImporter->getTriangleMapping(triangleIdx,tm);
					glMultiTexCoord2f(GL_TEXTURE0+MULTITEXCOORD_LIGHT_INDIRECT,tm.uv[v][0],tm.uv[v][1]);
				}

				// material diffuse map - uv
				if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
				{
					rr::RRVec2 uv[3];
					//!!! optimize, get once, not three times per triangle
					if(params.object->getChannelData(rr::RRObject::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV,triangleIdx,&uv,sizeof(uv)))
						glMultiTexCoord2f(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_DIFFUSE,uv[v][0],uv[v][1]);
					else
						RR_ASSERT(0); // expected data are missing
				}

				// material emissive map - uv
				if(params.renderedChannels.MATERIAL_EMISSIVE_MAP)
				{
					rr::RRVec2 uv[3];
					//!!! optimize, get once, not three times per triangle
					if(params.object->getChannelData(rr::RRObject::CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV,triangleIdx,&uv,sizeof(uv)))
						glMultiTexCoord2f(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_EMISSIVE,uv[v][0],uv[v][1]);
					else
						RR_ASSERT(0); // expected data are missing
				}

				// material transparency map - uv
				if(params.renderedChannels.MATERIAL_TRANSPARENCY_MAP)
				{
					rr::RRVec2 uv[3];
					//!!! optimize, get once, not three times per triangle
					if(params.object->getChannelData(rr::RRObject::CHANNEL_TRIANGLE_VERTICES_TRANSPARENCY_UV,triangleIdx,&uv,sizeof(uv)))
						glMultiTexCoord2f(GL_TEXTURE0+MULTITEXCOORD_MATERIAL_TRANSPARENCY,uv[v][0],uv[v][1]);
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
						glMultiTexCoord2f(GL_TEXTURE0+MULTITEXCOORD_FORCED_2D,xy[0],xy[1]);
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
	else
	{
		// NONE
		// none of rendering techniques can do this job
		RR_ASSERT(0);
	}
}

}; // namespace
