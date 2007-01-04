// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef RENDEREROFRROBJECT_H
#define RENDEREROFRROBJECT_H

#include <cstring> // memset
#include "RRVision.h"
#include "DemoEngine/Renderer.h"


//////////////////////////////////////////////////////////////////////////////
//
// per vertex data generator

class VertexDataGenerator
{
public:
	virtual ~VertexDataGenerator() {};
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) = 0; // vertexIndex=0..2
	virtual unsigned getHash() = 0;
};


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject - basic OpenGL renderer implementation

class RendererOfRRObject : public Renderer
{
public:
	RendererOfRRObject(const rr::RRObject* objectImporter, const rr::RRScene* radiositySolver, const rr::RRScaler* scaler);
	struct RenderedChannels
	{
		bool     LIGHT_DIRECT           :1; // gl_Normal
		bool     LIGHT_INDIRECT_COLOR   :1; // gl_Color
		bool     LIGHT_INDIRECT_MAP     :1; //
		bool     LIGHT_INDIRECT_ENV     :1; // gl_Normal
		bool     MATERIAL_DIFFUSE_COLOR :1; // gl_SecondaryColor
		bool     MATERIAL_DIFFUSE_MAP   :1; // gl_MultiTexCoord0, current sampler
		bool     OBJECT_SPACE           :1; // worldMatrix
		bool     FORCE_2D_POSITION      :1; // gl_MultiTexCoord7
		RenderedChannels()
		{
			// turn everything off by default
			memset(this,0,sizeof(*this));
		}
	};
	void setRenderedChannels(RenderedChannels renderedChannels);
	void setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle, unsigned alastCapturedTriangle);
	virtual const void* getParams(unsigned& length) const;
	virtual void render();
	virtual ~RendererOfRRObject() {};
private:
	struct Params
	{
		const rr::RRObject* object;
		const rr::RRScene* scene;
		const rr::RRScaler* scaler;
		RenderedChannels renderedChannels;
		VertexDataGenerator* generateForcedUv;
		unsigned otherCaptureParamsHash;
		unsigned firstCapturedTriangle;
		unsigned lastCapturedTriangle;
	};
	Params params;
};

#endif
