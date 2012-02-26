// --------------------------------------------------------------------------
// OpenGL specific camera code.
// Copyright (C) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/Camera.h"
#include <GL/glew.h>

namespace rr_gl
{

static const rr::RRCamera* s_renderCamera = NULL;

const rr::RRCamera* getRenderCamera()
{
	return s_renderCamera;
}

void setupForRender(const rr::RRCamera& camera)
{
	s_renderCamera = &camera;
	// set matrices in GL pipeline
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(camera.getProjectionMatrix());
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(camera.getViewMatrix());
}

}; // namespace
