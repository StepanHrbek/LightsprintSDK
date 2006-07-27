#ifndef TEXTURESHADOWMAP_H
#define TEXTURESHADOWMAP_H

#include "Texture.h"

#define SHADOW_MAP_SIZE 512


/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

class TextureShadowMap : public Texture
{
public:
	TextureShadowMap()
		: Texture(NULL, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, GL_DEPTH_COMPONENT, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
		channels = 1;
		// for shadow2D() instead of texture2D()
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
	}
	// sideeffect: binds texture
	void readFromBackbuffer()
	{
		//glGetIntegerv(GL_TEXTURE_2D,&oldTextureObject);
		bindTexture();
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height); // painfully slow on ATI (X800 PRO, Catalyst 6.6)
	}
};


#endif
