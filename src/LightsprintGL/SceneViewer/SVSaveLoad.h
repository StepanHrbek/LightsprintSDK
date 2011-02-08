// --------------------------------------------------------------------------
// Scene viewer - save & load functions.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSAVELOAD_H
#define SVSAVELOAD_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVApp.h"

namespace rr_gl
{

	struct ImportParameters
	{
		enum Unit
		{
			U_CUSTOM,
			U_M,
			U_INCH,
			U_CM,
			U_MM
		};
		Unit        unitEnum;
		float       unitFloat;
		bool        unitForce; // false=use selected units only if file does not know, true=use selected units even if file knows its units

		unsigned    up; // 0=x,1=y,2=z
		bool        upForce; // false=use selected up only if file does not know, true=use selected up even if file knows its up

		ImportParameters();

		float getUnitLength(const char* filename) const;
		unsigned getUpAxis(const char* filename) const;
	};

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
			}
		};
		WindowLayout windowLayout[3];

		ImportParameters import;

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
			sshotEnhanced = false;
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
