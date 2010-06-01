// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVFrame.h"
#include "Lightsprint/RRScene.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "SVRayLog.h"
#include "SVSaveLoad.h"
#include "SVSceneProperties.h"
#include "SVLightProperties.h"
#include "SVObjectProperties.h"
#include "SVMaterialProperties.h"
#include "SVSceneTree.h"
#include "../tmpstr.h"
#include "wx/aboutdlg.h"
#ifdef _WIN32
	#include <shlobj.h> // SHGetSpecialFolderPath
#endif

	#define DEFAULT_FIREBALL_QUALITY 350
// naming convention for lightmaps and ldm. final name is prefix+objectnumber+postfix
#define LMAP_PREFIX (wxString(svs.sceneFilename)+".").c_str()
#define LMAP_POSTFIX "lightmap.png"
#define LDM_PREFIX (wxString(svs.sceneFilename)+".").c_str()
#define LDM_POSTFIX "ldm.png"

namespace rr_gl
{

// "123abc" -> 123
unsigned getUnsigned(const wxString& str)
{
	unsigned result = 0;
	for (unsigned i=0;i<str.size() && str[i]>='0' && str[i]<='9';i++)
	{
		result = 10*result + (char)str[i]-'0';
	}
	return result;
}

// true = valid answer
// false = dialog was escaped
// Incoming quality is taken as default value.
static bool getQuality(wxString title, wxWindow* parent, unsigned& quality)
{
	wxArrayString choices;
	choices.Add("1");
	choices.Add("10 - low");
	choices.Add("40");
	choices.Add("100 - medium");
	choices.Add("350");
	choices.Add("1000 - high");
	choices.Add("3000");
	choices.Add("10000 - very high");
	choices.Add("30000");
	choices.Add("100000");
	wxSingleChoiceDialog dialog(parent,title,"Please select quality",choices);
	for (size_t i=0;i<choices.size();i++)
	{
		unsigned u = getUnsigned(choices[i]);
		if (u>=quality || i+1==choices.size())
		{
			dialog.SetSelection((int)i);
			break;
		}
	}
	if (dialog.ShowModal()==wxID_OK)
	{
		quality = getUnsigned(dialog.GetStringSelection());
		return true;
	}
	return false;
}

// true = valid answer
// false = dialog was escaped
// Incoming resolution is taken as default value.
static bool getResolution(wxString title, wxWindow* parent, unsigned& resolution, bool offerPerVertex)
{
	wxArrayString choices;
	if (offerPerVertex)	choices.Add("per-vertex");
	choices.Add("8x8");
	choices.Add("16x16");
	choices.Add("32x32");
	choices.Add("64x64");
	choices.Add("128x128");
	choices.Add("256x256");
	choices.Add("512x512");
	choices.Add("1024x1024");
	choices.Add("2048x2048");
	choices.Add("4096x4096");
	wxSingleChoiceDialog dialog(parent,title,"Please select texture resolution",choices);
	for (size_t i=0;i<choices.size();i++)
	{
		unsigned u = getUnsigned(choices[i]);
		if (u>=resolution || i+1==choices.size())
		{
			dialog.SetSelection((int)i);
			break;
		}
	}
	if (dialog.ShowModal()==wxID_OK)
	{
		resolution = getUnsigned(dialog.GetStringSelection());
		return true;
	}
	return false;
}

// true = valid answer
// false = dialog was escaped
static bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption)
{
	char value[30];
	sprintf(value,"%f",factor);
	wxTextEntryDialog dialog(parent,message,caption,value);
	if (dialog.ShowModal()==wxID_OK)
	{
		double d = 1;
		if (dialog.GetValue().ToDouble(&d))
		{
			factor = (float)d;
			return true;
		}
	}
	return false;
}

// true = valid answer
// false = dialog was escaped
static bool getBrightness(wxWindow* parent, rr::RRVec4& brightness)
{
	float average = brightness.avg();
	if (getFactor(parent,average,"Please adjust brightness (default is 1).","Brightness"))
	{
		brightness = rr::RRVec4(average);
		return true;
	}
	return false;
}

	#define APP_NAME wxString("SceneViewer")
	#define APP_VERSION __DATE__

void SVFrame::UpdateTitle()
{
	if (!svs.sceneFilename.empty())
		SetTitle(APP_NAME+APP_VERSION_TITLE+" - "+svs.sceneFilename);
	else
		SetTitle(APP_NAME+APP_VERSION_TITLE);
}

void SVFrame::UpdateEverything()
{
	// workaround for 2.8.9 problem fixed in 2.9.0
	// remember canvas size, only first canvas is sized automatically
	wxSize newSize = m_canvas ? m_canvas->GetSize() : wxDefaultSize;

	bool firstUpdate = !m_canvas;

	bool oldReleaseResources = svs.releaseResources;
	svs.releaseResources = true; // we are not returning yet, we should shutdown
	if (m_canvas) m_mgr.DetachPane(m_canvas);
	RR_SAFE_DELETE(m_canvas);
	svs.releaseResources = oldReleaseResources;

	// initialInputSolver may be changed only if canvas is NULL
	// we NULL it to avoid rendering solver contents again (new scene was opened)
	// it has also minor drawback: initialInputSolver->abort will be ignored
	if (!firstUpdate) svs.initialInputSolver = NULL;

	// creates canvas
	// loads scene if it is specified by filename
	m_canvas = new SVCanvas( svs, this, newSize);


	// must go after SVCanvas() otherwise canvas stays 16x16 pixels
	Show(true);

	UpdateMenuBar();

	// must go after Show() otherwise SetCurrent() in createContext() fails
	m_canvas->createContext();
	
	// without SetFocus, keyboard events may be sent to frame instead of canvas
	m_canvas->SetFocus();

	if (svs.autodetectCamera && !(svs.initialInputSolver && svs.initialInputSolver->aborting)) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_CAMERA_GENERATE_RANDOM));

	UpdateTitle();

	updateSelection();

	m_mgr.AddPane(m_canvas, wxAuiPaneInfo().Name(wxT("glcanvas")).CenterPane().PaneBorder(false));
	m_mgr.Update();

	// start playing videos
	if (svs.playVideos)
	{
		svs.playVideos = false;
		wxKeyEvent event;
		event.m_keyCode = ' ';
		m_canvas->OnKeyDown(event);
	}

}

static wxImage* loadImage(const char* filename)
{
	// Q: why do we read images via RRBuffer::load()?
	// A: wx file loaders are not compiled in, to reduce size
	//    wxIcon(fname.ico) works only in Windows, only on some icons and it reduces icon resolution
	//    wxBitmap(fname.bmp) works only in Windows and ignores alphachannel
	rr::RRBuffer* buffer = rr::RRBuffer::load(filename);
	if (!buffer)
		return NULL;
	buffer->flip(false,true,false);
	unsigned width = buffer->getWidth();
	unsigned height = buffer->getHeight();
	// filling wxImage per pixel rather than passing whole buffer to constructor is necessary with buggy wxWidgets 2.8.9
	wxImage* image = new wxImage(width,height,false);
	image->InitAlpha();
	for (unsigned j=0;j<height;j++)
		for (unsigned i=0;i<width;i++)
		{
			rr::RRVec4 element = buffer->getElement(j*width+i);
			image->SetRGB(i,j,(unsigned)(element[0]*255),(unsigned)(element[1]*255),(unsigned)(element[2]*255));
			image->SetAlpha(i,j,(unsigned)(element[3]*255));
		}
	delete buffer;
	return image;
}

