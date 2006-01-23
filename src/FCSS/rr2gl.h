//
// OpenGL renderer of RRObjectImporter by Stepan Hrbek, dee@mail.cz
//

#ifndef _RR2GL_H
#define _RR2GL_H

#include "RRVision.h"


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectRenderer - interface

class RRObjectRenderer
{
public:
	virtual ~RRObjectRenderer() {};

	enum ColorChannel
	{
		CC_NO_COLOR,
		//CC_NO_COLOR__DIFFUSE_UV,
		CC_TRIANGLE_INDEX,
		CC_DIFFUSE_REFLECTANCE,
		CC_DIFFUSE_REFLECTANCE_FORCED_2D_POSITION,
		CC_SOURCE_IRRADIANCE,
		CC_SOURCE_EXITANCE,
		CC_REFLECTED_IRRADIANCE,
		//CC_REFLECTED_IRRADIANCE__DIFFUSE_UV,
		CC_REFLECTED_EXITANCE,
		CC_LAST,
		CC_SOURCE_AUTO,
		CC_REFLECTED_AUTO,
	};
	virtual void render(ColorChannel cc) = 0;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRGLObjectRenderer - basic OpenGL renderer implementation

class RRGLObjectRenderer : public RRObjectRenderer
{
public:
	RRGLObjectRenderer(rrVision::RRObjectImporter* objectImporter, rrVision::RRScene* radiositySolver);
	virtual ~RRGLObjectRenderer() {};

	virtual void render(ColorChannel cc);
	//GLfloat* getChannel(ColorChannel cc);
private:
	rrVision::RRObjectImporter* object;
	rrVision::RRScene* scene;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRCachingRenderer - filter that adds caching into underlying renderer

class RRCachingRenderer : public RRObjectRenderer
{
public:
	RRCachingRenderer(RRObjectRenderer* renderer);
	virtual ~RRCachingRenderer();

	virtual void render(ColorChannel cc);

	enum ChannelStatus
	{
		CS_READY_TO_COMPILE,
		CS_COMPILED,
		CS_NEVER_COMPILE,
	};
	void setStatus(ColorChannel cc, ChannelStatus cs);
private:
	RRObjectRenderer* renderer;
	ChannelStatus status[CC_LAST];
	GLuint displayLists[CC_LAST];
};

#endif
