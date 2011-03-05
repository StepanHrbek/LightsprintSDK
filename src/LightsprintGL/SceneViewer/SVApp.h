// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVAPP_H
#define SVAPP_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/RRScene.h"
#include <string>
	#define SV_SUBWINDOW_BORDER wxBORDER_SUNKEN // wxBORDER_NONE // wxBORDER_SIMPLE

// string conversions, RR=RRString, WX=wxString, PATH=bf::path, CHAR=const char*, STREAM=fstream constructor
#define WX2CHAR(w)   ((const char*)(w))
#define WX2RR(w)     ((const char*)(w))
#define WX2PATH(w)   ((const wchar_t*)(w))
#define WX2STREAM(w) ((const wchar_t*)(w))
#define RR2WX(r)     ((r).c_str())
#define PATH2WX(p)   ((p).wstring())

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
	wxString sceneFilename;
	//! Current skybox filename, e.g. skybox.hdr or skybox_ft.tga.
	//! To specify Quake-style cube map, use name of any one of 6 images (Quake uses suffixes ft,bk,up,dn,rt,lf).
	wxString skyboxFilename;
	bool releaseResources;

	SceneViewerStateEx()
	{
		initialInputSolver = NULL;
		pathToShaders = NULL;
		releaseResources = true;
	}
	bool operator ==(const SceneViewerStateEx& a) const
	{
		return this->SceneViewerState::operator==(a)
			&& a.sceneFilename==sceneFilename
			&& a.skyboxFilename==skyboxFilename
			;
	}
	bool operator !=(const SceneViewerStateEx& a) const
	{
		return !(a==*this);
	}
};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