static wxIcon* loadIcon(const char* filename)
{
	wxImage* image = loadImage(filename);
	if (!image)
		return NULL;
	wxBitmap bitmap(*image);
	wxIcon* icon = new wxIcon();
	icon->CopyFromBitmap(bitmap);
	delete image;
	return icon;
}

void SVFrame::userPreferencesGatherFromWx()
{
	userPreferences.windowLayout[userPreferences.currentWindowLayout].fullscreen = svs.fullscreen;
	userPreferences.windowLayout[userPreferences.currentWindowLayout].maximized = IsMaximized();
	userPreferences.windowLayout[userPreferences.currentWindowLayout].perspective = m_mgr.SavePerspective();
}

void SVFrame::userPreferencesApplyToWx()
{
	if (userPreferences.windowLayout[userPreferences.currentWindowLayout].fullscreen != svs.fullscreen)
		OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_FULLSCREEN));
	if (userPreferences.windowLayout[userPreferences.currentWindowLayout].maximized != IsMaximized())
		Maximize(!IsMaximized());
	m_mgr.LoadPerspective(userPreferences.windowLayout[userPreferences.currentWindowLayout].perspective,true);
	UpdateMenuBar();
}

SVFrame* SVFrame::Create(SceneViewerStateEx& svs)
{
	// workaround for http://trac.wxwidgets.org/ticket/11787
	// solves crash in second Toolbench Relighting 
	wxPGEditor_TextCtrl = NULL;
	wxPGEditor_Choice = NULL;
	wxPGEditor_ComboBox = NULL;
	wxPGEditor_TextCtrlAndButton = NULL;
	wxPGEditor_CheckBox = NULL;
	wxPGEditor_ChoiceAndButton = NULL;
//	wxPGEditor_SpinCtrl = NULL;
//	wxPGEditor_DatePickerCtrl = NULL;

	// open at ~50% of screen size
	int x,y,width,height;
	::wxClientDisplayRect(&x,&y,&width,&height);
	const int border = (width+height)/25;
	return new SVFrame(NULL, APP_NAME+" - loading", wxPoint(x+2*border,y+border), wxSize(width-4*border,height-2*border), svs);
}

SVFrame::SVFrame(wxWindow* _parent, const wxString& _title, const wxPoint& _pos, const wxSize& _size, SceneViewerStateEx& _svs)
	: wxFrame(_parent, wxID_ANY, _title, _pos, _size, wxDEFAULT_FRAME_STYLE), svs(_svs)
{
	updateMenuBarNeeded = false;
	m_canvas = NULL;
	m_sceneProperties = new SVSceneProperties(this,svs);
	m_lightProperties = new SVLightProperties(this,svs);
	m_objectProperties = new SVObjectProperties(this);
	m_materialProperties = new SVMaterialProperties(this,svs);
	m_sceneTree = new SVSceneTree(this,svs);
	userPreferences.currentWindowLayout = 2;
	static const char * sample_xpm[] = {
	// columns rows colors chars-per-pixel
	"32 32 6 1",
	"  c black",
	". c navy",
	"X c red",
	"o c yellow",
	"O c gray100",
	"+ c None",
	// pixels
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++              ++++++++++",
	"++++++++ ............ ++++++++++",
	"++++++++ ............ ++++++++++",
	"++++++++ .OO......... ++++++++++",
	"++++++++ .OO......... ++++++++++",
	"++++++++ .OO......... ++++++++++",
	"++++++++ .OO......              ",
	"++++++++ .OO...... oooooooooooo ",
	"         .OO...... oooooooooooo ",
	" XXXXXXX .OO...... oOOooooooooo ",
	" XXXXXXX .OO...... oOOooooooooo ",
	" XOOXXXX ......... oOOooooooooo ",
	" XOOXXXX ......... oOOooooooooo ",
	" XOOXXXX           oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oOOooooooooo ",
	" XOOXXXXXXXXX ++++ oooooooooooo ",
	" XOOXXXXXXXXX ++++ oooooooooooo ",
	" XXXXXXXXXXXX ++++              ",
	" XXXXXXXXXXXX ++++++++++++++++++",
	"              ++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++",
	"++++++++++++++++++++++++++++++++"
	};
	SetIcon(wxIcon(sample_xpm));

	CreateStatusBar();


	m_mgr.SetManagedWindow(this);

	UpdateEverything(); // slow. if specified by filename, loads scene from disk
	if (svs.fullscreen) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_FULLSCREEN));

	// load layouts, create layouts if load failed
	bool layoutLoaded = userPreferences.load();
	if (!layoutLoaded)
	{
		userPreferences.windowLayout[0].fullscreen = false;
		userPreferences.windowLayout[0].maximized = true;
		userPreferences.windowLayout[0].perspective = m_mgr.SavePerspective();
	}

	// setup dock art (colors etc)
	wxAuiDockArt* dockArt = new wxAuiDefaultDockArt;
	//dockArt->SetMetric(wxAUI_DOCKART_SASH_SIZE,4);
	//dockArt->SetColor(wxAUI_DOCKART_SASH_COLOUR,wxColour(255,255,255));

	//dockArt->SetMetric(wxAUI_DOCKART_GRIPPER_SIZE,0);
	//dockArt->SetColor(wxAUI_DOCKART_GRIPPER_COLOUR,wxColour(0,0,0));

	dockArt->SetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE,0);
	dockArt->SetColor(wxAUI_DOCKART_BORDER_COLOUR,wxColour(0,0,0));

	//dockArt->SetMetric(wxAUI_DOCKART_PANE_BUTTON_SIZE,20);
	//dockArt->SetColor(wxAUI_DOCKART_BACKGROUND_COLOUR,wxColour(250,0,0));

	dockArt->SetColor(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR,wxColour(235,235,255));
	dockArt->SetColor(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR,wxColour(140,140,160));
	dockArt->SetColor(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR,wxColour(0,0,0));
	dockArt->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR,wxColour(255,255,255));
	dockArt->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR,wxColour(150,150,150));
	dockArt->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR,wxColour(0,0,0));
	dockArt->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE,wxAUI_GRADIENT_HORIZONTAL);

	dockArt->SetMetric(wxAUI_DOCKART_CAPTION_SIZE,30);
	static wxFont font(13,wxFONTFAMILY_SWISS,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD);
	dockArt->SetFont(wxAUI_DOCKART_CAPTION_FONT,font);
	m_mgr.SetArtProvider(dockArt);

	// create panes
	m_mgr.AddPane(m_sceneTree, wxAuiPaneInfo().Name(wxT("scenetree")).Caption(wxT("Scene tree")).CloseButton(true).Left());
	m_mgr.AddPane(m_sceneProperties, wxAuiPaneInfo().Name(wxT("sceneproperties")).Caption(wxT("Scene properties")).CloseButton(true).Left());
	if (!layoutLoaded)
	{
		userPreferences.windowLayout[1].fullscreen = false;
		userPreferences.windowLayout[1].maximized = false;
		userPreferences.windowLayout[1].perspective = m_mgr.SavePerspective();
	}
	m_mgr.AddPane(m_lightProperties, wxAuiPaneInfo().Name(wxT("lightproperties")).Caption(wxT("Light properties")).CloseButton(true).Right());
	m_mgr.AddPane(m_objectProperties, wxAuiPaneInfo().Name(wxT("objectproperties")).Caption(wxT("Object properties")).CloseButton(true).Right());
	m_mgr.AddPane(m_materialProperties, wxAuiPaneInfo().Name(wxT("materialproperties")).Caption(wxT("Material properties")).CloseButton(true).Right());
	if (!layoutLoaded)
	{
		userPreferences.windowLayout[2].fullscreen = false;
		userPreferences.windowLayout[2].maximized = false;
		userPreferences.windowLayout[2].perspective = m_mgr.SavePerspective();
	}
	userPreferencesApplyToWx();
	m_mgr.Update();

}

