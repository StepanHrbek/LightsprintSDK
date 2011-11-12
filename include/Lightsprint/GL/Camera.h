//----------------------------------------------------------------------------
//! \file Camera.h
//! \brief LightsprintGL | OpenGL specific camera code.
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2011
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef CAMERA_H
#define CAMERA_H

#include "DemoEngine.h"
#include "Lightsprint/RRCamera.h"

namespace rr_gl
{

//! Copies matrices from camera to OpenGL pipeline, so that following primitives are transformed as if viewed by this camera.
//
//! Changes glMatrixMode to GL_MODELVIEW.
//! Note that if you modify camera inputs, changes are propagated to OpenGL only after update() and setupForRender().
RR_GL_API void setupForRender(const rr::RRCamera& camera);

//! Returns last camera that executed setupForRender().
RR_GL_API const rr::RRCamera* getRenderCamera();

}; // namespace

#endif //CAMERA_H
