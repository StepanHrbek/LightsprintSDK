// --------------------------------------------------------------------------
// Scene viewer - panel with properties.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVPROPERTIES_H
#define SVPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

//#define PROPERTYGRID_TABS

#include "Lightsprint/GL/SceneViewer.h"
#include "SVFrame.h"
#include "wx/propgrid/propgrid.h"

#ifdef PROPERTYGRID_TABS
	#include "wx/propgrid/manager.h"
	#define PROPERTYGRID_BASE wxPropertyGridPage
#else
	#define PROPERTYGRID_BASE wxPropertyGrid
#endif

namespace rr_gl
{

	// Code shared by all property panels (SVXxxProperties).
	class SVProperties : public wxPropertyGrid
	{
	public:
		SVProperties(SVFrame* _svframe)
		: wxPropertyGrid(_svframe, wxID_ANY, wxDefaultPosition, wxSize(300,300), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER|SV_SUBWINDOW_BORDER/*|wxPG_TOOLTIPS*/), svs(_svframe->svs)
		{
			svframe = _svframe;
			SetMarginColour(wxColour(220,220,220));
			//SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		}

	protected:
		SVFrame* svframe; // we must remember our main frame, reading it from parent is not reliable, wxAUI may reparent us
		SceneViewerStateEx& svs; // shortcut for svframe->svs
	};

#ifdef PROPERTYGRID_TABS
	// Code shared by all property panels (SVXxxProperties).
	class SVPropertiesPage : public wxPropertyGridPage
	{
	public:
		SVPropertiesPage(SVFrame* _svframe)
		: wxPropertyGridPage(), svs(_svframe->svs)
		{
			svframe = _svframe;
		}

	protected:
		SVFrame* svframe; // we must remember our main frame, reading it from parent is not reliable, wxAUI may reparent us
		SceneViewerStateEx& svs; // shortcut for svframe->svs
	};
#endif

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
