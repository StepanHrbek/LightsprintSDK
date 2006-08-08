#ifndef RENDEREROFRROBJECT_H
#define RENDEREROFRROBJECT_H

#include "DemoEngine.h"
#include <cstring> // memset
#include "Renderer.h"
#include "RRVision.h"
#include "VertexDataGenerator.h"


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfRRObject - basic OpenGL renderer implementation

class RR_API RendererOfRRObject : public Renderer
{
public:
	RendererOfRRObject(rr::RRObject* objectImporter, rr::RRScene* radiositySolver);
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
	void setCapture(VertexDataGenerator* capture, unsigned afirstCapturedTriangle);
	virtual const void* getParams(unsigned& length) const;
	virtual void render();
	virtual ~RendererOfRRObject() {};
private:
	struct Params
	{
		rr::RRObject* object;
		rr::RRScene* scene;
		RenderedChannels renderedChannels;
		VertexDataGenerator* generateForcedUv;
		unsigned firstCapturedTriangle;
	};
	Params params;
};

#endif
