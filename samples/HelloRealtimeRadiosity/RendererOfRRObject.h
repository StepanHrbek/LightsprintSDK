// --------------------------------------------------------------------------
// Renderer implementation that renders RRObject instance.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

// Renders RRObject.
// Feeds OpenGL with primitives and binds textures,
// expects that shaders were already set.
//
// Why does it exist?
//   It can render each triangle with independent generated uv coords,
//   without regard to possible vertex sharing.
//   It is useful during detectDirectIllumination().
//
// Other ways how to reach the same without second renderer:
//
// 1) Stay with any other renderer you have.
//    Don't share vertices, resave all your meshes with vertices splitted.
//    This makes mesh bigger and rendering bit slower,
//    but uv coord may be set for each triangle independently.
//    You can eventually store both versions of mesh and render
//    the bigger one only during detectDirectIllumination().
// 2) Stay with any other renderer that supports geometry shaders.
//    Write simple geometry shader that generates uv coordinates
//    based on primitive id.

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
		bool     LIGHT_DIRECT           :1; // feeds gl_Normal
		bool     LIGHT_INDIRECT_COLOR   :1; // feeds gl_Color
		bool     LIGHT_INDIRECT_MAP     :1; // feeds gl_MultiTexCoord[MULTITEXCOORD_LIGHT_INDIRECT] + texture[TEXTURE_2D_LIGHT_INDIRECT]
		bool     LIGHT_INDIRECT_ENV     :1; // feeds gl_Normal + texture[TEXTURE_CUBE_LIGHT_INDIRECT]
		bool     MATERIAL_DIFFUSE_COLOR :1; // feeds gl_SecondaryColor
		bool     MATERIAL_DIFFUSE_MAP   :1; // feeds gl_MultiTexCoord[MULTITEXCOORD_MATERIAL_DIFFUSE] + texture[TEXTURE_2D_MATERIAL_DIFFUSE]
		bool     FORCE_2D_POSITION      :1; // feeds gl_MultiTexCoord7
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