SVFrame::~SVFrame()
{
	userPreferencesGatherFromWx();
	userPreferences.save();
	m_mgr.UnInit();
}


void SVFrame::OnExit(wxCommandEvent& event)
{
	// true is to force the frame to close
	Close(true);
}

static void dirtyLights(rr_gl::RRDynamicSolverGL* solver)
{
	for (unsigned i=0;i<solver->realtimeLights.size();i++)
	{
		solver->reportDirectIlluminationChange(i,true,true);
	}
}

void SVFrame::UpdateMenuBar()
{
	if (svs.fullscreen) return; // menu in fullscreen is disabled
	updateMenuBarNeeded = false;
	wxMenuBar *menuBar = new wxMenuBar;
	wxMenu *winMenu = NULL;

	// File...
	if (rr::RRScene::getSupportedLoaderExtensions() && rr::RRScene::getSupportedLoaderExtensions()[0])
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_FILE_OPEN_SCENE,_T("Open scene..."));
		winMenu->Append(ME_FILE_SAVE_SCENE,_T("Save scene"));
		winMenu->Append(ME_FILE_SAVE_SCENE_AS,_T("Save scene as..."));
		winMenu->Append(ME_FILE_SAVE_SCREENSHOT,_T("Save screenshot"),_T("Saves screenshot to desktop."));
		winMenu->Append(ME_FILE_SAVE_ENHANCED_SCREENSHOT,_T("Save enhanced screenshot (F9)"),_T("Saves enhanced screenshot to desktop, may fail on old GPUs."));
		winMenu->Append(ME_EXIT,_T("Exit"));
		menuBar->Append(winMenu, _T("File"));
	}


	// Camera...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_CAMERA_GENERATE_RANDOM,_T("Set random camera"));
		menuBar->Append(winMenu, _T("Camera"));
	}

	{
		winMenu = new wxMenu;
		winMenu->Append(ME_ENV_OPEN,_T("Load skybox..."),_T("Supported formats: cross-shaped 3:4 and 4:3 images, Quake-like sets of 6 images."));
		winMenu->Append(ME_ENV_WHITE,_T("Set white"),_T("Sets uniform white environment."));
		winMenu->Append(ME_ENV_BLACK,_T("Set black"),_T("Sets uniform black environment."));
		winMenu->Append(ME_ENV_WHITE_TOP,_T("Set white top"),_T("Sets uniform white environment in upper hemisphere, black in lower hemisphere."));
		menuBar->Append(winMenu, _T("Environment"));
	}

	// Lights...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_LIGHT_DIR,_T("Add Sun light"));
		winMenu->Append(ME_LIGHT_SPOT,_T("Add spot light (alt-s)"));
		winMenu->Append(ME_LIGHT_POINT,_T("Add point light (alt-o)"));
		winMenu->Append(ME_LIGHT_DELETE,_T("Delete selected light (del)"));
		menuBar->Append(winMenu, _T("Lights"));
	}


	// Global illumination...
	{
		winMenu = new wxMenu;
		winMenu->AppendRadioItem(ME_LIGHTING_DIRECT_REALTIME,_T("Direct illumination: realtime"));
		winMenu->AppendRadioItem(ME_LIGHTING_DIRECT_STATIC,_T("Direct illumination: static lightmap"));
		winMenu->AppendRadioItem(ME_LIGHTING_DIRECT_NONE,_T("Direct illumination: none"));
		switch (svs.renderLightDirect)
		{
			case LD_REALTIME: winMenu->Check(ME_LIGHTING_DIRECT_REALTIME,true); break;
			case LD_STATIC_LIGHTMAPS: winMenu->Check(ME_LIGHTING_DIRECT_STATIC,true); break;
			case LD_NONE: winMenu->Check(ME_LIGHTING_DIRECT_NONE,true); break;
		}
		winMenu->AppendSeparator();
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_FIREBALL_LDM,_T("Indirect illumination: realtime Fireball+LDM (fast+detailed)"),_T("Changes lighting technique to Fireball with LDM, fast and detailed realtime GI that supports lights, emissive materials, skylight."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_FIREBALL,_T("Indirect illumination: realtime Fireball (fast)"),_T("Changes lighting technique to Fireball, fast realtime GI that supports lights, emissive materials, skylight."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_ARCHITECT,_T("Indirect illumination: realtime Architect (no precalc)"),_T("Changes lighting technique to Architect, legacy realtime GI that supports lights, emissive materials."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_STATIC,_T("Indirect illumination: static lightmap"),_T("Changes lighting technique to precomputed lightmaps. If you haven't built lightmaps yet, everything will be dark."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_CONST,_T("Indirect illumination: constant ambient"));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_NONE,_T("Indirect illumination: none"));
		switch (svs.renderLightIndirect)
		{
			case LI_REALTIME_FIREBALL_LDM: winMenu->Check(ME_LIGHTING_INDIRECT_FIREBALL_LDM,true); break;
			case LI_REALTIME_FIREBALL: winMenu->Check(ME_LIGHTING_INDIRECT_FIREBALL,true); break;
			case LI_REALTIME_ARCHITECT: winMenu->Check(ME_LIGHTING_INDIRECT_ARCHITECT,true); break;
			case LI_STATIC_LIGHTMAPS: winMenu->Check(ME_LIGHTING_INDIRECT_STATIC,true); break;
			case LI_CONSTANT: winMenu->Check(ME_LIGHTING_INDIRECT_CONST,true); break;
			case LI_NONE: winMenu->Check(ME_LIGHTING_INDIRECT_NONE,true); break;
		}
		winMenu->AppendSeparator();
		winMenu->Append(ME_REALTIME_FIREBALL_BUILD,_T("Build Fireball..."),_T("(Re)builds Fireball, acceleration structure used by realtime GI. You can change GI quality here (from default ") wxSTRINGIZE_T(DEFAULT_FIREBALL_QUALITY) _T(")."));
		winMenu->Append(ME_REALTIME_LDM_BUILD,_T("Build LDM (light detail map)..."),_T("(Re)builds LDM, structure that adds per-pixel details to realtime GI. Takes tens of minutes to build. LDM is efficient only with good unwrap in scene."));
		winMenu->AppendSeparator();
		winMenu->Append(ME_STATIC_BUILD,_T("Build lightmaps..."),_T("(Re)builds per-vertex or per-pixel lightmaps. Per-pixel is efficient only with good unwrap in scene."));
		winMenu->Append(ME_STATIC_2D,_T("Inspect lightmaps in 2D"),_T("Shows lightmap in 2D, with unwrap wireframe."));
		winMenu->Append(ME_STATIC_BILINEAR,_T("Toggle lightmap bilinear interpolation"));
		//winMenu->Append(ME_STATIC_BUILD_1OBJ,_T("Build lightmap for selected obj, only direct..."),_T("For testing only."));
#ifdef DEBUG_TEXEL
		winMenu->Append(ME_STATIC_DIAGNOSE,_T("Diagnose texel..."),_T("For debugging purposes, shows rays traced from texel in final gather step."));
#endif
		winMenu->AppendSeparator();
		winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_2D,_T("Build 2d lightfield"),_T("Lightfield is illumination captured in 3d, lightmap for freely moving dynamic objects. Not saved to disk, for testing only."));
		winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_3D,_T("Build 3d lightfield"),_T("Lightfield is illumination captured in 3d, lightmap for freely moving dynamic objects. Not saved to disk, for testing only."));
		menuBar->Append(winMenu, _T("Global illumination"));
	}

	// Window...
	{
		winMenu = new wxMenu;
		winMenu->AppendCheckItem(ME_WINDOW_FULLSCREEN,_T("Fullscreen (F11)"),_T("Fullscreen mode uses full desktop resolution."));
		winMenu->Check(ME_WINDOW_FULLSCREEN,svs.fullscreen);
		winMenu->AppendCheckItem(ME_WINDOW_TREE,_T("Scene tree"),_T("Opens scene tree window."));
		winMenu->Check(ME_WINDOW_TREE,m_sceneTree->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_SCENE_PROPERTIES,_T("Scene properties"),_T("Opens scene properties window."));
		winMenu->Check(ME_WINDOW_SCENE_PROPERTIES,m_sceneProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_LIGHT_PROPERTIES,_T("Light properties"),_T("Opens light properties window and starts rendering light icons."));
		winMenu->Check(ME_WINDOW_LIGHT_PROPERTIES,m_lightProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_OBJECT_PROPERTIES,_T("Object properties"),_T("Opens object properties window."));
		winMenu->Check(ME_WINDOW_OBJECT_PROPERTIES,m_objectProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_MATERIAL_PROPERTIES,_T("Material properties"),_T("Opens material properties window."));
		winMenu->Check(ME_WINDOW_MATERIAL_PROPERTIES,m_materialProperties->IsShown());
		winMenu->AppendSeparator();
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT1,_T("Workspace 1"),_T("Custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT2,_T("Workspace 2"),_T("Custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT3,_T("Workspace 3"),_T("Custom window layout, changes automatically saved per user."));
		winMenu->Check(ME_WINDOW_LAYOUT1+userPreferences.currentWindowLayout,true);
		menuBar->Append(winMenu, _T("Window"));
	}

	// About...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_HELP,_T("Help (h)"));
		winMenu->Append(ME_CHECK_SOLVER,_T("Log solver diagnose"),_T("For diagnostic purposes."));
		winMenu->Append(ME_CHECK_SCENE,_T("Log scene errors"),_T("For diagnostic purposes."));
		winMenu->Append(ME_ABOUT,_T("About"));
		menuBar->Append(winMenu, _T("Help"));
	}

	wxMenuBar* oldMenuBar = GetMenuBar();
	SetMenuBar(menuBar);
	delete oldMenuBar;
}

