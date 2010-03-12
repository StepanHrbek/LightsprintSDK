// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVAPP_H
#define SVAPP_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/RRScene.h"
#include <string>
#include <vector>

	#define SV_SUBWINDOW_BORDER wxBORDER_SUNKEN // wxBORDER_NONE // wxBORDER_SIMPLE
	#define SV_SET_PG_COLORS SetMarginColour(wxColour(220,220,220))

namespace rr_gl
{

//! Extended SceneViewerState.
struct SceneViewerStateEx : public SceneViewerState
{
	//! Initial scene to be displayed, never deleted, never NULLed even when no longer displayed (because solver->aborting is still used).
	rr::RRDynamicSolver* initialInputSolver;
	//! Initial and never changing path to shaders, never freed.
	const char* pathToShaders;
	//! Current scene filename, e.g. path/scene.dae.
	std::string sceneFilename;
	//! Current skybox filename, e.g. skybox.hdr or skybox_ft.tga.
	//! To specify Quake-style cube map, use name of any one of 6 images (Quake uses suffixes ft,bk,up,dn,rt,lf).
	std::string skyboxFilename;
	bool releaseResources;

	SceneViewerStateEx()
	{
		initialInputSolver = NULL;
		pathToShaders = NULL;
		releaseResources = true;
	}
	bool operator ==(const SceneViewerStateEx& a) const
	{
		return this->SceneViewerState::operator==(a) && skyboxFilename==a.skyboxFilename;
	}
	bool operator !=(const SceneViewerStateEx& a) const
	{
		return !(a==*this);
	}
};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
