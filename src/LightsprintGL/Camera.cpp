// --------------------------------------------------------------------------
// OpenGL specific camera code.
// Copyright (C) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/Camera.h"
#include <GL/glew.h>

namespace rr_gl
{

void setupForRender(const rr::RRCamera& camera)
{
	// set matrices in GL pipeline
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(camera.getProjectionMatrix());
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(camera.getViewMatrix());
}

}; // namespace