void SVFrame::OnMenuEvent(wxCommandEvent& event)
{
#ifdef _WIN32
	__try
	{
		OnMenuEventCore(event);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		rr::RRReporter::report(rr::ERRO,"Processing event crashed.\n");
	}
#else
	OnMenuEventCore(event);
#endif
}

void SVFrame::OnMenuEventCore(wxCommandEvent& event)
{
	try
{
	RR_ASSERT(m_canvas);
	rr_gl::RRDynamicSolverGL*& solver = m_canvas->solver;
	RR_ASSERT(solver);
	rr::RRLightField*& lightField = m_canvas->lightField;
	bool& fireballLoadAttempted = m_canvas->fireballLoadAttempted;
	int* windowCoord = m_canvas->windowCoord;
	bool& envToBeDeletedOnExit = m_canvas->envToBeDeletedOnExit;

	switch (event.GetId())
	{

		//////////////////////////////// FILE ///////////////////////////////

		case ME_FILE_OPEN_SCENE:
			{
				// wildcard format: "BMP and GIF files (*.bmp;*.gif)|*.bmp;*.gif|PNG files (*.png)|*.png"
				std::string extensions = rr::RRScene::getSupportedLoaderExtensions();
				std::string wxextensions = "All scene formats|"+extensions;
				while (!extensions.empty())
				{
					size_t i = extensions.find(';');
					std::string ext = (i==-1) ? extensions : extensions.substr(0,i);
					wxextensions += std::string("|")+ext+'|'+ext;
					extensions.erase(0,ext.size()+1);
				}
				wxFileDialog dialog(this,"Choose a 3d scene","","",wxextensions.c_str(),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.sceneFilename);
				if (dialog.ShowModal()==wxID_OK)
				{
					svs.sceneFilename = dialog.GetPath();
					UpdateEverything();
				}
			}
			break;
		case ME_FILE_SAVE_SCENE:
			// no name yet?
			if (svs.sceneFilename.empty())
				goto save_scene_as;
			// name that can't be saved?
			{
				std::string extension = svs.sceneFilename.substr(svs.sceneFilename.find_last_of("."));
				std::string extensions = rr::RRScene::getSupportedSaverExtensions();
				bool extensionSupportsSave = !extension.empty() && extensions.find(extension)!=std::string::npos;
				if (!extensionSupportsSave) goto save_scene_as;
			}
			// valid name, save it
save_scene:
			if (m_canvas->manuallyOpenedScene)
			{
				m_canvas->manuallyOpenedScene->lights = m_canvas->solver->getLights();
				if (!m_canvas->manuallyOpenedScene->save(svs.sceneFilename.c_str()))
					wxMessageBox("Save failed.","Not saved.",wxOK|wxICON_ERROR);
			}
			else
			if (m_canvas->solver)
			{
				rr::RRScene scene;
				scene.objects = m_canvas->solver->getStaticObjects();
				scene.lights = m_canvas->solver->getLights();
				scene.environment = m_canvas->solver->getEnvironment();
				if (!scene.save(svs.sceneFilename.c_str()))
					wxMessageBox("Scene save failed.","Not saved.",wxOK|wxICON_ERROR);
				scene.environment = NULL; // would be deleted in destructor otherwise
			}
			break;
		case ME_FILE_SAVE_SCENE_AS:
			{
save_scene_as:
				// wildcard format: "BMP and GIF files (*.bmp;*.gif)|*.bmp;*.gif|PNG files (*.png)|*.png"
				std::string extensions = rr::RRScene::getSupportedSaverExtensions();
				if (extensions.empty())
				{
					wxMessageBox("Program built without saving.","No savers registered.",wxOK);
				}
				else
				{
					std::string wxextensions;
					while (!extensions.empty())
					{
						size_t i = extensions.find(';');
						std::string ext = (i==-1) ? extensions : extensions.substr(0,i);
						if (!wxextensions.empty())
							wxextensions += std::string("|");
						wxextensions += ext+'|'+ext;
						extensions.erase(0,ext.size()+1);
					}

					// delete extension if it can't be saved, dialog will automatically append supported one
					std::string presetFilename = svs.sceneFilename;
					std::string::size_type lastDot = svs.sceneFilename.find_last_of(".");
					std::string extension = (lastDot==std::string::npos) ? presetFilename : presetFilename.substr(lastDot);
					std::string extensions = rr::RRScene::getSupportedSaverExtensions();
					bool extensionSupportsSave = !extension.empty() && extensions.find(extension)!=std::string::npos;
					if (!extensionSupportsSave) presetFilename = presetFilename.substr(0,presetFilename.size()-extension.size());

					wxFileDialog dialog(this,"Save as","","",wxextensions.c_str(),wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
					dialog.SetPath(presetFilename);
					if (dialog.ShowModal()==wxID_OK)
					{
						svs.sceneFilename = dialog.GetPath();
						goto save_scene;
					}
				}
			}
			break;
		case ME_FILE_SAVE_SCREENSHOT:
		case ME_FILE_SAVE_ENHANCED_SCREENSHOT:
			{
				// Grabs content of backbuffer to sshot.
				wxSize size = m_canvas->GetSize();
				rr::RRBuffer* sshot = rr::RRBuffer::create(rr::BT_2D_TEXTURE,size.x,size.y,1,rr::BF_RGB,true,NULL);
				unsigned char* pixels = sshot->lock(rr::BL_DISCARD_AND_WRITE);
				glReadBuffer(GL_BACK);
				glReadPixels(0,0,size.x,size.y,GL_RGB,GL_UNSIGNED_BYTE,pixels);

				if (event.GetId()==ME_FILE_SAVE_SCREENSHOT)
				{
					// No more work, use sshot.
					sshot->unlock();
				}
				else
				{
					// Attempt to overwrite sshot by enhanced
					// frame rendered with 2*2*FSAA, 2*2*higher shadow resolution, 2*more shadow samples.
					// (even 3*3*FSAA works fine, 3*2560<8k, but it's probably too much for older cards)
					// (4*2560 uz by neslo na Radeonech 4xxx)
					const unsigned AA = 2;
					const unsigned W = size.x*AA;
					const unsigned H = size.y*AA;
					
					// 1. enhance shadows
					rr::RRVector<RealtimeLight*>& lights = m_canvas->solver->realtimeLights;
					unsigned* shadowSamples = new unsigned[lights.size()];
					unsigned* shadowRes = new unsigned[lights.size()];
					for (unsigned i=0;i<lights.size();i++)
					{
						RealtimeLight* rl = lights[i];
						shadowRes[i] = rl->getShadowmapSize();
						shadowSamples[i] = rl->getNumShadowSamples();
						rl->setShadowmapSize(shadowRes[i]*2);
						rl->setNumShadowSamples(8);
					}
					rr::RRDynamicSolver::CalculateParameters params;
					params.qualityIndirectStatic = 0; // set it to update only shadows
					params.qualityIndirectDynamic = 0;
					m_canvas->solver->calculate(&params); // renders into FBO, must go before renderingToBegin()

					// 2. alloc temporary textures
					rr::RRBuffer* bufColor = rr::RRBuffer::create(rr::BT_2D_TEXTURE,W,H,1,rr::BF_RGB,true,NULL);
					rr::RRBuffer* bufDepth = rr::RRBuffer::create(rr::BT_2D_TEXTURE,W,H,1,rr::BF_DEPTH,true,(unsigned char*)1);
					Texture texColor(bufColor,false,false);
					Texture texDepth(bufDepth,false,false);

					// 3. set new rendertarget, propagate new size to renderer
					if (texColor.renderingToBegin() && texDepth.renderingToBegin())
					{
						m_canvas->winWidth = W;
						m_canvas->winHeight = H;
						glViewport(0,0,W,H);

						// 4. disable automatic tonemapping, uses FBO, would not work
						bool oldTonemapping = svs.tonemappingAutomatic;
						svs.tonemappingAutomatic = false;

						// 5. render to texColor
						wxPaintEvent e;
						m_canvas->Paint(e);

						// 6. downscale to sshot
						unsigned char* pixelsBig = bufColor->lock(rr::BL_DISCARD_AND_WRITE);
						glPixelStorei(GL_PACK_ALIGNMENT,1);
						glReadPixels(0,0,W,H,GL_RGB,GL_UNSIGNED_BYTE,pixelsBig);
						unsigned char* pixelsSmall = sshot->lock(rr::BL_DISCARD_AND_WRITE);
						for (int j=0;j<size.y;j++)
							for (int i=0;i<size.x;i++)
								for (unsigned k=0;k<3;k++)
								{
									unsigned a = 0;
									for (int y=0;y<AA;y++)
										for (int x=0;x<AA;x++)
											a += pixelsBig[k+3*(AA*i+x+AA*size.x*(AA*j+y))];
									pixelsSmall[k+3*(i+size.x*j)] = (a+AA*AA/2)/(AA*AA);
								}
						sshot->unlock();
						bufColor->unlock();

						// 4. cleanup
						svs.tonemappingAutomatic = oldTonemapping;
					}

					// 3. cleanup
					texDepth.renderingToEnd();
					texColor.renderingToEnd();
					m_canvas->winWidth = size.x;
					m_canvas->winHeight = size.y;
					glViewport(0,0,size.x,size.y);

					// 2. cleanup
					delete bufDepth;
					delete bufColor;

					// 1. cleanup
					for (unsigned i=0;i<lights.size();i++)
					{
						RealtimeLight* rl = lights[i];
						rl->setShadowmapSize(shadowRes[i]);
						rl->setNumShadowSamples(shadowSamples[i]);
					}
					delete[] shadowSamples;
					delete[] shadowRes;
				}
				char screenshotFilename[1000]=".";
#ifdef _WIN32
				SHGetSpecialFolderPathA(NULL, screenshotFilename, CSIDL_DESKTOP, FALSE); // CSIDL_PERSONAL
#endif
				time_t t = time(NULL);
				sprintf(screenshotFilename+strlen(screenshotFilename),"/screenshot%04d.jpg",t%10000);
				rr::RRBuffer::SaveParameters saveParameters;
				saveParameters.jpegQuality = 100;
				if (sshot->save(screenshotFilename,NULL,&saveParameters))
					rr::RRReporter::report(rr::INF2,"Saved %s.\n",screenshotFilename);
				else
					rr::RRReporter::report(rr::WARN,"Error: Failed to saved %s.\n",screenshotFilename);
				delete sshot;
			}
			break;
		case ME_EXIT:
			Close();
			break;


		//////////////////////////////// CAMERA ///////////////////////////////

		case ME_CAMERA_GENERATE_RANDOM:
			svs.eye.setPosDirRangeRandomly(solver->getMultiObjectCustom());
			svs.cameraMetersPerSecond = svs.eye.getFar()*0.08f;
			break;



		//////////////////////////////// ENVIRONMENT ///////////////////////////////

		case ME_ENV_WHITE:
			svs.skyboxFilename = "../../data/maps/skybox/white.png";
			goto reload_skybox;
		case ME_ENV_BLACK:
			svs.skyboxFilename = "../../data/maps/skybox/black.png";
			goto reload_skybox;
		case ME_ENV_WHITE_TOP:
			svs.skyboxFilename = "../../data/maps/skybox/white_top.png";
			goto reload_skybox;
		case ME_ENV_OPEN:
			{
				wxFileDialog dialog(this,"Choose a skybox image","","","*.*",wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.skyboxFilename);
				if (dialog.ShowModal()!=wxID_OK)
					break;

				svs.skyboxFilename = dialog.GetPath();
			}
			goto reload_skybox;
		case ME_ENV_RELOAD: // not a menu item, just command we can call from outside
			{
reload_skybox:
				rr::RRBuffer* skybox = rr::RRBuffer::loadCube(svs.skyboxFilename.c_str());
				// skybox is used only if it exists
				if (skybox)
				{
					if (envToBeDeletedOnExit)
						delete solver->getEnvironment();
					solver->setEnvironment(skybox);
					envToBeDeletedOnExit = true;
				}
			}
			break;
			
		//////////////////////////////// LIGHTS ///////////////////////////////

		case ME_LIGHT_DIR:
		case ME_LIGHT_SPOT:
		case ME_LIGHT_POINT:
			{
				rr::RRLights newList = solver->getLights();
				rr::RRLight* newLight = NULL;
				switch (event.GetId())
				{
					case ME_LIGHT_DIR: newLight = rr::RRLight::createDirectionalLight(rr::RRVec3(-1),rr::RRVec3(1),true); break;
					case ME_LIGHT_SPOT: newLight = rr::RRLight::createSpotLight(svs.eye.pos,rr::RRVec3(1),svs.eye.dir,svs.eye.getFieldOfViewVerticalRad()/2,svs.eye.getFieldOfViewVerticalRad()/4); break;
					case ME_LIGHT_POINT: newLight = rr::RRLight::createPointLight(svs.eye.pos,rr::RRVec3(1)); break;
				}
				m_canvas->lightsToBeDeletedOnExit.push_back(newLight);
				newList.push_back(newLight);
				solver->setLights(newList); // RealtimeLight in light props is deleted here

				// select newly added light (it's probably better to not select it)
				//m_canvas->selectedType = ST_LIGHT;
				//svs.selectedLightIndex = newList.size()-1;

				updateSelection();
			}
			break;
		case ME_LIGHT_DELETE:
			if (svs.selectedLightIndex<solver->realtimeLights.size())
			{
				rr::RRLights newList = solver->getLights();

				if (newList[svs.selectedLightIndex]->rtProjectedTexture)
					newList[svs.selectedLightIndex]->rtProjectedTexture->stop();

				newList.erase(svs.selectedLightIndex);

				solver->setLights(newList); // RealtimeLight in light props is deleted here, lightprops is temporarily unsafe
				updateSelection(); // deletes lightprops
			}
			break;


		//////////////////////////////// SETTINGS ///////////////////////////////



		//////////////////////////////// GLOBAL ILLUMINATION - DIRECT ///////////////////////////////

		case ME_LIGHTING_DIRECT_REALTIME:
			svs.renderLightDirect = LD_REALTIME;
			if (svs.renderLightIndirect==LI_STATIC_LIGHTMAPS) // indirect must not stay lightmaps
				svs.renderLightIndirect = LI_CONSTANT;
			svs.renderLightmaps2d = 0;
			break;
		case ME_LIGHTING_DIRECT_NONE:
			svs.renderLightDirect = LD_NONE;
			if (svs.renderLightIndirect==LI_STATIC_LIGHTMAPS) // indirect must not stay lightmaps
				svs.renderLightIndirect = LI_CONSTANT;
			svs.renderLightmaps2d = 0;
			break;


		//////////////////////////////// GLOBAL ILLUMINATION - INDIRECT ///////////////////////////////

		case ME_LIGHTING_INDIRECT_FIREBALL_LDM:
			// starts fireball, sets LI_REALTIME_FIREBALL
			OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_FIREBALL));
			// enables ldm if in ram
			for (unsigned i=0;i<solver->getStaticObjects().size();i++)
				if (solver->getStaticObjects()[i]->illumination.getLayer(svs.ldmLayerNumber))
				{
					svs.renderLightIndirect = LI_REALTIME_FIREBALL_LDM;
					break;
				}
			// if ldm not in ram, try to load it from disk
			if (svs.renderLightIndirect==LI_REALTIME_FIREBALL && solver->getStaticObjects().loadLayer(svs.ldmLayerNumber,LDM_PREFIX,LDM_POSTFIX))
			{
				svs.renderLightIndirect = LI_REALTIME_FIREBALL_LDM;
			}
			// if ldm not in ram and not on disk, keep it disabled
			break;

		case ME_LIGHTING_INDIRECT_FIREBALL:
			svs.renderLightIndirect = LI_REALTIME_FIREBALL;
			if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS) // direct must not stay lightmaps
				svs.renderLightDirect = LD_REALTIME;
			svs.renderLightmaps2d = 0;
			dirtyLights(solver);
			if (solver->getInternalSolverType()!=rr::RRDynamicSolver::FIREBALL && solver->getInternalSolverType()!=rr::RRDynamicSolver::BOTH)
			{
				if (!fireballLoadAttempted)
				{
					fireballLoadAttempted = true;
					solver->loadFireball(NULL,true);
				}
			}
			if (solver->getInternalSolverType()!=rr::RRDynamicSolver::FIREBALL && solver->getInternalSolverType()!=rr::RRDynamicSolver::BOTH)
			{
				// ask no questions, it's possible scene is loading right now and it's not safe to render/idle. dialog would render/idle on background
				solver->buildFireball(DEFAULT_FIREBALL_QUALITY,NULL);
				dirtyLights(solver);
				// this would ask questions
				//OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_REALTIME_FIREBALL_BUILD));
			}
			break;

		case ME_LIGHTING_INDIRECT_ARCHITECT:
			svs.renderLightIndirect = LI_REALTIME_ARCHITECT;
			if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS) // direct must not stay lightmaps
				svs.renderLightDirect = LD_REALTIME;
			svs.renderLightmaps2d = 0;
			dirtyLights(solver);
			fireballLoadAttempted = false;
			solver->leaveFireball();
			break;

		case ME_LIGHTING_DIRECT_STATIC:
		case ME_LIGHTING_INDIRECT_STATIC:
			svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
			svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
			svs.renderLightmaps2d = 0;
			// checks whether lightmap exists in ram
			for (unsigned i=0;i<solver->getStaticObjects().size();i++)
				if (solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber))
					goto atLeastOneLightmapBufferExists;
			// try to load lightmaps from disk
			solver->getStaticObjects().loadLayer(svs.staticLayerNumber,LMAP_PREFIX,LMAP_POSTFIX);
			atLeastOneLightmapBufferExists:
			break;

		case ME_LIGHTING_INDIRECT_CONST:
			svs.renderLightIndirect = LI_CONSTANT;
			if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS) // direct must not stay lightmaps
				svs.renderLightDirect = LD_REALTIME;
			svs.renderLightmaps2d = 0;
			break;

		case ME_LIGHTING_INDIRECT_NONE:
			svs.renderLightIndirect = LI_NONE;
			if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS) // direct must not stay lightmaps
				svs.renderLightDirect = LD_REALTIME;
			svs.renderLightmaps2d = 0;
			break;


		//////////////////////////////// GLOBAL ILLUMINATION - BUILD ///////////////////////////////

		case ME_REALTIME_LDM_BUILD:
			{
				unsigned res = 256;
				unsigned quality = 100;
				if (getQuality("LDM build",this,quality) && getResolution("LDM build",this,res,false))
				{
					// if in fireball mode, leave it, otherwise updateLightmaps() below fails
					fireballLoadAttempted = false;
					solver->leaveFireball();

					// build ldm
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
						solver->getStaticObjects()[i]->illumination.getLayer(svs.ldmLayerNumber) =
							rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
					rr::RRDynamicSolver::UpdateParameters paramsDirect(quality);
					paramsDirect.applyLights = 0;
					rr::RRDynamicSolver::UpdateParameters paramsIndirect(quality);
					paramsIndirect.applyLights = 0;
					paramsIndirect.locality = 1;
					rr::RRBuffer* oldEnv = solver->getEnvironment();
					rr::RRBuffer* newEnv = rr::RRBuffer::createSky(rr::RRVec4(0.5f),rr::RRVec4(0.5f)); // higher sky color would decrease effect of emissive materials
					solver->setEnvironment(newEnv);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.backgroundColor = rr::RRVec4(0.5f);
					filtering.wrap = false;
					filtering.smoothBackground = true;
					solver->updateLightmaps(svs.ldmLayerNumber,-1,-1,&paramsDirect,&paramsIndirect,&filtering); 
					solver->setEnvironment(oldEnv);
					delete newEnv;

					// save ldm to disk
					solver->getStaticObjects().saveLayer(svs.ldmLayerNumber,LDM_PREFIX,LDM_POSTFIX);

					// switch to fireball+ldm
					OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_LIGHTING_INDIRECT_FIREBALL_LDM));
				}
			}
			break;
		case ME_REALTIME_FIREBALL_BUILD:
			{
				static unsigned quality = DEFAULT_FIREBALL_QUALITY;
				if (getQuality("Fireball build",this,quality))
				{
					svs.renderLightDirect = LD_REALTIME;
					svs.renderLightIndirect = LI_REALTIME_FIREBALL;
					svs.renderLightmaps2d = 0;
					solver->buildFireball(quality,NULL);
					dirtyLights(solver);
					fireballLoadAttempted = true;
				}
			}
			break;

		case ME_STATIC_2D:
			svs.renderLightmaps2d = 1;
			break;
		case ME_STATIC_BILINEAR:
			svs.renderLightmapsBilinear = !svs.renderLightmapsBilinear;
			svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
			svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
			for (unsigned i=0;i<solver->getStaticObjects().size();i++)
			{	
				if (solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber) && solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
				{
					glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
					getTexture(solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber))->bindTexture();
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, svs.renderLightmapsBilinear?GL_LINEAR:GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, svs.renderLightmapsBilinear?GL_LINEAR:GL_NEAREST);
				}
			}
			break;
		case ME_STATIC_BUILD_LIGHTFIELD_2D:
			{
				// create solver if it doesn't exist yet
				if (solver->getInternalSolverType()==rr::RRDynamicSolver::NONE)
				{
					solver->calculate();
				}

				rr::RRVec4 aabbMin,aabbMax;
				solver->getMultiObjectCustom()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,NULL);
				aabbMin.y = aabbMax.y = svs.eye.pos.y;
				aabbMin.w = aabbMax.w = 0;
				delete lightField;
				lightField = rr::RRLightField::create(aabbMin,aabbMax-aabbMin,1);
				lightField->captureLighting(solver,0);
			}
			break;
		case ME_STATIC_BUILD_LIGHTFIELD_3D:
			{
				// create solver if it doesn't exist yet
				if (solver->getInternalSolverType()==rr::RRDynamicSolver::NONE)
				{
					solver->calculate();
				}

				rr::RRVec4 aabbMin,aabbMax;
				solver->getMultiObjectCustom()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,NULL);
				aabbMin.w = aabbMax.w = 0;
				delete lightField;
				lightField = rr::RRLightField::create(aabbMin,aabbMax-aabbMin,1);
				lightField->captureLighting(solver,0);
			}
			break;
		/*case ME_STATIC_BUILD_1OBJ:
			{
				unsigned quality = 100;
				if (getQuality("Lightmap build",this,quality))
				{
					// calculate 1 object, direct lighting
					solver->leaveFireball();
					fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(quality);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.wrap = false;
					solver->updateLightmap(svs.selectedObjectIndex,solver->getStaticObjects()[svs.selectedObjectIndex]->illumination->getLayer(svs.staticLayerNumber),NULL,NULL,&params,&filtering);
					svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
					svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
					// propagate computed data from buffers to textures
					if (solver->getStaticObjects()[svs.selectedObjectIndex]->illumination->getLayer(svs.staticLayerNumber) && solver->getStaticObjects()[svs.selectedObjectIndex]->illumination->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
						getTexture(solver->getStaticObjects()[svs.selectedObjectIndex]->illumination->getLayer(svs.staticLayerNumber),true,false); // don't compres lmaps(ugly 4x4 blocks on HD2400)
					// reset cache, GL texture ids constant, but somehow rendered maps are not updated without display list rebuild
					solver->resetRenderCache();
				}
			}
			break;*/
