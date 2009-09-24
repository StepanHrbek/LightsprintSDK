// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
//
// include

#include "SVFrame.h"

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/RRScene.h"
#include "SVRayLog.h"
#include "SVSolver.h"
#include "SVSaveLoad.h"
#include "SVLightProperties.h"
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
static bool getSpeed(wxWindow* parent, float& speed)
{
	return getFactor(parent,speed,"Please adjust camera speed (m/s)","Camera speed");
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
		SetTitle(APP_NAME+" - "+svs.sceneFilename);
	else
		SetTitle(APP_NAME);
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
	if (svs.fullscreen) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_FULLSCREEN));

	UpdateTitle();

	updateSelection();

	m_mgr.AddPane(m_canvas, wxAuiPaneInfo().Name(wxT("glcanvas")).CenterPane());
	m_mgr.Update();
}

SVFrame* SVFrame::Create(SceneViewerStateEx& svs)
{
	// open at ~50% of screen size
	int x,y,width,height;
	::wxClientDisplayRect(&x,&y,&width,&height);
	const int border = (width+height)/25;
	SVFrame *frame = new SVFrame(NULL, APP_NAME+" - loading", wxPoint(x+2*border,y+border), wxSize(width-4*border,height-2*border), svs);


	frame->UpdateEverything(); // slow. if specified by filename, loads scene from disk
	frame->Update();

	return frame;
}

