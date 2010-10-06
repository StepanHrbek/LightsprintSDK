// --------------------------------------------------------------------------
// Scene viewer - save & load functions.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSAVELOAD_H
#define SVSAVELOAD_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
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
			WindowLayout()
			{
				fullscreen = false;
				maximized = false;
				perspective = "";
			}
		};
		WindowLayout windowLayout[3];

		std::string sshotFilename;
		bool        sshotEnhanced;
		unsigned    sshotEnhancedWidth;
		unsigned    sshotEnhancedHeight;
		unsigned    sshotEnhancedFSAA;
		float       sshotEnhancedShadowResolutionFactor;
		unsigned    sshotEnhancedShadowSamples;

		UserPreferences()
		{
			currentWindowLayout = 0;
			sshotFilename = "";
			sshotEnhanced = true;
			sshotEnhancedWidth = 1920;
			sshotEnhancedHeight = 1080;
			sshotEnhancedFSAA = 4;
			sshotEnhancedShadowResolutionFactor = 2;
			sshotEnhancedShadowSamples = 8;
		}
		//! Saves preferences, filename is automatic.
		bool save() const;
		//! Loads preferences, filename is automatic.
		bool load();
	};


}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
