// --------------------------------------------------------------------------
// Scene viewer - save & load functions.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSAVELOAD_H
#define SVSAVELOAD_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "SVApp.h"

namespace rr_gl
{

	struct UserPreferences
	{
		unsigned currentWindowLayout;
		struct WindowLayout
		{
			bool fullscreen;
			bool maximized;
			std::string perspective;
		};
		WindowLayout windowLayout[3];

		//! Saves preferences, filename is automatic.
		bool save() const;
		//! Loads preferences, filename is automatic.
		bool load();
	};


}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
