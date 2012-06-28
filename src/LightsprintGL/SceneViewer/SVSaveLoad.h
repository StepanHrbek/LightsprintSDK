// --------------------------------------------------------------------------
// Scene viewer - save & load functions.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVSAVELOAD_H
#define SVSAVELOAD_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "SVApp.h"
#include "wx/wx.h"

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

		bool knowsUnitLength(const wxString& filename) const;
		float getUnitLength(const wxString& filename) const;
		unsigned getUpAxis(const wxString& filename) const;
	};

	struct UserPreferences
	{
		bool        tooltips;
		StereoMode  stereoMode;

		unsigned    currentWindowLayout;
		struct WindowLayout
		{
			bool fullscreen;
			bool maximized;
			wxString perspective;
			WindowLayout()
			{
				fullscreen = false;
				maximized = false;
			}
		};
		WindowLayout windowLayout[3];

		ImportParameters import;

		wxString    sshotFilename;
		bool        sshotEnhanced;
		unsigned    sshotEnhancedWidth;
		unsigned    sshotEnhancedHeight;
		unsigned    sshotEnhancedFSAA;
		float       sshotEnhancedShadowResolutionFactor;
		unsigned    sshotEnhancedShadowSamples;
		bool        testingLogShaders;
		bool        testingLogMore;
		bool        testingBeta;

		UserPreferences()
		{
			tooltips = true;
			stereoMode = SM_SIDE_BY_SIDE;
			currentWindowLayout = 0;
			resetLayouts();
			sshotFilename = "";
			sshotEnhanced = false;
			sshotEnhancedWidth = 1920;
			sshotEnhancedHeight = 1080;
			sshotEnhancedFSAA = 4;
			sshotEnhancedShadowResolutionFactor = 2;
			sshotEnhancedShadowSamples = 8;
			testingLogShaders = false;
			testingLogMore = false;
			testingBeta = false;
		}
		void resetLayouts()
		{
			windowLayout[0].fullscreen = false;
			windowLayout[0].maximized = false;
			windowLayout[0].perspective = "layout2|name=glcanvas;caption=;state=256;dir=5;layer=0;row=0;pos=0;prop=100000;bestw=20;besth=20;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=scenetree;caption=Scene tree;state=2099196;dir=4;layer=2;row=0;pos=0;prop=100180;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=629;floaty=210;floatw=304;floath=320|name=userproperties;caption=User preferences;state=2099196;dir=3;layer=3;row=0;pos=4;prop=69403;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1280;floaty=1054;floatw=304;floath=320|name=sceneproperties;caption=Scene properties;state=2099196;dir=2;layer=4;row=0;pos=1;prop=154981;bestw=280;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1926;floaty=224;floatw=288;floath=320|name=giproperties;caption=Global Illumination;state=2099196;dir=2;layer=4;row=0;pos=0;prop=107208;bestw=280;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=2264;floaty=110;floatw=288;floath=320|name=lightproperties;caption=Light;state=2099196;dir=3;layer=3;row=0;pos=1;prop=82193;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=327;floaty=628;floatw=304;floath=320|name=objectproperties;caption=Object;state=2099196;dir=3;layer=3;row=0;pos=2;prop=88387;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=330;floaty=813;floatw=304;floath=320|name=materialproperties;caption=Material;state=2099196;dir=3;layer=3;row=0;pos=3;prop=76970;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1459;floaty=1076;floatw=304;floath=320|name=log;caption=Log;state=2099196;dir=3;layer=3;row=0;pos=0;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1222;floaty=1056;floatw=304;floath=320|dock_size(5,0,0)=20|dock_size(4,2,0)=296|dock_size(3,3,0)=326|dock_size(2,4,0)=300|";
			windowLayout[1].fullscreen = false;
			windowLayout[1].maximized = true;
			windowLayout[1].perspective = "layout2|name=glcanvas;caption=;state=256;dir=5;layer=0;row=0;pos=0;prop=100000;bestw=20;besth=20;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=scenetree;caption=Scene tree;state=2099196;dir=4;layer=0;row=1;pos=0;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=userproperties;caption=User preferences;state=2099196;dir=2;layer=1;row=0;pos=2;prop=48543;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1548;floaty=1008;floatw=304;floath=320|name=sceneproperties;caption=Scene properties;state=2099196;dir=2;layer=1;row=0;pos=1;prop=154628;bestw=280;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1375;floaty=378;floatw=288;floath=320|name=giproperties;caption=Global Illumination;state=2099196;dir=2;layer=1;row=0;pos=0;prop=104037;bestw=280;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=2275;floaty=59;floatw=288;floath=320|name=lightproperties;caption=Light;state=2099196;dir=4;layer=0;row=1;pos=1;prop=125064;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=334;floaty=1000;floatw=304;floath=320|name=objectproperties;caption=Object;state=2099196;dir=4;layer=0;row=1;pos=2;prop=74936;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=334;floaty=216;floatw=304;floath=320|name=materialproperties;caption=Material;state=2099196;dir=4;layer=0;row=1;pos=3;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=326;floaty=1013;floatw=304;floath=320|name=log;caption=Log;state=2099196;dir=4;layer=0;row=1;pos=4;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=541;floaty=994;floatw=856;floath=257|dock_size(5,0,0)=20|dock_size(2,1,0)=296|dock_size(4,0,1)=296|";
			windowLayout[2].fullscreen = true;
			windowLayout[2].maximized = true;
			windowLayout[2].perspective = "layout2|name=glcanvas;caption=;state=256;dir=5;layer=0;row=0;pos=0;prop=100000;bestw=20;besth=20;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=scenetree;caption=Scene tree;state=2099198;dir=4;layer=0;row=0;pos=0;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=userproperties;caption=User preferences;state=2099198;dir=2;layer=0;row=0;pos=0;prop=36442;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=341;floaty=961;floatw=304;floath=320|name=sceneproperties;caption=Scene properties;state=2099198;dir=2;layer=0;row=0;pos=0;prop=163558;bestw=280;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=341;floaty=812;floatw=288;floath=320|name=giproperties;caption=Global Illumination;state=2099198;dir=2;layer=1;row=0;pos=0;prop=104037;bestw=280;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=2275;floaty=59;floatw=288;floath=320|name=lightproperties;caption=Light;state=2099198;dir=4;layer=0;row=0;pos=0;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1918;floaty=677;floatw=304;floath=320|name=objectproperties;caption=Object;state=2099198;dir=4;layer=0;row=0;pos=0;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=334;floaty=610;floatw=304;floath=320|name=materialproperties;caption=Material;state=2099198;dir=4;layer=0;row=0;pos=0;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1921;floaty=998;floatw=304;floath=320|name=log;caption=Log;state=2099198;dir=3;layer=1;row=0;pos=0;prop=100000;bestw=296;besth=296;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=1003;floaty=777;floatw=367;floath=244|dock_size(5,0,0)=20|";
		}
		//! Saves preferences, filename is automatic.
		bool save() const;
		//! Loads preferences, ""=filename is automatic.
		bool load(const wxString& nonDefaultFilename);
	};


}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