#ifdef DEBUG_TEXEL
		case ME_STATIC_DIAGNOSE:
			{
				if (m_canvas->centerObject!=UINT_MAX)
				{
					solver->leaveFireball();
					fireballLoadAttempted = false;
					unsigned quality = 100;
					if (getQuality("Lightmap diagnose",this,quality))
					{
						rr::RRDynamicSolver::UpdateParameters params(quality);
						params.debugObject = m_canvas->centerObject;
						params.debugTexel = m_canvas->centerTexel;
						params.debugTriangle = (m_canvas->centerTexel==UINT_MAX)?m_canvas->centerTriangle:UINT_MAX;
						params.debugRay = SVRayLog::push_back;
						SVRayLog::size = 0;
						solver->updateLightmaps(svs.staticLayerNumber,-1,-1,&params,&params,NULL);
					}
				}
				else
				{
					rr::RRReporter::report(rr::WARN,"No lightmap in center of screen.\n");
				}
			}
			break;
#endif
		case ME_STATIC_BUILD:
			{
				unsigned res = 256; // 0=vertex buffers
				unsigned quality = 100;
				if (getResolution("Lightmap build",this,res,true) && getQuality("Lightmap build",this,quality))
				{
					// allocate buffers
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
					{
						if (solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices())
						{
							delete solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber);
							solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber) = res
								? rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL)
								: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
						}
					}

					// calculate all
					solver->leaveFireball();
					fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(quality);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.wrap = false;
					solver->updateLightmaps(svs.staticLayerNumber,-1,-1,&params,&params,&filtering);
					svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
					svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
					// propagate computed data from buffers to textures
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
					{
						if (solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber) && solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
							getTexture(solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber),true,false); // don't compres lmaps(ugly 4x4 blocks on HD2400)
					}

					// save calculated lightmaps
					solver->getStaticObjects().saveLayer(svs.staticLayerNumber,LMAP_PREFIX,LMAP_POSTFIX);
				}
			}
			break;


		///////////////////////////////// WINDOW ////////////////////////////////

		case ME_WINDOW_FULLSCREEN:
			svs.fullscreen = !svs.fullscreen;
			if (svs.fullscreen)
			{
				// wxFULLSCREEN_ALL does not work for menu (probably wx error), hide it manualy
				wxMenuBar* oldMenuBar = GetMenuBar();
				SetMenuBar(NULL);
				delete oldMenuBar;
			}
			ShowFullScreen(svs.fullscreen,wxFULLSCREEN_ALL);
			if (!svs.fullscreen)
			{
				// wxFULLSCREEN_ALL does not work for menu (probably wx error), unhide it manualy
				UpdateMenuBar();
			}
			GetPosition(windowCoord+0,windowCoord+1);
			GetSize(windowCoord+2,windowCoord+3);
			break;
		case ME_WINDOW_TREE:
			m_mgr.GetPane(wxT("scenetree")).Show(!m_sceneTree->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_SCENE_PROPERTIES:
			m_mgr.GetPane(wxT("sceneproperties")).Show(!m_sceneProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_LIGHT_PROPERTIES:
			m_mgr.GetPane(wxT("lightproperties")).Show(!m_lightProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_OBJECT_PROPERTIES:
			m_mgr.GetPane(wxT("objectproperties")).Show(!m_objectProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_MATERIAL_PROPERTIES:
			m_mgr.GetPane(wxT("materialproperties")).Show(!m_materialProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_LAYOUT1:
		case ME_WINDOW_LAYOUT2:
		case ME_WINDOW_LAYOUT3:
			userPreferencesGatherFromWx();
			userPreferences.currentWindowLayout = event.GetId()-ME_WINDOW_LAYOUT1;
			userPreferencesApplyToWx();
			break;


		//////////////////////////////// HELP ///////////////////////////////

		case ME_HELP: svs.renderHelp = !svs.renderHelp; break;
		case ME_CHECK_SOLVER: solver->checkConsistency(); break;
		case ME_CHECK_SCENE:
			solver->getStaticObjects().checkConsistency("static");
			solver->getDynamicObjects().checkConsistency("dynamic");
			break;
		case ME_ABOUT:
			{
				wxIcon* icon = loadIcon(tmpstr("%s../maps/sv_logo.png",svs.pathToShaders));
				wxAboutDialogInfo info;
				if (icon) info.SetIcon(*icon);
				info.SetName("Lightsprint SDK");
				info.SetWebSite("http://lightsprint.com");
				info.SetCopyright("(c) 1999-2010 Stepan Hrbek, Lightsprint");
				wxAboutBox(info);
				delete icon;
			}
			break;

			
	}

	UpdateMenuBar();

}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Processing event failed.\n");
	}
}

void SVFrame::OnPaneOpenClose(wxAuiManagerEvent& event)
{
	// it's too soon to UpdateMenuBar(), pane that is closing is still open
	// but next SVCanvas::OnPaint will call AfterPaneOpenClose()
	updateMenuBarNeeded = true;
	event.Skip();
}

void SVFrame::AfterPaneOpenClose()
{
	// doing this in SVFrame::OnPaneOpenClose would be too soon, pane that is closing is still open
	// doing this in SVFrame::OnMenuOpen would be too late, menu would open with old items
	// doing this in SVFrame::OnMouseMotion is unreliable, sometimes mouse moves 10s before it is called, and user sees outdated menu
	if (updateMenuBarNeeded)
		UpdateMenuBar();
}

EntityId SVFrame::getSelectedEntity() const
{
	switch (m_canvas->selectedType)
	{
		case ST_LIGHT:
			return EntityId(m_canvas->selectedType,svs.selectedLightIndex);
		case ST_STATIC_OBJECT:
			return EntityId(m_canvas->selectedType,svs.selectedObjectIndex);
		case ST_DYNAMIC_OBJECT:
			return EntityId(m_canvas->selectedType,svs.selectedObjectIndex);
		case ST_CAMERA:
			return EntityId(m_canvas->selectedType,0);
		default:
			return EntityId(m_canvas->selectedType,0);
	}
}

void SVFrame::selectEntity(EntityId entity, bool updateSceneTree, SelectEntityAction action)
{
	bool alreadySelected = entity==getSelectedEntity();
	switch (entity.type)
	{
		case ST_LIGHT:
			// update light properties
			if (m_lightProperties->IsShown())
			{
				m_lightProperties->setLight(m_canvas->solver->realtimeLights[entity.index],svs.precision);
			}

			m_canvas->selectedType = entity.type;
			svs.selectedLightIndex = entity.index;
			if (action==SEA_ACTION) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_LIGHT_PROPERTIES));
			break;

		case ST_STATIC_OBJECT:
		case ST_DYNAMIC_OBJECT:
			// update object properties
			if (m_objectProperties->IsShown())
			{
				const rr::RRObjects& objects = (entity.type==ST_STATIC_OBJECT)?m_canvas->solver->getStaticObjects():m_canvas->solver->getDynamicObjects();
				m_objectProperties->setObject(objects[entity.index],svs.precision);
			}

			m_canvas->selectedType = entity.type;
			svs.selectedObjectIndex = entity.index;
			if (action==SEA_ACTION) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_OBJECT_PROPERTIES));
			break;

		case ST_CAMERA:
			m_canvas->selectedType = entity.type;
			break;

	}

	if (updateSceneTree && m_sceneTree)
	{
		m_sceneTree->selectItem(entity);
	}
}

