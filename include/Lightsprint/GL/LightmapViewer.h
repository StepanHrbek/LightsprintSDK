//----------------------------------------------------------------------------
//! \file LightmapViewer.h
//! \brief LightsprintGL | viewer of lightmap
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef LIGHTMAPVIEWER_H
#define LIGHTMAPVIEWER_H

#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
//! Viewer of lightmap with input/output callbacks for glut.
//
//! Displays given lightmap in 2D with mapping as a vector overlay.
//! \n User controls:
//! - position = passive motion
//! - wheel = zoom
//! - left mouse button = toggle special alpha display
class RR_GL_API LightmapViewer : public rr::RRUniformlyAllocated
{
public:
	//! Creates lightmap viewer instance. Only one instance at a time is allowed.
	static LightmapViewer* create(const char* pathToShaders);
	static void setObject(rr::RRBuffer* _pixelBuffer, rr::RRMesh* _mesh);

	//! Destructs instance.
	~LightmapViewer();

	//! Callback for glut.
	static void mouse(int button, int state, int x, int y);
	//! Callback for glut.
	static void passive(int x, int y);
	//! Callback for glut.
	static void display();

private:
	LightmapViewer(const char* pathToShaders);
};

}; // namespace

#endif
