// --------------------------------------------------------------------------
// Scene viewer - panel with properties.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVPROPERTIES_H
#define SVPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

//#define PROPERTYGRID_TABS

#include "Lightsprint/GL/SceneViewer.h"
#include "SVFrame.h"
#include "SVCustomProperties.h"
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
		: wxPropertyGrid(_svframe, wxID_ANY, wxDefaultPosition, wxSize(300,300), wxPG_DEFAULT_STYLE|wxPG_SPLITTER_AUTO_CENTER|SV_SUBWINDOW_BORDER), svs(_svframe->svs)
		{
			svframe = _svframe;

			importantPropertyBackgroundColor = wxColour(230,230,230);
			SetMarginColour(wxColour(220,220,220));
		}

		void enableTooltips(bool enable)
		{
			SetWindowStyle((GetWindowStyle()&~wxPG_TOOLTIPS)|(enable?wxPG_TOOLTIPS:0));
			SetExtraStyle(enable?wxPG_EX_HELP_AS_TOOLTIPS:0);
		}

		void defocusButtonEditor()
		{
			// when editor is clicked, second click would not create any event, we have to deselect it first
			// we can't immediately deselect it while it is being selected, wx would ignore us, so we deselect it here, one frame later
			wxPGProperty* property = GetSelectedProperty();
			if ((dynamic_cast<HDRColorProperty*>(property) || dynamic_cast<ButtonProperty*>(property)) && IsEditorFocused())
			{
				// moves focus from editor to label
				SelectProperty(property,false);
				// user can focus editor again (and start action) by
				// a) TAB
				// b) click image <--- DOES NOT WORK, wx bug?, clicking image does not open editor/generate eny event
				// b) click to the right of image
				// c) click other property, click image
			}
		}

	protected:
		SVFrame* svframe; // we must remember our main frame, reading it from parent is not reliable, wxAUI may reparent us
		SceneViewerStateEx& svs; // shortcut for svframe->svs
		wxColour importantPropertyBackgroundColor;
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