void SVFrame::OnExit(wxCommandEvent& event)
{
	// true is to force the frame to close
	Close(true);
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

SVFrame::SVFrame(wxWindow* _parent, const wxString& _title, const wxPoint& _pos, const wxSize& _size, SceneViewerStateEx& _svs)
	: wxFrame(_parent, wxID_ANY, _title, _pos, _size, wxDEFAULT_FRAME_STYLE), svs(_svs)
{
	m_canvas = NULL;
	m_lightProperties = new SVLightProperties(this);
	m_sceneTree = new SVSceneTree(this,svs);

	m_mgr.SetManagedWindow(this);
	m_mgr.AddPane(m_sceneTree, wxAuiPaneInfo().Name(wxT("scenetree")).Caption(wxT("Scene tree")).CloseButton(true).Left());
	m_mgr.AddPane(m_lightProperties, wxAuiPaneInfo().Name(wxT("lightproperties")).Caption(wxT("Light properties")).CloseButton(true).Left());
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
}

SVFrame::~SVFrame()
{
	m_mgr.UnInit();
}

void SVFrame::UpdateMenuBar()
{
	updateMenuBarNeeded = false;
	wxMenuBar *menuBar = new wxMenuBar;
	wxMenu *winMenu = NULL;

	// File...
	if (rr::RRScene::getSupportedExtensions() && rr::RRScene::getSupportedExtensions()[0])
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_FILE_OPEN_SCENE,_T("Open scene..."));
		winMenu->Append(ME_FILE_SAVE_SCREENSHOT,_T("Save screenshot"),_T("Saves screenshot to desktop."));
		winMenu->Append(ME_EXIT,_T("Exit"));
		menuBar->Append(winMenu, _T("File"));
	}


	// Camera...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_CAMERA_SPEED,_T("Set camera speed..."));
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
		winMenu->Append(ME_LIGHT_DELETE,_T("Delete selected light"));
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

	// Render...
	{
		winMenu = new wxMenu;
		winMenu->AppendCheckItem(ME_RENDER_DIFFUSE,_T("Diffuse color"),_T("Toggles between rendering diffuse colors and diffuse white. With diffuse color disabled, color bleeding is usually clearly visible."));
		winMenu->Check(ME_RENDER_DIFFUSE,svs.renderMaterialDiffuse);
		winMenu->AppendCheckItem(ME_RENDER_SPECULAR,_T("Specular reflection"),_T("Toggles rendering specular reflections. Disabling them could make huge highly specular scenes render faster."));
		winMenu->Check(ME_RENDER_SPECULAR,svs.renderMaterialSpecular);
		winMenu->AppendCheckItem(ME_RENDER_EMISSION,_T("Emittance"),_T("Toggles rendering emittance of emissive surfaces."));
		winMenu->Check(ME_RENDER_EMISSION,svs.renderMaterialEmission);
		winMenu->AppendCheckItem(ME_RENDER_TRANSPARENT,_T("Transparency"),_T("Toggles rendering transparency of semi-transparent surfaces. Disabling it could make rendering faster."));
		winMenu->Check(ME_RENDER_TRANSPARENT,svs.renderMaterialTransparency);
		winMenu->AppendCheckItem(ME_RENDER_TEXTURES,_T("Material textures (ctrl-t)"),_T("Toggles between material textures and flat colors. Disabling textures could make rendering faster."));
		winMenu->Check(ME_RENDER_TEXTURES,svs.renderMaterialTextures);
		winMenu->AppendCheckItem(ME_RENDER_WIREFRAME,_T("Wireframe (ctrl-w)"),_T("Toggles between solid and wireframe rendering modes."));
		winMenu->Check(ME_RENDER_WIREFRAME,svs.renderWireframe);
		winMenu->AppendSeparator();
		winMenu->AppendCheckItem(ME_RENDER_ICONS,_T("Lights"),_T("Toggles rendering light wire models."));
		winMenu->Check(ME_RENDER_ICONS,svs.renderIcons);
		winMenu->AppendCheckItem(ME_RENDER_HELPERS,_T("Helpers/dignostics (ctrl-h)"),_T("Helpers are all non-scene elements rendered with scene, usually for diagnostic purposes."));
		winMenu->Check(ME_RENDER_HELPERS,svs.renderHelpers);
		winMenu->AppendCheckItem(ME_RENDER_WATER,_T("Water"),_T("Water is water-like plane with adjustable elevation."));
		winMenu->Check(ME_RENDER_WATER,svs.renderWater);
		winMenu->AppendCheckItem(ME_RENDER_FPS,_T("FPS (ctrl-f)"),_T("FPS counter shows number of frames rendered in last second."));
		winMenu->Check(ME_RENDER_FPS,svs.renderFPS);
		winMenu->AppendCheckItem(ME_RENDER_LOGO,_T("Logo"),_T("Logo is loaded from data/maps/logo.png."));
		winMenu->Check(ME_RENDER_LOGO,svs.renderLogo);
		winMenu->AppendCheckItem(ME_RENDER_VIGNETTE,_T("Vignettation"),_T("Vignette overlay is loaded from data/maps/vignette.png."));
		winMenu->Check(ME_RENDER_VIGNETTE,svs.renderVignette);
		winMenu->AppendCheckItem(ME_RENDER_TONEMAPPING,_T("Tone mapping"),_T("Tone mapping automatically adjusts fullscreen brightness. It simulates eyes adapting to dark or bright environment."));
		winMenu->Check(ME_RENDER_TONEMAPPING,svs.adjustTonemapping);
		winMenu->AppendSeparator();
		winMenu->Append(ME_RENDER_BRIGHTNESS,_T("Adjust brightness..."),_T("Makes it possible to manually set brightness if tone mapping is disabled."));
		winMenu->Append(ME_RENDER_CONTRAST,_T("Adjust contrast..."));
		winMenu->Append(ME_RENDER_WATER_LEVEL,_T("Adjust water level..."));
		menuBar->Append(winMenu, _T("Render"));
	}

	// Window...
	{
		winMenu = new wxMenu;
		winMenu->AppendCheckItem(ME_WINDOW_FULLSCREEN,_T("Fullscreen (F11)"),_T("Fullscreen mode uses full desktop resolution."));
		winMenu->Check(ME_WINDOW_FULLSCREEN,svs.fullscreen);
		winMenu->AppendCheckItem(ME_WINDOW_TREE,_T("Scene tree"),_T("Opens scene tree window."));
		winMenu->Check(ME_WINDOW_TREE,m_sceneTree->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_PROPERTIES,_T("Light properties"),_T("Opens light properties window."));
		winMenu->Check(ME_WINDOW_PROPERTIES,m_lightProperties->IsShown());
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
	RR_ASSERT(m_canvas);
	SVSolver*& solver = m_canvas->solver;
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
				wxFileDialog dialog(this,"Choose a 3d scene","","",rr::RRScene::getSupportedExtensions(),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.sceneFilename);
				if (dialog.ShowModal()==wxID_OK)
				{
					svs.sceneFilename = dialog.GetPath();
					UpdateEverything();
				}
			}
			break;
		case ME_FILE_SAVE_SCREENSHOT:
			{
				wxSize size = m_canvas->GetSize();
				rr::RRBuffer* sshot = rr::RRBuffer::create(rr::BT_2D_TEXTURE,size.x,size.y,1,rr::BF_RGB,true,NULL);
				unsigned char* pixels = sshot->lock(rr::BL_DISCARD_AND_WRITE);
				glReadBuffer(GL_BACK);
				glReadPixels(0,0,size.x,size.y,GL_RGB,GL_UNSIGNED_BYTE,pixels);
				sshot->unlock();
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
		case ME_CAMERA_SPEED:
			getSpeed(this,svs.cameraMetersPerSecond);
			break;


		//////////////////////////////// ENVIRONMENT ///////////////////////////////

		case ME_ENV_WHITE:
		case ME_ENV_BLACK:
		case ME_ENV_WHITE_TOP:
			if (envToBeDeletedOnExit)
			{
				delete solver->getEnvironment();
			}
			envToBeDeletedOnExit = true;
			switch (event.GetId())
			{
				case ME_ENV_WHITE: solver->setEnvironment(rr::RRBuffer::createSky()); break;
				case ME_ENV_BLACK: solver->setEnvironment(NULL); break;
				case ME_ENV_WHITE_TOP: solver->setEnvironment(rr::RRBuffer::createSky(rr::RRVec4(1),rr::RRVec4(0))); break;
			}
			break;
		case ME_ENV_OPEN:
			{
				wxFileDialog dialog(this,"Choose a skybox image","","","*.*",wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.skyboxFilename);
				if (dialog.ShowModal()!=wxID_OK)
					break;

				svs.skyboxFilename = dialog.GetPath();
			}
			// intentionally no break
		case ME_ENV_RELOAD: // not a menu item, just command we can call from outside
			{
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

				// select newly added light
				m_canvas->selectedType = ST_LIGHT;
				svs.selectedLightIndex = newList.size()-1;

				updateSelection();
			}
			break;
		case ME_LIGHT_DELETE:
			if (svs.selectedLightIndex<solver->realtimeLights.size())
			{
				rr::RRLights newList = solver->getLights();
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
			for (unsigned i=0;i<solver->getNumObjects();i++)
				if (solver->getIllumination(i)->getLayer(svs.ldmLayerNumber))
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
			solver->dirtyLights();
			if (solver->getInternalSolverType()!=rr::RRDynamicSolver::FIREBALL && solver->getInternalSolverType()!=rr::RRDynamicSolver::BOTH)
			{
				if (!fireballLoadAttempted)
				{
					fireballLoadAttempted = true;
					solver->loadFireball(svs.sceneFilename.empty()?NULL:tmpstr("%s.fireball",svs.sceneFilename.c_str()));
				}
			}
			if (solver->getInternalSolverType()!=rr::RRDynamicSolver::FIREBALL && solver->getInternalSolverType()!=rr::RRDynamicSolver::BOTH)
			{
				// ask no questions, it's possible scene is loading right now and it's not safe to render/idle. dialog would render/idle on background
				solver->buildFireball(DEFAULT_FIREBALL_QUALITY,svs.sceneFilename.empty()?NULL:tmpstr("%s.fireball",svs.sceneFilename.c_str()));
				solver->dirtyLights();
				// this would ask questions
				//OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_REALTIME_FIREBALL_BUILD));
			}
			break;

		case ME_LIGHTING_INDIRECT_ARCHITECT:
			svs.renderLightIndirect = LI_REALTIME_ARCHITECT;
			if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS) // direct must not stay lightmaps
				svs.renderLightDirect = LD_REALTIME;
			svs.renderLightmaps2d = 0;
			solver->dirtyLights();
			fireballLoadAttempted = false;
			solver->leaveFireball();
			break;

		case ME_LIGHTING_DIRECT_STATIC:
		case ME_LIGHTING_INDIRECT_STATIC:
			svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
			svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
			svs.renderLightmaps2d = 0;
			// checks whether lightmap exists in ram
			for (unsigned i=0;i<solver->getNumObjects();i++)
				if (solver->getIllumination(i)->getLayer(svs.staticLayerNumber))
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
					for (unsigned i=0;i<solver->getNumObjects();i++)
						solver->getIllumination(i)->getLayer(svs.ldmLayerNumber) =
							rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
					rr::RRDynamicSolver::UpdateParameters paramsDirect(quality);
					paramsDirect.applyLights = 0;
					rr::RRDynamicSolver::UpdateParameters paramsIndirect(quality);
					paramsIndirect.applyLights = 0;
					paramsIndirect.locality = 1;
					const rr::RRBuffer* oldEnv = solver->getEnvironment();
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
				unsigned quality = DEFAULT_FIREBALL_QUALITY;
				if (getQuality("Fireball build",this,quality))
				{
					svs.renderLightDirect = LD_REALTIME;
					svs.renderLightIndirect = LI_REALTIME_FIREBALL;
					svs.renderLightmaps2d = 0;
					solver->buildFireball(quality,svs.sceneFilename.empty()?NULL:tmpstr("%s.fireball",svs.sceneFilename.c_str()));
					solver->dirtyLights();
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
			for (unsigned i=0;i<solver->getNumObjects();i++)
			{	
				if (solver->getIllumination(i) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
				{
					glActiveTexture(GL_TEXTURE0+TEXTURE_2D_LIGHT_INDIRECT);
					getTexture(solver->getIllumination(i)->getLayer(svs.staticLayerNumber))->bindTexture();
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
					solver->updateLightmap(svs.selectedObjectIndex,solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber),NULL,NULL,&params,&filtering);
					svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
					svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
					// propagate computed data from buffers to textures
					if (solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber) && solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
						getTexture(solver->getIllumination(svs.selectedObjectIndex)->getLayer(svs.staticLayerNumber))->reset(true,false); // don't compres lmaps(ugly 4x4 blocks on HD2400)
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
						if (solver->getIllumination(i) && solver->getObject(i)->getCollider()->getMesh()->getNumVertices())
						{
							delete solver->getIllumination(i)->getLayer(svs.staticLayerNumber);
							solver->getIllumination(i)->getLayer(svs.staticLayerNumber) = res
								? rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL)
								: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
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
						if (solver->getIllumination(i) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber) && solver->getIllumination(i)->getLayer(svs.staticLayerNumber)->getType()==rr::BT_2D_TEXTURE)
							getTexture(solver->getIllumination(i)->getLayer(svs.staticLayerNumber))->reset(true,false); // don't compres lmaps(ugly 4x4 blocks on HD2400)
					}

					// reset cache, GL texture ids constant, but somehow rendered maps are not updated without display list rebuild
					if (res)
					{
						solver->resetRenderCache();
					}

					// save calculated lightmaps
					solver->getStaticObjects().saveLayer(svs.staticLayerNumber,LMAP_PREFIX,LMAP_POSTFIX);
				}
			}
			break;


		//////////////////////////////// RENDER ///////////////////////////////

		case ME_RENDER_DIFFUSE: svs.renderMaterialDiffuse = !svs.renderMaterialDiffuse; break;
		case ME_RENDER_SPECULAR: svs.renderMaterialSpecular = !svs.renderMaterialSpecular; break;
		case ME_RENDER_EMISSION: svs.renderMaterialEmission = !svs.renderMaterialEmission; break;
		case ME_RENDER_TRANSPARENT: svs.renderMaterialTransparency = !svs.renderMaterialTransparency; break;
		case ME_RENDER_WATER: svs.renderWater = !svs.renderWater; break;
		case ME_RENDER_TEXTURES: svs.renderMaterialTextures = !svs.renderMaterialTextures; break;
		case ME_RENDER_WIREFRAME: svs.renderWireframe = !svs.renderWireframe; break;
		case ME_RENDER_FPS: svs.renderFPS = !svs.renderFPS; break;
		case ME_RENDER_ICONS: svs.renderIcons = !svs.renderIcons; break;
		case ME_RENDER_HELPERS: svs.renderHelpers = !svs.renderHelpers; break;
		case ME_RENDER_LOGO: svs.renderLogo = !svs.renderLogo; break;
		case ME_RENDER_VIGNETTE: svs.renderVignette = !svs.renderVignette; break;
		case ME_RENDER_TONEMAPPING: svs.adjustTonemapping = !svs.adjustTonemapping; break;
		case ME_RENDER_BRIGHTNESS: getBrightness(this,svs.brightness); break;
		case ME_RENDER_CONTRAST: getFactor(this,svs.gamma,"Please adjust contrast (default is 1).","Contrast"); break;
		case ME_RENDER_WATER_LEVEL: getFactor(this,svs.waterLevel,"Please adjust water level in meters(scene units).","Water level"); break;


		///////////////////////////////// WINDOW ////////////////////////////////

		case ME_WINDOW_FULLSCREEN:
			svs.fullscreen = !svs.fullscreen;
			ShowFullScreen(svs.fullscreen,wxFULLSCREEN_ALL);
			GetPosition(windowCoord+0,windowCoord+1);
			GetSize(windowCoord+2,windowCoord+3);
			break;
		case ME_WINDOW_TREE:
			m_mgr.GetPane(wxT("scenetree")).Show(!m_sceneTree->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_PROPERTIES:
			m_mgr.GetPane(wxT("lightproperties")).Show(!m_lightProperties->IsShown());
			m_mgr.Update();
			break;


		//////////////////////////////// HELP ///////////////////////////////

		case ME_HELP: svs.renderHelp = !svs.renderHelp; break;
		case ME_CHECK_SOLVER: solver->checkConsistency(); break;
		case ME_CHECK_SCENE: solver->getMultiObjectCustom()->getCollider()->getMesh()->checkConsistency(); break;
		case ME_ABOUT:
			{
				wxIcon* icon = loadIcon(tmpstr("%s../maps/sv_logo.png",svs.pathToShaders));
				wxAboutDialogInfo info;
				if (icon) info.SetIcon(*icon);
				info.SetName("Lightsprint SDK");
				info.SetWebSite("http://lightsprint.com");
				info.SetCopyright("(c) 1999-2009 Stepan Hrbek, Lightsprint");
				wxAboutBox(info);
				delete icon;
			}
			break;
	}

	UpdateMenuBar();
}

