#ifndef TEXTURESHADOWMAP_H
#define TEXTURESHADOWMAP_H

#include "Texture.h"

#define SHADOW_MAP_SIZE 512

#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}


/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

class TextureShadowMap : public Texture
{
public:
	TextureShadowMap();
	void renderingToInit();
	void renderingToDone(); // sideeffect: binds texture
	static unsigned getDepthBits(); // number of bits in texture depth channel
private:
	static bool useFBO;
	static GLuint fb;
	static GLuint depth_rb;
	static GLint depthBits;
	static void oneTimeFBOInit();
};


#endif
