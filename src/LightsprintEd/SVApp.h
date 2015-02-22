// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - entry point of wxWidgets application.
// --------------------------------------------------------------------------

#ifndef SVAPP_H
#define SVAPP_H

#include "Lightsprint/Ed/Ed.h"
#include "Lightsprint/RRScene.h"
#include <string>
#include <wx/wx.h>
	#define SV_SUBWINDOW_BORDER wxBORDER_SUNKEN // wxBORDER_NONE // wxBORDER_SIMPLE

namespace rr_ed
{

void tm_setNow(tm& a);
void tm_addSeconds(tm& a, double _seconds);

//! Extended SceneViewerState.
struct SceneViewerStateEx : public SceneViewerState
{
	//! Initial scene to be displayed, never deleted, never NULLed even when no longer displayed (because solver->aborting is still used).
	rr::RRSolver* initialInputSolver;
	//! Path to data necessary for scene viewer.
	wxString pathToData;
	//! pathToData+"shaders/"
	wxString pathToShaders;
	//! pathToData+"maps/"
	wxString pathToMaps;
	//! Current scene filename, e.g. path/scene.dae.
	wxString sceneFilename;
	bool releaseResources;

	SceneViewerStateEx()
	{
		initialInputSolver = nullptr;
		releaseResources = true;
	}
	bool operator ==(const SceneViewerStateEx& a) const
	{
		return this->SceneViewerState::operator==(a)
			&& a.sceneFilename==sceneFilename
			;
	}
	bool operator !=(const SceneViewerStateEx& a) const
	{
		return !(a==*this);
	}
};

}; // namespace

#endif
