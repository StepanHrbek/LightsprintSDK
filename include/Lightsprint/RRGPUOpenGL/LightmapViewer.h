// --------------------------------------------------------------------------
// Viewer of lightmap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef VIEWEROFLIGHTMAP_H
#define VIEWEROFLIGHTMAP_H

#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/DemoEngine/Texture.h"

namespace rr_gl
{

class LightmapViewer
{
public:
	LightmapViewer(de::Texture* _lightmap, rr::RRMesh* _mesh);
	LightmapViewer(rr::RRIlluminationPixelBuffer* _pixelBuffer, rr::RRMesh* _mesh);
	~LightmapViewer();
	static void mouse(int button, int state, int x, int y);
	static void passive(int x, int y);
	static void display();

private:
	static void init(de::Texture* _lightmap, rr::RRMesh* _mesh);
};

}; // namespace

#endif
