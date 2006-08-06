#include <assert.h>
#include <GL/glew.h>
#include "DemoEngine/3ds2rr.h" // CHANNEL_SURFACE_DIF_TEX
#include <GL/glut.h>
#include "DemoEngine/RendererOfRRObject.h"
#include "DemoEngine/Texture.h"

int   SIDES  =1; // 1,2=force all faces 1/2-sided, 0=let them as specified by surface
bool  SMOOTH =1; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)


RendererOfRRObject::RendererOfRRObject(rr::RRObject* objectImporter, rr::RRScene* radiositySolver)
{
	params.object = objectImporter;
	params.scene = radiositySolver;
	//params.renderedChannels = ... set to default by constructor
	params.generateForcedUv = NULL;
	params.firstCapturedTriangle = 0;
}

void RendererOfRRObject::setRenderedChannels(RenderedChannels renderedChannels)
{
	params.renderedChannels = renderedChannels;
}

void RendererOfRRObject::setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle)
{
	params.generateForcedUv = capture;
	params.firstCapturedTriangle = afirstCapturedTriangle;
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
	//glEnable(GL_CULL_FACE);
	if(SIDES==1) glEnable(GL_CULL_FACE);
	if(SIDES==2) glDisable(GL_CULL_FACE);

	glBegin(GL_TRIANGLES);
	rr::RRMesh* meshImporter = params.object->getCollider()->getMesh();
	unsigned numTriangles = meshImporter->getNumTriangles();
	unsigned oldSurfaceIdx = UINT_MAX;
	for(unsigned triangleIdx=0;triangleIdx<numTriangles;triangleIdx++)
	{
		rr::RRMesh::Triangle tri;
		meshImporter->getTriangle(triangleIdx,tri);

		if(params.renderedChannels.MATERIAL_DIFFUSE_COLOR || params.renderedChannels.MATERIAL_DIFFUSE_MAP)
		{
				unsigned surfaceIdx = params.object->getTriangleSurface(triangleIdx);
				if(surfaceIdx!=oldSurfaceIdx)
				{
					oldSurfaceIdx = surfaceIdx;

					// material culling
					// nastavuje culling podle materialu
					// vypnuto protoze kdyz to na nvidii vlozim do display listu, pri jeho provadeni hlasi error
					// !!! mozna byl error proto ze je to zakazane uvnitr glBegin/End
					//if(cc!=CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION)
					//	if((SIDES==0 && surface->sideBits[1].renderFrom) || SIDES==1) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

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
						Texture* tex = NULL;
						params.object->getCollider()->getMesh()->getChannelData(CHANNEL_SURFACE_DIF_TEX,surfaceIdx,&tex,sizeof(tex));
						if(tex)
						{
							glEnd();
							tex->bindTexture();
							glBegin(GL_TRIANGLES);
						}
						else
						{			
							assert(0); // expected data are missing
						}
					}
				}
		}

		// mesh normals - prepare data
		bool setNormals = params.renderedChannels.LIGHT_DIRECT;
		rr::RRObject::TriangleNormals triangleNormals;
		if(setNormals)
		{
			params.object->getTriangleNormals(triangleIdx,triangleNormals);
		}

		for(int v=0;v<3;v++)
		{
			// mesh normals - set
			if(setNormals && (SMOOTH || v==0))
			{
				glNormal3fv(&triangleNormals.norm[v].x);
			}

			// light indirect color
			if(params.renderedChannels.LIGHT_INDIRECT_COLOR)
			{
				rr::RRColor color;				
				params.scene->getTriangleMeasure(0,triangleIdx,v,rr::RM_IRRADIANCE,color);
				glColor3fv(&color.x);
			}

			// light indirect map
			//!!! not implemented
			/*
			// setup light indirect texture
			rr::RRIlluminationPixelBuffer* pixelBuffer = app->getIllumination(i)->getChannel(0)->pixelBuffer;
			const rr::RRColorI8* pixels = ((rr::RRIlluminationPixelBufferInMemory<rr::RRColorI8>*)pixelBuffer)->lock();
			glActiveTextureARB(GL_TEXTURE12); // used by lightIndirectMap
			...
			?->bindTexture();
			glActiveTextureARB(GL_TEXTURE11); // used by materialDiffuseMap
			// if not created yet, create unwrap buffer
			rr::RRObjectIllumination& illum = app->getIllumination(i);
			if(!illum.pixelBufferUnwrap)
			{
			illum.createPixelBufferUnwrap(app->getObject(i));
			}
			// setup light indirect texture coords
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, 0, ?);
			*/

			// material diffuse map - uv
			if(params.renderedChannels.MATERIAL_DIFFUSE_MAP)
			{
				rr::RRVec2 uv[3];
				//!!! getnout jednou, ne trikrat (viz setNormals)
				if(params.object->getCollider()->getMesh()->getChannelData(CHANNEL_TRIANGLE_VERTICES_DIF_UV,triangleIdx,&uv,sizeof(uv)))
					glMultiTexCoord2f(GL_TEXTURE0,uv[v][0],uv[v][1]);
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
					glMultiTexCoord2f(GL_TEXTURE7,xy[0],xy[1]);
				}
			}

			// mesh positions
			rr::RRMesh::Vertex vertex;
			meshImporter->getVertex(tri.m[v],vertex);
			glVertex3fv(&vertex.x);
		}
	}
	glEnd();
}

