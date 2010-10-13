// --------------------------------------------------------------------------
// Scene viewer - object properties window.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVOBJECTPROPERTIES_H
#define SVOBJECTPROPERTIES_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVProperties.h"

namespace rr_gl
{

	class SVObjectProperties : public SVProperties
	{
	public:
		SVObjectProperties(SVFrame* svframe);

		//! Copy object -> property (all values, show/hide).
		void setObject(rr::RRObject* object, int precision);

		//! Copy object -> property (cube sizes).
		void updateProperties();

		//! Copy property -> object (all values).
		void OnPropertyChange(wxPropertyGridEvent& event);

	private:
		rr::RRObject*     object;
		rr::RRVec3        localCenter;

		wxPGProperty*     propName;
		wxPGProperty*     propWTranslation;

		wxPGProperty*     propCubeDiffuse;
		wxPGProperty*     propCubeSpecular;

		DECLARE_EVENT_TABLE()
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
