// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVAPP_H
#define SVAPP_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/RRScene.h"
#include <ctime> // struct tm
#include <string>
#include <wx/wx.h>
	#define SV_SUBWINDOW_BORDER wxBORDER_SUNKEN // wxBORDER_NONE // wxBORDER_SIMPLE

namespace rr_gl
{


struct DateTime : public tm
{
	int tm_nsec; // nanoseconds in second, 0..999999999

	DateTime();
	void setNow();
	void addSeconds(double seconds); // tested for small positive values of seconds
};

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
	DateTime         envDateTime;               //! Date and time for sky/sun simulation.

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
			&& a.envDateTime.tm_year==envDateTime.tm_year
			&& a.envDateTime.tm_mon==envDateTime.tm_mon
			&& a.envDateTime.tm_mday==envDateTime.tm_mday
			&& a.envDateTime.tm_hour==envDateTime.tm_hour
			&& a.envDateTime.tm_min==envDateTime.tm_min
			&& a.envDateTime.tm_sec==envDateTime.tm_sec
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
