//----------------------------------------------------------------------------
//! \file PluginCube.h
//! \brief LightsprintGL | renders to given cubemap
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2014
//! All rights reserved
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

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsCube(const PluginParams* _next, Texture* _cubeTexture, Texture* _depthTexture) : cubeTexture(_cubeTexture), depthTexture(_depthTexture) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
