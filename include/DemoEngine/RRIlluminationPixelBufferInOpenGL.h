#include "RRIllumination.h"
#include "DemoEngine/Texture.h"
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

	class RR_API RRIlluminationPixelBufferInOpenGL : public RRIlluminationPixelBuffer
	{
	public:
		RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight)
		{
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

			//texture = Texture::create(buf,awidth,aheight,GL_RGB);

			usePBO = false;
			width = awidth;
			height = aheight;
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
				glScissor(0,0,width,height);
				glEnable(GL_SCISSOR_TEST);
			}
			// clear to pink
			glClearColor(1,0,1,0);
			glClear(GL_COLOR_BUFFER_BIT);
			// setup pipeline
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			if(glUseProgram) glUseProgram(0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
		}
		virtual void renderTriangle(const IlluminatedTriangle& it)
		{
			glBegin(GL_TRIANGLES);
			for(unsigned v=0;v<3;v++)
			{
				glColor3fv(&it.iv[v].measure[0]);
				glVertex2f(it.iv[v].texCoord[0]*2-1,it.iv[v].texCoord[1]*2-1);
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
				bindTexture();
				glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width,height);
			}
			//!!! fill unused(pink) pixels
			// restore pipeline
			//!!! hack for fcss
			glDisable(GL_SCISSOR_TEST);
			glViewport(0,0,640,480);
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);
		}
		virtual void bindTexture()
		{
			//texture->bindTexture();
			glBindTexture(GL_TEXTURE_2D, textureId);
		}
		virtual ~RRIlluminationPixelBufferInOpenGL()
		{
			//delete texture;
			glDeleteTextures(1, &textureId);
		}
	private:
		//Texture* texture;
		GLuint textureId;
		unsigned width;
		unsigned height;
		bool usePBO;
	};

} // namespace
