// --------------------------------------------------------------------------
// Viewer of lightmap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef LIGHTMAPVIEWER_H
#define LIGHTMAPVIEWER_H

#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/DemoEngine/Texture.h"
#include "Lightsprint/RRGPUOpenGL.h"

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
//! - left mouse button = toggle interpolation
//! - right mouse button = toggle special alpha display
class RR_API LightmapViewer
{
public:
	//! Creates lightmap viewer instance. Only one instance at a time is allowed.
	static LightmapViewer* create(de::Texture* _lightmap, rr::RRMesh* _mesh);

	//! Creates lightmap viewer instance. Only one instance at a time is allowed.
	static LightmapViewer* create(rr::RRIlluminationPixelBuffer* _pixelBuffer, rr::RRMesh* _mesh);

	//! Destructs instance.
	~LightmapViewer();

	//! Callback for glut.
	static void mouse(int button, int state, int x, int y);
	//! Callback for glut.
	static void passive(int x, int y);
	//! Callback for glut.
	static void display();

private:
	LightmapViewer(de::Texture* _lightmap, rr::RRMesh* _mesh);
};

}; // namespace

#endif
