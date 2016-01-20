// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - lightmap viewer with unwrap, zoom, panning.
// --------------------------------------------------------------------------

#include "SVLightmapViewer.h"

namespace rr_ed
{


SVLightmapViewer::SVLightmapViewer()
{
	nearest = false;
	alpha = false;
	zoom = 1;
	center = rr::RRVec2(0);
	buffer = nullptr;
	object = nullptr;
}

void SVLightmapViewer::setObject(rr::RRBuffer* _pixelBuffer, const rr::RRObject* _object, bool _bilinear)
{
	if (_pixelBuffer && _pixelBuffer->getType()==rr::BT_2D_TEXTURE && _object && (_pixelBuffer!=buffer || _object!=object) && _object->faceGroups.size())
	{
		// report median texel size
		unsigned unwrapChannel = _object->faceGroups[0].material->lightmap.texcoord;
		for (unsigned g=0;g<_object->faceGroups.size();g++)
			if (_object->faceGroups[g].material->lightmap.texcoord!=unwrapChannel)
			{
				RR_LIMITED_TIMES(10,rr::RRReporter::report(rr::INF2,"Median texel size: not calculated, unwrap scattered across different uv channels\n"));
				return;
			}
		rr::RRMesh* worldSpaceMesh = _object->createWorldSpaceMesh();
		float density = worldSpaceMesh->getMappingDensity(unwrapChannel);
		delete worldSpaceMesh;
		rr::RRReporter::report(rr::INF2,"Median texel size: %f * %f m\n",density/_pixelBuffer->getWidth(),density/_pixelBuffer->getHeight());
	}

	buffer = (_pixelBuffer && (_pixelBuffer->getType()==rr::BT_2D_TEXTURE || _pixelBuffer->getType()==rr::BT_CUBE_TEXTURE)) ? _pixelBuffer : nullptr;
	object = _object;
	if (buffer)
	{
		rr_gl::getTexture(buffer,true,_pixelBuffer->getType()==rr::BT_2D_TEXTURE); // no compression for cubemaps, it would prevent rendering into them later
		rr_gl::glActiveTexture(GL_TEXTURE0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _bilinear?GL_LINEAR:GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _bilinear?GL_LINEAR:GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

void SVLightmapViewer::updateTransformation(wxSize windowSize)
{
	unsigned bw = buffer ? buffer->getWidth() : 1;
	unsigned bh = buffer ? buffer->getHeight() : 1;
	float mult = RR_MIN(windowSize.x/(float)bw,windowSize.y/float(bh))*0.9f;
	bw = (unsigned)(mult*bw);
	bh = (unsigned)(mult*bh);
	t_x = 0.5f + ( center[0] - bw*0.5f )*zoom/windowSize.x;
	t_y = 0.5f + ( center[1] - bh*0.5f )*zoom/windowSize.y;
	t_w = bw*zoom/windowSize.x;
	t_h = bh*zoom/windowSize.y;
}

rr::RRVec2 SVLightmapViewer::transformUvToScreen(rr::RRVec2 uv)
{
	return rr::RRVec2((t_x+t_w*uv[0])*2-1,(t_y+t_h*uv[1])*2-1);
}

rr::RRVec2 SVLightmapViewer::getCenterUv(wxSize windowSize)
{
	updateTransformation(windowSize);
	return rr::RRVec2(-(t_x-0.5f)/t_w,-(t_y-0.5f)/t_h);
}

void SVLightmapViewer::OnMouseEvent(wxMouseEvent& event, wxSize windowSize)
{
	if (event.RightDown())
	{
		// right now there's no use for alpha mode, it is only confusing users, so it's disabled
		//alpha = !alpha;
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

void SVLightmapViewer::OnPaint(rr_gl::TextureRenderer* textureRenderer, wxSize windowSize)
{
	if (!textureRenderer)
	{
		RR_ASSERT(0);
		return;
	}

	// clear
	glClear(GL_COLOR_BUFFER_BIT);

	// setup states
	rr_gl::glDisable(GL_DEPTH_TEST);

	updateTransformation(windowSize);
	rr::RRVec2 quad[4];
	quad[0] = transformUvToScreen(rr::RRVec2(0,0)); // 2*x-1,2*y-1
	quad[1] = transformUvToScreen(rr::RRVec2(1,0)); // 2*(x+w)-1,2*y-1
	quad[2] = transformUvToScreen(rr::RRVec2(1,1)); // 2*(x+w)-1,2*(y+h)-1
	quad[3] = transformUvToScreen(rr::RRVec2(0,1)); // 2*x-1,2*(y+h)-1

	// render lightmap
	if (buffer)
		textureRenderer->render2D(rr_gl::getTexture(buffer),nullptr,t_x,t_y,t_w,t_h,-1,alpha?"#define SHOW_ALPHA0\n":nullptr);

	// render mapping edges
	const rr::RRMesh* mesh = object ? object->getCollider()->getMesh() : nullptr;
	unsigned numTriangles = mesh ? mesh->getNumTriangles() : 0;
	if (numTriangles)
	{
		rr_gl::Program* lineProgram = textureRenderer->twodProgram->getProgram(nullptr);
		if (lineProgram)
		{
			lineProgram->useIt();
			
			// 0,0..1,1 frame
			lineProgram->sendUniform("color",rr::RRVec4(0.0f,1.0f,0.0f,1.0f));
			glBegin(GL_LINE_LOOP);
			glVertex2f(quad[0][0],quad[0][1]);
			glVertex2f(quad[1][0],quad[1][1]);
			glVertex2f(quad[2][0],quad[2][1]);
			glVertex2f(quad[3][0],quad[3][1]);
			glEnd();
			
			// mapping
			if ((!buffer) || buffer->getType()==rr::BT_2D_TEXTURE)
			{
				lineProgram->sendUniform("color",rr::RRVec4(1));
				glBegin(GL_LINES);
				for (unsigned i=0;i<numTriangles;i++)
				{
					const rr::RRMaterial* material = object->getTriangleMaterial(i,nullptr,nullptr);
					if (material)
					{
						rr::RRMesh::TriangleMapping mapping;
						mesh->getTriangleMapping(i,mapping,material->lightmap.texcoord);
						for (unsigned j=0;j<3;j++)
						{
							mapping.uv[j] = transformUvToScreen(mapping.uv[j]);
						}
						for (unsigned j=0;j<3;j++)
						{
							glVertex2fv(&mapping.uv[j].x);
							glVertex2fv(&mapping.uv[(j+1)%3].x);
						}
					}
				}
				glEnd(); // here Radeon X300/Catalyst2007.09 does random fullscreen effects for 5-10sec, X1650 is ok
			}
		}
	}

	// restore states
	rr_gl::glEnable(GL_DEPTH_TEST);
	glUseProgram(0); // prevents crashes in Radeon driver in BuildLightmaps sample
}

}; // namespace
