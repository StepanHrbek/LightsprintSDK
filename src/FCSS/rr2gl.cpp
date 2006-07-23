//
// OpenGL renderer of RRObject by Stepan Hrbek, dee@mail.cz
//

#include "rr2gl.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include <assert.h>
#include <math.h>
#include "3ds2rr.h" // additional diffuse_texture channels in RRObject


int   SIDES  =1; // 1,2=force all faces 1/2-sided, 0=let them as specified by surface
bool  SMOOTH =1; // allow multiple normals in polygon if mgf specifies (otherwise whole polygon gets one normal)
bool  COMPILE=1;

#define MINUS(a,b,res) res[0]=a[0]-b[0];res[1]=a[1]-b[1];res[2]=a[2]-b[2]
#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
#define DIV(a,b,res) res[0]=a[0]/(b);res[1]=a[1]/(b);res[2]=a[2]/(b)
#define MIN(a,b) (((a)<(b))?(a):(b))

VertexDataGenerator* generateForcedUv = NULL;


//////////////////////////////////////////////////////////////////////////////
//
// RRRenderer

const void* RRRenderer::getParams(unsigned& length) const
{
	length = 0;
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRGLObjectRenderer

RRGLObjectRenderer::RRGLObjectRenderer(rr::RRObject* objectImporter, rr::RRScene* radiositySolver)
{
	params.object = objectImporter;
	params.scene = radiositySolver;
}

void RRGLObjectRenderer::setRenderedChannels(RenderedChannels renderedChannels)
{
	params.renderedChannels = renderedChannels;
}

const void* RRGLObjectRenderer::getParams(unsigned& length) const
{
	length = sizeof(params);
	return &params;
}

void RRGLObjectRenderer::render()
{
	glColor4ub(0,0,0,255);
	//glEnable(GL_CULL_FACE);
	if(SIDES==1) glEnable(GL_CULL_FACE);
	if(SIDES==2) glDisable(GL_CULL_FACE);

	glBegin(GL_TRIANGLES);
	assert(params.object); //!!! rrview514/gcc ho hodi u raista
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
			glActiveTextureARB(GL_TEXTURE12_ARB); // used by lightIndirectMap
			...
			?->bindTexture();
			glActiveTextureARB(GL_TEXTURE11_ARB); // used by materialDiffuseMap
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
				if(generateForcedUv)
				{
					GLfloat xy[2];
					generateForcedUv->generateData(triangleIdx, v, xy, sizeof(xy));
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


//////////////////////////////////////////////////////////////////////////////
//
// RRGLCachingRenderer

RRGLCachingRenderer::RRGLCachingRenderer(RRRenderer* arenderer)
{
	renderer = arenderer;
}

RRGLCachingRenderer::~RRGLCachingRenderer()
{
	for(Map::iterator i=mapa.begin();i!=mapa.end();i++)
	{
		setStatus(CS_NEVER_COMPILE,i->second);
	}
}

RRGLCachingRenderer::Info& RRGLCachingRenderer::findInfo()
{
	Key key;
	memset(&key,0,sizeof(key));
	unsigned length;
	const void* params = renderer->getParams(length);
	if(length)
	{
		//!!! params delsi nez 16 jsou oriznuty
		memcpy(&key,params,MIN(length,sizeof(key)));
	}
//	Map::iterator i = mapa.find(key);
//	if(i!=mapa.end())
//		return (Info&)i->second;
	return mapa[key];
}

void RRGLCachingRenderer::setStatus(ChannelStatus cs,RRGLCachingRenderer::Info& info)
{
	if(info.status==CS_COMPILED && cs!=CS_COMPILED)
	{
		assert(info.displayList!=UINT_MAX);
		glDeleteLists(info.displayList,1);
		info.displayList = UINT_MAX;
	}
	if(info.status!=CS_COMPILED && cs==CS_COMPILED)
	{
		assert(info.displayList==UINT_MAX);
		cs = CS_READY_TO_COMPILE;
	}
	info.status = cs;
}

void RRGLCachingRenderer::setStatus(ChannelStatus cs)
{
	setStatus(cs,findInfo());
}

void RRGLCachingRenderer::render()
{
	RRGLCachingRenderer::Info& info = findInfo();
	switch(info.status)
	{
	case CS_READY_TO_COMPILE:
		if(!COMPILE) goto never;
		assert(info.displayList==UINT_MAX);
		info.displayList = glGenLists(1);
		glNewList(info.displayList,GL_COMPILE);
		renderer->render();
		glEndList();
		info.status = CS_COMPILED;
		// intentionally no break
	case CS_COMPILED:
		assert(info.displayList!=UINT_MAX);
		glCallList(info.displayList);
		break;
	case CS_NEVER_COMPILE:
never:
		assert(info.displayList==UINT_MAX);
		renderer->render();
		break;
	default:
		assert(0);
	}
}
