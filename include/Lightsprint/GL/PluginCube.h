//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginCube.h
//! \brief LightsprintGL | renders to given cubemap
//----------------------------------------------------------------------------

#ifndef PLUGINCUBE_H
#define PLUGINCUBE_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Render to cubemap plugin

//! Renders scene (next plugin) into given cubemap. For comparison, other plugins render into current render target, which is always 2d.
class RR_GL_API PluginParamsCube : public PluginParams
{
public:
	//! Next plugins will render to this cubemap.
	Texture* cubeTexture;
	//! Next plugins will use this 2d depthmap when rendering to cubemap side.
	Texture* depthTexture;
	//! Field of view captured, 360 for full view.
	float fovDeg;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsCube(const PluginParams* _next, Texture* _cubeTexture, Texture* _depthTexture, float _fovDeg) : cubeTexture(_cubeTexture), depthTexture(_depthTexture), fovDeg(_fovDeg) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
