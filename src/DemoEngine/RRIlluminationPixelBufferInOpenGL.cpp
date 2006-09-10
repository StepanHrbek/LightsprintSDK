#include "DemoEngine/RRIlluminationPixelBufferInOpenGL.h"
#include "DemoEngine/Program.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Helpers, one instance for all RRIlluminationPixelBufferInOpenGL

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
	glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor);

	//helpers->tempTexture->renderingToBegin();
	texture->renderingToBegin();

	glViewport(0,0,texture->getWidth(),texture->getHeight());
	glScissor(0,0,texture->getWidth(),texture->getHeight());
	glEnable(GL_SCISSOR_TEST);
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
	//!!! potrebuju aby tempTexture mela presne stejnej rozmer jako texture
	assert(texture->getWidth()==helpers->tempTexture->getWidth());

	// fill unused pixels (alpha==0)
	//!!! use alpha test
	helpers->filterProgram->useIt();
	helpers->filterProgram->sendUniform("lightmap",0);
	helpers->filterProgram->sendUniform("pixelDistance",1.0f/texture->getWidth(),1.0f/texture->getHeight());

	helpers->tempTexture->renderingToBegin();
	texture->bindTexture();

	glBegin(GL_POLYGON);
	glVertex2f(-1,-1);
	glVertex2f(1,-1);
	glVertex2f(1,1);
	glVertex2f(-1,1);
	glEnd();

	texture->renderingToBegin();
	helpers->tempTexture->bindTexture();

	glBegin(GL_POLYGON);
	glVertex2f(-1,-1);
	glVertex2f(1,-1);
	glVertex2f(1,1);
	glVertex2f(-1,1);
	glEnd();

	texture->renderingToEnd();

	// restore pipeline
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]);
	if(!scissorTest) glDisable(GL_SCISSOR_TEST);
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
}

void RRIlluminationPixelBufferInOpenGL::bindTexture()
{
	texture->bindTexture();
}

RRIlluminationPixelBufferInOpenGL::~RRIlluminationPixelBufferInOpenGL()
{
	delete texture;

	numInstances--;
	if(!numInstances)
	{
		delete helpers;
		helpers = NULL;
	}
}

} // namespace
