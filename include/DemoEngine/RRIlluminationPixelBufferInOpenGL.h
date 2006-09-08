#include "RRIllumination.h"
#include <GL/glew.h>

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in pixel buffer in OpenGL texture.
	//
	//! Uses OpenGL rasterizer.
	//! Depends on OpenGL, can't be used on Xbox and Xbox 360.
	//! Warning: doesn't restore changed GL states.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRIlluminationPixelBufferInOpenGL : public RRIlluminationPixelBuffer
	{
	public:
		RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight)
		{
			glGenTextures(1, &textureId);
			usePBO = false;
			width = awidth;
			height = aheight;
			glBindTexture(GL_TEXTURE_2D,textureId);
			glTexImage2D(GL_TEXTURE_2D,0,3,width,height,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
		}
		virtual void setSize(unsigned awidth, unsigned aheight)
		{
			width = awidth;
			height = aheight;
			glBindTexture(GL_TEXTURE_2D,textureId);
			glTexImage2D(GL_TEXTURE_2D,0,3,width,height,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
		}
		virtual void renderBegin()
		{
			if(usePBO)
			{
				//!!! set texture as render target using PBO
			}
			else
			{
				glViewport(0,0,width,height);
			}
			// clear to pink
			glClearColor(1,0,1,0);
			glClear(GL_COLOR_BUFFER_BIT);
			// ignore depth
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			// setup pipeline
			if(glUseProgram) glUseProgram(0);
		}
		virtual void renderTriangle(const IlluminatedTriangle& it)
		{
			glBegin(GL_TRIANGLES);
			for(unsigned v=0;v<3;v++)
			{
				glColor3fv(&it.iv[v].measure[0]);
				glVertex2fv(&it.iv[v].texCoord[0]);
			}
			glEnd();
		}
		//!!! write optimized version with interleaved array
		//virtual void renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles)
		//{
		//}
		virtual void renderEnd()
		{
			if(usePBO)
			{
				//!!! unset texture as render target using PBO
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D,textureId);
				glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width,height);
			}
			//!!! fill unused(pink) pixels
		}
		virtual void bindTexture()
		{
			glBindTexture(GL_TEXTURE_2D, textureId);
		}
		virtual ~RRIlluminationPixelBufferInOpenGL()
		{
			glDeleteTextures(1, &textureId);
		}
	private:
		GLuint textureId;
		unsigned width;
		unsigned height;
		bool usePBO;
	};

} // namespace