void SVFrame::updateSelection()
{
	// update svs
	if (svs.selectedLightIndex>=m_canvas->solver->getLights().size())
	{
		svs.selectedLightIndex = m_canvas->solver->getLights().size();
		if (svs.selectedLightIndex)
			svs.selectedLightIndex--;
		else
		if (m_canvas->selectedType==ST_LIGHT)
			m_canvas->selectedType = ST_CAMERA;
	}

	// update light props
	if (svs.selectedLightIndex>=m_canvas->solver->getLights().size())
	{
		m_lightProperties->setLight(NULL,svs.precision);
	}
	else
	{
		m_lightProperties->setLight(m_canvas->solver->realtimeLights[svs.selectedLightIndex],svs.precision);
	}

	// update object props
	if (svs.selectedObjectIndex>=m_canvas->solver->getStaticObjects().size())
	{
		m_objectProperties->setObject(NULL,svs.precision);
	}
	else
	{
		m_objectProperties->setObject(m_canvas->solver->getStaticObjects()[svs.selectedObjectIndex],svs.precision);
	}

	// update scene tree
	m_sceneTree->updateContent(m_canvas->solver);
	m_sceneTree->selectItem(getSelectedEntity());
}

void SVFrame::updateSceneTree()
{
	m_sceneTree->updateContent(m_canvas->solver);
}

BEGIN_EVENT_TABLE(SVFrame, wxFrame)
	EVT_MENU(wxID_EXIT, SVFrame::OnExit)
	EVT_MENU(wxID_ANY, SVFrame::OnMenuEvent)
	EVT_AUI_PANE_RESTORE(SVFrame::OnPaneOpenClose)
	EVT_AUI_PANE_CLOSE(SVFrame::OnPaneOpenClose)
END_EVENT_TABLE()

}; // namespace

#endif // SUPPORT_SCENEVIEWER
