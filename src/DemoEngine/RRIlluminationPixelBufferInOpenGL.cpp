#include "DemoEngine/RRIlluminationPixelBufferInOpenGL.h"
#include "DemoEngine/Program.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Helpers

class Helpers
{
public:
	enum {
		MAX_LIGHTMAP_WIDTH = 512, //!!! hlidat shodu/preteceni
		MAX_LIGHTMAP_HEIGHT = 512,
	};
	Helpers()
	{
		tempTexture = Texture::create(NULL,MAX_LIGHTMAP_WIDTH,MAX_LIGHTMAP_HEIGHT,GL_RGBA,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		filterProgram = new Program(NULL,"shaders/lightmap_filter.vp","shaders/lightmap_filter.fp");
	}
	~Helpers()
	{
		delete filterProgram;
		delete tempTexture;
	}
	Texture* tempTexture;
	Program* filterProgram;
};

static Helpers* helpers = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationPixelBufferInOpenGL

unsigned RRIlluminationPixelBufferInOpenGL::numInstances = 0;

//RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::createInOpenGL(unsigned width, unsigned height)
//{
//	return new RRIlluminationPixelBufferInOpenGL(width,height);
//}

RRIlluminationPixelBufferInOpenGL::RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight)
{
	rendering = false;

	if(!numInstances) helpers = new Helpers();
	numInstances++;

	texture = Texture::create(NULL,awidth,aheight,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);

	/*
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D,textureId);

	//glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,width,height,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
	unsigned char* buf = new unsigned char[awidth*aheight*3];
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, awidth, aheight, GL_RGB, GL_UNSIGNED_BYTE, buf);
	delete[] buf;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	*/

	usePBO = false;
}

void RRIlluminationPixelBufferInOpenGL::renderBegin()
{
	if(rendering) 
	{
		assert(!rendering);
		return;
	}
	rendering = true;
	// backup pipeline
	glGetIntegerv(GL_VIEWPORT,viewport);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	scissorTest = glIsEnabled(GL_SCISSOR_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	if(usePBO)
	{
		//!!! set texture as render target using PBO
	}
	else
	{
		glViewport(0,0,texture->getWidth(),texture->getHeight());
		glScissor(0,0,texture->getWidth(),texture->getHeight());
		glEnable(GL_SCISSOR_TEST);
	}
	// clear to alpha=0 (color=pink, if we see it in scene, filtering or uv mapping is wrong)
	glClearColor(1,0,1,0);
	glClear(GL_COLOR_BUFFER_BIT);
	// setup pipeline
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	if(glUseProgram) glUseProgram(0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
}

void RRIlluminationPixelBufferInOpenGL::renderTriangle(const IlluminatedTriangle& it)
{
	assert(rendering);
	if(!rendering) return;
	glBegin(GL_TRIANGLES);
	for(unsigned v=0;v<3;v++)
	{
		glColor3fv(&it.iv[v].measure[0]);
		glVertex2f(it.iv[v].texCoord[0]*2-1,it.iv[v].texCoord[1]*2-1);
	}
	glEnd();
}

//void RRIlluminationPixelBufferInOpenGL::renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles)
//{
//	assert(rendering);
//	if(!rendering) return;
//!!! optimized version with interleaved array
//}

void RRIlluminationPixelBufferInOpenGL::renderEnd()
{
	if(!rendering) 
	{
		assert(rendering);
		return;
	}
	rendering = false;
	if(usePBO)
	{
		//!!! unset texture as render target using PBO
	}
	else
	{
		assert(texture->getWidth()==helpers->tempTexture->getWidth());
		helpers->tempTexture->bindTexture();
		glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,texture->getWidth(),texture->getHeight());
		//!!! kricet pokud je okno mensi nez textura, v lightmape pak je kus bily s facy sviti
	}

	// fill unused(pink) pixels
	//!!! use alpha test
	helpers->filterProgram->useIt();
	helpers->filterProgram->sendUniform("lightmap",0);
	helpers->filterProgram->sendUniform("pixelDistance",1.0f/texture->getWidth(),1.0f/texture->getHeight());
	glBegin(GL_POLYGON);
	glVertex2f(-1,-1);
	glVertex2f(1,-1);
	glVertex2f(1,1);
	glVertex2f(-1,1);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	bindTexture();
	glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,texture->getWidth(),texture->getHeight());

	// restore pipeline
	if(!scissorTest) glDisable(GL_SCISSOR_TEST);
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
}

void RRIlluminationPixelBufferInOpenGL::bindTexture()
{
	texture->bindTexture();
	//glBindTexture(GL_TEXTURE_2D, textureId);
}

RRIlluminationPixelBufferInOpenGL::~RRIlluminationPixelBufferInOpenGL()
{
	delete texture;
	//glDeleteTextures(1, &textureId);

	numInstances--;
	if(!numInstances)
	{
		delete helpers;
		helpers = NULL;
	}
}


} // namespace
