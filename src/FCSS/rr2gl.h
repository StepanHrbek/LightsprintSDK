//
// OpenGL renderer of RRObject by Stepan Hrbek, dee@mail.cz
//

#ifndef _RR2GL_H
#define _RR2GL_H

#include <map>
#include <GL/glew.h>
#include <GL/glut.h>
#include "GL/Texture.h"
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
	struct RenderedChannels
	{
		bool     LIGHT_DIRECT           :1; // gl_Normal (normals are necessary only for direct lighting)
		bool     LIGHT_INDIRECT_COLOR   :1; // gl_Color
		bool     LIGHT_INDIRECT_MAP     :1; //
		bool     MATERIAL_DIFFUSE_COLOR :1; // gl_SecondaryColor
		bool     MATERIAL_DIFFUSE_MAP   :1; // gl_MultiTexCoord0, current sampler
		bool     FORCE_2D_POSITION      :1; // gl_MultiTexCoord7
		RenderedChannels()
		{
			memset(this,0,sizeof(*this));
		}
	};
	void setRenderedChannels(RenderedChannels renderedChannels);
	void setFirstCapturedTriangle(unsigned afirstCapturedTriangle);
	virtual const void* getParams(unsigned& length) const;
	virtual void render();
	virtual ~RRGLObjectRenderer() {};
private:
	struct Params
	{
		rr::RRObject* object;
		rr::RRScene* scene;
		RenderedChannels renderedChannels;
		unsigned firstCapturedTriangle;
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
