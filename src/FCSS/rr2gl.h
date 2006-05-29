//
// OpenGL renderer of RRObject by Stepan Hrbek, dee@mail.cz
//

#ifndef _RR2GL_H
#define _RR2GL_H

#include <map>
#include <GL/glew.h>
#include <GL/glut.h>
#include "RRVision.h"


//////////////////////////////////////////////////////////////////////////////
//
//! RRRenderer - interface

class RRRenderer
{
public:
	//! Renders.
	virtual void render() = 0;
	//! When renderer instance has parameters that modify output, this returns them.
	//! By default, renderer is expected to have no parameters and render always the same.
	virtual const void* getParams(unsigned& length) const;
	virtual ~RRRenderer() {};
};


//////////////////////////////////////////////////////////////////////////////
//
// vertex data generator

class VertexDataGenerator
{
public:
	virtual ~VertexDataGenerator() {};
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) = 0; // vertexIndex=0..2
};

extern VertexDataGenerator* generateForcedUv;


//////////////////////////////////////////////////////////////////////////////
//
// RRGLObjectRenderer - basic OpenGL renderer implementation

class RRGLObjectRenderer : public RRRenderer
{
public:
	RRGLObjectRenderer(rr::RRObject* objectImporter, rr::RRScene* radiositySolver);
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
	virtual void setChannel(ColorChannel cc);
	virtual const void* getParams(unsigned& length) const;
	virtual void render();
	virtual ~RRGLObjectRenderer() {};
private:
	struct Params
	{
		rr::RRObject* object;
		rr::RRScene* scene;
		ColorChannel cc;
	};
	Params params;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRGLCachingRenderer - filter that adds caching into underlying renderer

class RRGLCachingRenderer : public RRRenderer
{
public:
	RRGLCachingRenderer(RRRenderer* renderer);
	enum ChannelStatus
	{
		CS_READY_TO_COMPILE,
		CS_COMPILED,
		CS_NEVER_COMPILE,
	};
	void setStatus(ChannelStatus cs);
	virtual void render();
	virtual ~RRGLCachingRenderer();
private:
	RRRenderer* renderer;
	struct Key
	{
		unsigned char params[16];
		bool operator <(const Key& key) const
		{
			return memcmp(params,key.params,sizeof(Key))<0;
		};
	};
	struct Info
	{
		Info()
		{
			status = CS_READY_TO_COMPILE;
			displayList = UINT_MAX;
		};
		ChannelStatus status;
		GLuint displayList;
	};
	typedef std::map<Key,Info> Map;
	Map mapa;
	Info& findInfo();
	void setStatus(ChannelStatus cs,Info& info);
};

#endif