void SVFrame::OnPaneOpenClose(wxAuiManagerEvent& event)
{
	// it's too soon to UpdateMenuBar(), pane that is closing is still open
	updateMenuBarNeeded = true;
}

void SVFrame::OnMouseMotion(wxMouseEvent& event)
{
	// doing this in OnMenuOpen would be too late, menu would open with old items
	// doing it here still fails if user closes pane by mouse and opens menu by keyboard without moving mouse
	if (updateMenuBarNeeded)
		UpdateMenuBar();
}

EntityId SVFrame::getSelectedEntity() const
{
	switch (m_canvas->selectedType)
	{
		case ST_LIGHT:
			return EntityId(m_canvas->selectedType,svs.selectedLightIndex);
		case ST_OBJECT:
			return EntityId(m_canvas->selectedType,svs.selectedObjectIndex);
		case ST_CAMERA:
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
			// show/hide light properties
			if (!m_lightProperties->IsShown() && (action==SEA_ACTION || (action==SEA_ACTION_IF_ALREADY_SELECTED && alreadySelected)))
			{
				m_mgr.GetPane(wxT("lightproperties")).Show();
				m_mgr.Update();
				UpdateMenuBar();
			}
			else
			if (m_lightProperties->IsShown() && alreadySelected && action!=SEA_SELECT)
			{
				m_mgr.GetPane(wxT("lightproperties")).Hide();
				m_mgr.Update();
				UpdateMenuBar();
			}

			// update light properties
			if (m_lightProperties->IsShown())
			{
				m_lightProperties->setLight(m_canvas->solver->realtimeLights[entity.index]);
			}

			m_canvas->selectedType = entity.type;
			svs.selectedLightIndex = entity.index;
			break;

		case ST_OBJECT:
			m_canvas->selectedType = entity.type;
			svs.selectedObjectIndex = entity.index;
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
		m_lightProperties->setLight(NULL);
	}
	else
	{
		m_lightProperties->setLight(m_canvas->solver->realtimeLights[svs.selectedLightIndex]);
	}

	// update scene tree
	m_sceneTree->updateContent(m_canvas->solver);
	m_sceneTree->selectItem(getSelectedEntity());
}

BEGIN_EVENT_TABLE(SVFrame, wxFrame)
	EVT_MENU(wxID_EXIT, SVFrame::OnExit)
	EVT_MENU(wxID_ANY, SVFrame::OnMenuEvent)
	EVT_AUI_PANE_RESTORE(SVFrame::OnPaneOpenClose)
	EVT_AUI_PANE_CLOSE(SVFrame::OnPaneOpenClose)
	EVT_MOTION(SVFrame::OnMouseMotion)
END_EVENT_TABLE()

}; // namespace

#endif // SUPPORT_SCENEVIEWER
