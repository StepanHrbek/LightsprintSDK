// --------------------------------------------------------------------------
// Scene viewer - object properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVOBJECTPROPERTIES_H
#define SVOBJECTPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "wx/wx.h"
#include "wx/propgrid/propgrid.h"

namespace rr_gl
{

	class SVObjectProperties : public wxPropertyGrid
	{
	public:
		SVObjectProperties( wxWindow* parent );

		//! Copy light -> property (all values, show/hide).
		void setObject(rr::RRObject* _object, int _precision);

		//! Copy light -> property (show/hide).
		void updateHide();

		//! Copy light -> property (8 floats).
		//! Update pos+dir properties according to current light state.
		//! They may be changed outside dialog, this must be called to update values in dialog.
		void updatePosDir();

		//! Copy property -> light (all values).
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		rr::RRObject*     object;

		wxPGProperty*     propName;
		wxPGProperty*     propWTranslation;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
