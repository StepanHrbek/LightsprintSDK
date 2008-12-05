// --------------------------------------------------------------------------
// Scene viewer - lightmap viewer with unwrap, zoom, panning.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include "SVLightmapViewer.h"
#include "../tmpstr.h"

namespace rr_gl
{


SVLightmapViewer::SVLightmapViewer(const char* _pathToShaders)
{
	nearest = false;
	alpha = false;
	zoom = 1;
	center = rr::RRVec2(0);
	uberProgram = UberProgram::create(tmpstr("%stexture.vs",_pathToShaders),tmpstr("%stexture.fs",_pathToShaders));
	lmapProgram = uberProgram->getProgram("#define TEXTURE\n");
	lmapAlphaProgram = uberProgram->getProgram("#define TEXTURE\n#define SHOW_ALPHA0\n");
	lineProgram = uberProgram->getProgram(NULL);
	buffer = NULL;
	mesh = NULL;
}

SVLightmapViewer::~SVLightmapViewer()
{
	delete uberProgram;
}

void SVLightmapViewer::setObject(rr::RRBuffer* _pixelBuffer, const rr::RRObject* _object, bool _bilinear)
{
	buffer = (_pixelBuffer && _pixelBuffer->getType()==rr::BT_2D_TEXTURE) ? _pixelBuffer : NULL;
	mesh = _object ? _object->getCollider()->getMesh() : NULL;
	lightmapTexcoord = 0;
	if (_object)
	{
		const rr::RRMaterial* material = _object->getTriangleMaterial(0,NULL,NULL);
		if (material)
		{
			lightmapTexcoord = material->lightmapTexcoord;
		}
	}
	if (buffer)
	{
		getTexture(buffer);
		glActiveTexture(GL_TEXTURE0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _bilinear?GL_LINEAR:GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _bilinear?GL_LINEAR:GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

void SVLightmapViewer::OnMouseEvent(wxMouseEvent& event, wxSize windowSize)
{
	if (event.RightDown())
	{
		alpha = !alpha;
	}
	if (event.GetWheelRotation()>0)
	{
		zoom *= 0.625f;
	}
	if (event.GetWheelRotation()<0)
	{
		zoom *= 1.6f;
	}
	if (event.Dragging())
	{
		center[0] += 2*(event.GetX()-previousPosition.x)/zoom;
		center[1] -= 2*(event.GetY()-previousPosition.y)/zoom;
	}
	previousPosition = event.GetPosition();
}

rr::RRVec2 SVLightmapViewer::getCenterUv(wxSize windowSize)
{
	// copy of code from display(), could be simplified
	unsigned bw = buffer ? buffer->getWidth() : 1;
	unsigned bh = buffer ? buffer->getHeight() : 1;
	float mult = MIN(windowSize.x/(float)bw,windowSize.y/float(bh))*0.9f;
	bw = (unsigned)(mult*bw);
	bh = (unsigned)(mult*bh);
	float x = 0.5f + ( center[0] - bw*0.5f )*zoom/windowSize.x;
	float y = 0.5f + ( center[1] - bh*0.5f )*zoom/windowSize.y;
	float w = bw*zoom/windowSize.x;
	float h = bh*zoom/windowSize.y;
	// new code
	return rr::RRVec2(-(x-0.5f)/w,-(y-0.5f)/h);
}

void SVLightmapViewer::OnPaint(wxPaintEvent& event, wxSize windowSize)
{
	if (!lmapProgram || !lmapAlphaProgram || !lineProgram)
	{
		RR_ASSERT(0);
		return;
	}

	// clear
	glClear(GL_COLOR_BUFFER_BIT);

	// setup states
	glDisable(GL_DEPTH_TEST);

	unsigned bw = buffer ? buffer->getWidth() : 1;
	unsigned bh = buffer ? buffer->getHeight() : 1;
	float mult = MIN(windowSize.x/(float)bw,windowSize.y/float(bh))*0.9f;
	bw = (unsigned)(mult*bw);
	bh = (unsigned)(mult*bh);

	// render lightmap
	if (buffer)
	{
		float x = 0.5f + ( center[0] - bw*0.5f )*zoom/windowSize.x;
		float y = 0.5f + ( center[1] - bh*0.5f )*zoom/windowSize.y;
		float w = bw*zoom/windowSize.x;
		float h = bh*zoom/windowSize.y;
		Program* prg = alpha?lmapAlphaProgram:lmapProgram;
		prg->useIt();
		glActiveTexture(GL_TEXTURE0);
		getTexture(buffer)->bindTexture();
		prg->sendUniform("map",0);
		prg->sendUniform("color",1.0f,1.0f,1.0f,1.0f);
		glBegin(GL_POLYGON);
		glTexCoord2f(0,0);
		glVertex2f(2*x-1,2*y-1);
		glTexCoord2f(1,0);
		glVertex2f(2*(x+w)-1,2*y-1);
		glTexCoord2f(1,1);
		glVertex2f(2*(x+w)-1,2*(y+h)-1);
		glTexCoord2f(0,1);
		glVertex2f(2*x-1,2*(y+h)-1);
		glEnd();
	}

	// render mapping edges
	if (mesh)
	{
		lineProgram->useIt();
		lineProgram->sendUniform("color",1.0f,1.0f,1.0f,1.0f);
		glBegin(GL_LINES);
		for (unsigned i=0;i<mesh->getNumTriangles();i++)
		{
			rr::RRMesh::TriangleMapping mapping;
			mesh->getTriangleMapping(i,mapping,lightmapTexcoord);
			for (unsigned j=0;j<3;j++)
			{
				mapping.uv[j][0] = ( center[0]*2 + (mapping.uv[j][0]-0.5f)*2*bw )*zoom/windowSize.x;
				mapping.uv[j][1] = ( center[1]*2 + (mapping.uv[j][1]-0.5f)*2*bh )*zoom/windowSize.y;
			}
			for (unsigned j=0;j<3;j++)
			{
				glVertex2fv(&mapping.uv[j].x);
				glVertex2fv(&mapping.uv[(j+1)%3].x);
			}
		}
		glEnd(); // here Radeon X300/Catalyst2007.09 does random fullscreen effects for 5-10sec, X1650 is ok
	}

	// restore states
	glEnable(GL_DEPTH_TEST);
	glUseProgram(0); // prevents crashes in Radeon driver in BuildLightmaps sample
}

}; // namespace
