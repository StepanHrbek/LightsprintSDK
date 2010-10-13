// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVFrame.h"
#include "Lightsprint/RRScene.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/FBO.h"
#include "SVRayLog.h"
#include "SVSaveLoad.h"
#include "SVUserProperties.h"
#include "SVSceneProperties.h"
#include "SVLightProperties.h"
#include "SVObjectProperties.h"
#include "SVMaterialProperties.h"
#include "SVSceneTree.h"
#include "../tmpstr.h"
#include "wx/aboutdlg.h"
#ifdef _WIN32
	#include <shlobj.h> // SHGetSpecialFolderPath
	#include <process.h> // _beginthread in AlphaSplashScreen
#endif

	#define LOG_CAPTION NULL
// naming convention for lightmaps and ldm. final name is prefix+objectnumber+postfix
#define LMAP_PREFIX (wxString(svs.sceneFilename)+".").c_str()
#define LMAP_POSTFIX "lightmap.png"
#define LDM_PREFIX (wxString(svs.sceneFilename)+".").c_str()
#define LDM_POSTFIX "ldm.png"

namespace rr_gl
{


#if defined(_WIN32) && _MSC_VER>=1400
/////////////////////////////////////////////////////////////////////////////
//
// AlphaSplashScreen

bool g_alphaSplashOn = false;

class AlphaSplashScreen
{
public:
	AlphaSplashScreen(const char* filename, int dx=0, int dy=0)
	{
		// load image
		hWnd = 0;
		rr::RRBuffer* buffer = rr::RRBuffer::load(filename);
		if (!buffer)
			return;

		// register class
		WNDCLASSEX wcex;
		wcex.cbSize         = sizeof(WNDCLASSEX);
		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= DefWindowProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= NULL;
		wcex.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= TEXT("Splash");
		wcex.hIconSm		= LoadIcon(NULL, IDI_APPLICATION);
		RegisterClassEx(&wcex);

		// create window
		RECT workArea;
		SystemParametersInfo(SPI_GETWORKAREA,0,&workArea,0);
		int x = workArea.left + RR_MAX(0,workArea.right-workArea.left-buffer->getWidth())/2 + dx;
		int y = workArea.top + RR_MAX(0,workArea.bottom-workArea.top-buffer->getHeight())/2 + dy;
		hWnd = CreateWindowEx(WS_EX_LAYERED|WS_EX_TOPMOST,TEXT("Splash"),TEXT("Splash"),WS_POPUPWINDOW|WS_VISIBLE,x,y,buffer->getWidth(),buffer->getHeight(),NULL,NULL,NULL,NULL);
		if (!hWnd)
		{
			RR_ASSERT(0);
			delete buffer;
			return;
		}

		// copy image to windows
		HDC hdcScreen = GetDC(NULL);
		HDC hdcBackBuffer = CreateCompatibleDC(hdcScreen);
		HBITMAP hbmBackBuffer = CreateCompatibleBitmap(hdcScreen, buffer->getWidth(), buffer->getHeight());
		HGDIOBJ hbmOld = SelectObject(hdcBackBuffer, hbmBackBuffer);
		BITMAPINFO bitmapinfo;
		memset(&bitmapinfo,0,sizeof(bitmapinfo));
		bitmapinfo.bmiHeader.biSize = sizeof(bitmapinfo.bmiHeader);
		bitmapinfo.bmiHeader.biWidth = buffer->getWidth();
		bitmapinfo.bmiHeader.biHeight = buffer->getHeight();
		bitmapinfo.bmiHeader.biPlanes = 1;
		bitmapinfo.bmiHeader.biBitCount = 32;
		bitmapinfo.bmiHeader.biCompression = BI_RGB;
		buffer->setFormat(rr::BF_RGBA);
		unsigned char* data = buffer->lock(rr::BL_READ_WRITE);
		for (unsigned i=0;i<buffer->getBufferBytes();i+=4)
		{
			// swap r,b, premultiply by a
			unsigned char r = data[i];
			unsigned char g = data[i+1];
			unsigned char b = data[i+2];
			unsigned char a = data[i+3];
			data[i] = b*a/255;
			data[i+1] = g*a/255;
			data[i+2] = r*a/255;
			data[i+3] = a;
		}
		SetDIBitsToDevice(hdcBackBuffer,0,0,buffer->getWidth(),buffer->getHeight(),0,0,0,buffer->getHeight(),data,&bitmapinfo,0);
		buffer->unlock();
		delete buffer;

		// start rendering splash
		POINT ptSrc;
		ptSrc.x = 0;
		ptSrc.y = 0;
		SIZE size;
		size.cx = buffer->getWidth();
		size.cy = buffer->getHeight();
		BLENDFUNCTION bf;
		bf.AlphaFormat = AC_SRC_ALPHA;
		bf.SourceConstantAlpha = 255;
		bf.BlendFlags = 0;
		bf.BlendOp = AC_SRC_OVER;
		UpdateLayeredWindow(hWnd, NULL, NULL, &size, hdcBackBuffer, &ptSrc, 0, &bf, ULW_ALPHA);
		SelectObject(hdcBackBuffer, hbmOld);
		DeleteDC(hdcBackBuffer);
		_beginthread(windowThreadFunc,0,NULL);
		g_alphaSplashOn = true;
	}

	~AlphaSplashScreen()
	{
		DestroyWindow(hWnd);
		g_alphaSplashOn = false;
	}
private:
	static void windowThreadFunc(void* instanceData)
	{
		MSG msg;
		while (GetMessage(&msg,NULL,0,0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	HWND hWnd;
};
#endif // _WIN32


/////////////////////////////////////////////////////////////////////////////
//
// LogWithAbort

bool g_logIsOn = false;

class LogWithAbort
{
public:
	LogWithAbort(wxWindow* _window, RRDynamicSolverGL*& _solver)
	{
		enabled = !g_logIsOn; // do nothing if log is already enabled
		if (enabled)
		{
			// display log window with 'abort'
			g_logIsOn = true;
			window = _window;
			localReporter = rr::RRReporter::createWindowedReporter(*(rr::RRDynamicSolver**)&_solver,LOG_CAPTION);
			oldReporter = rr::RRReporter::getReporter();
			rr::RRReporter::setReporter(localReporter);
		}
	}
	~LogWithAbort()
	{
		if (enabled)
		{
			// restore old reporter, close log
			g_logIsOn = false;
			rr::RRReporter::setReporter(oldReporter);
			delete localReporter;
			// When windowed reporter shuts down, z-order changes (why?), SV drops below toolbench.
			// This bring SV back to front.
			window->Raise();
		}
	}
private:
	bool enabled;
	wxWindow* window;
	rr::RRReporter* localReporter;
	rr::RRReporter* oldReporter;
};


/////////////////////////////////////////////////////////////////////////////
//
// UI dialogs

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
	wxSingleChoiceDialog dialog(parent,title,"Resolution",choices);
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
// Incoming resolution is taken as default value.
static bool getResolution(wxString title, wxWindow* parent, unsigned& width, unsigned& height)
{
	wxString result = wxGetTextFromUser(title,"Resolution",tmpstr("%dx%d",width,height),parent);
	unsigned x = result.find('x');
	if (x>0)
	{
		unsigned long w,h;
		if (result.Left(x).ToULong(&w) && result.Right(result.size()-x-1).ToULong(&h))
		{
			width = w;
			height = h;
			return true;
		}
	}
	return false;
}

// true = valid answer
// false = dialog was escaped
bool getFactor(wxWindow* parent, float& factor, const wxString& message, const wxString& caption)
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


/////////////////////////////////////////////////////////////////////////////
//
// SVFrame

	#define APP_NAME wxString("SceneViewer ")
	#define APP_VERSION_TITLE __DATE__

void SVFrame::UpdateTitle()
{
	if (!svs.sceneFilename.empty())
		SetTitle(APP_NAME+APP_VERSION_TITLE+" - "+svs.sceneFilename);
	else
		SetTitle(APP_NAME+APP_VERSION_TITLE);
}

void SVFrame::UpdateEverything()
{
	SVCanvas* nextCanvas = new SVCanvas( svs, this, wxDefaultSize);

	// display log window with 'abort' while this function runs
	LogWithAbort logWithAbort(this,nextCanvas->solver);

	bool oldReleaseResources = svs.releaseResources;
	svs.releaseResources = true; // we are not returning yet, we should shutdown
	if (m_canvas) m_mgr.DetachPane(m_canvas);
	RR_SAFE_DELETE(m_canvas);
	svs.releaseResources = oldReleaseResources;

	// initialInputSolver may be changed only if canvas is NULL
	// we NULL it to avoid rendering solver contents again (new scene was opened)
	// it has also minor drawback: initialInputSolver->abort will be ignored
	if (m_canvas) svs.initialInputSolver = NULL;

	// creates canvas
	m_canvas = nextCanvas;


	UpdateMenuBar();

	// should go as late as possible, we don't want window displayed yet
	Show(true);

	// must go after Show() otherwise SetCurrent() in createContext() fails
	// loads scene if it is specified by filename
	m_canvas->createContext();

	// without SetFocus, keyboard events may be sent to frame instead of canvas
	m_canvas->SetFocus();

	if (svs.autodetectCamera && !(svs.initialInputSolver && svs.initialInputSolver->aborting)) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_VIEW_RANDOM));

	UpdateTitle();

	updateAllPanels();

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
	if (!svs.fullscreen && userPreferences.windowLayout[userPreferences.currentWindowLayout].maximized != IsMaximized())
		Maximize(!IsMaximized());
	m_mgr.LoadPerspective(userPreferences.windowLayout[userPreferences.currentWindowLayout].perspective,true);
	UpdateMenuBar();
}

SVFrame* SVFrame::Create(SceneViewerStateEx& svs)
{
	// workaround for http://trac.wxwidgets.org/ticket/11787
	// fixed after 2.9.1, in 65666 (small detail finished in 65787)
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
	bool layoutLoaded = userPreferences.load(); // must be loaded before SVUserProperties is created
	m_userProperties = new SVUserProperties(this);
	m_sceneProperties = new SVSceneProperties(this);
	m_lightProperties = new SVLightProperties(this);
	m_objectProperties = new SVObjectProperties(this);
	m_materialProperties = new SVMaterialProperties(this);
	m_sceneTree = new SVSceneTree(this);
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

#if defined(_WIN32) && _MSC_VER>=1400
#ifdef NDEBUG
	double splashStartSec = GETSEC;
	AlphaSplashScreen splash(tmpstr("%s../maps/sv_splash.png",svs.pathToShaders),230,-245);
#endif
#endif

	m_mgr.SetManagedWindow(this);

	UpdateEverything(); // slow. if specified by filename, loads scene from disk
	if (svs.fullscreen) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_FULLSCREEN));

	// create layouts if load failed
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
	m_mgr.AddPane(m_userProperties, wxAuiPaneInfo().Name(wxT("userproperties")).Caption(wxT("User preferences")).CloseButton(true).Left());
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
	{
		userPreferencesGatherFromWx();
		userPreferences.save();
	}
	m_mgr.UnInit();
}


void SVFrame::OnExit(wxCommandEvent& event)
{
	// true is to force the frame to close
	Close(true);
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
		winMenu->Append(ME_FILE_MERGE_SCENE,_T("Merge scene..."),_T("Merge automatically reduces lighting quality, click Global Illumination / Indirect: Firebal to restore it when you finish merging."));
		winMenu->Append(ME_FILE_SAVE_SCENE,_T("Save scene"));
		winMenu->Append(ME_FILE_SAVE_SCENE_AS,_T("Save scene as..."));
		winMenu->Append(ME_FILE_SAVE_SCREENSHOT,_T("Save screenshot (F9)"),_T("See options in user preferences."));
		winMenu->Append(ME_EXIT,_T("Exit"));
		menuBar->Append(winMenu, _T("File"));
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
		winMenu->Append(ME_REALTIME_FIREBALL_BUILD,_T("Build Fireball..."),_T("(Re)builds Fireball, acceleration structure used by realtime GI. Scene properties / GI quality / Fireball quality is ") wxSTRINGIZE_T(svs.fireballQuality) _T("."));
		winMenu->Append(ME_REALTIME_LDM_BUILD,_T("Build LDM (light detail map)..."),_T("(Re)builds LDM, structure that adds per-pixel details to realtime GI. Takes tens of minutes to build. LDM is efficient only with good unwrap in scene."));
		winMenu->AppendSeparator();
		winMenu->Append(ME_STATIC_BUILD_UNWRAP,_T("Build unwrap..."),_T("(Re)builds unwrap. Unwrap is necessary for lightmaps and LDM."));
		winMenu->Append(ME_STATIC_BUILD,_T("Build lightmaps..."),_T("(Re)builds per-vertex or per-pixel lightmaps. Per-pixel is efficient only with good unwrap in scene."));
		winMenu->Append(ME_STATIC_2D,_T("Inspect unwrap+lightmaps in 2D"),_T("Shows unwrap and lightmap in 2D."));
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
		winMenu->AppendCheckItem(ME_WINDOW_USER_PROPERTIES,_T("User preferences"),_T("Opens user preferences window."));
		winMenu->Check(ME_WINDOW_USER_PROPERTIES,m_userProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_SCENE_PROPERTIES,_T("Scene properties"),_T("Opens scene properties window."));
		winMenu->Check(ME_WINDOW_SCENE_PROPERTIES,m_sceneProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_LIGHT_PROPERTIES,_T("Light properties"),_T("Opens light properties window and starts rendering light icons."));
		winMenu->Check(ME_WINDOW_LIGHT_PROPERTIES,m_lightProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_OBJECT_PROPERTIES,_T("Object properties"),_T("Opens object properties window."));
		winMenu->Check(ME_WINDOW_OBJECT_PROPERTIES,m_objectProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_MATERIAL_PROPERTIES,_T("Material properties"),_T("Opens material properties window."));
		winMenu->Check(ME_WINDOW_MATERIAL_PROPERTIES,m_materialProperties->IsShown());
		winMenu->AppendSeparator();
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT1,_T("Workspace 1 (alt-1)"),_T("Your custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT2,_T("Workspace 2 (alt-2)"),_T("Your custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT3,_T("Workspace 3 (alt-3)"),_T("Your custom window layout, changes automatically saved per user."));
		winMenu->Check(ME_WINDOW_LAYOUT1+userPreferences.currentWindowLayout,true);
		winMenu->AppendSeparator();
		winMenu->AppendCheckItem(ME_WINDOW_RESIZE,_T("Set viewport size"),_T("Lets you set exact viewport size in pixels."));
		menuBar->Append(winMenu, _T("Windows"));
	}

	// About...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_HELP,_T("Help - controls (h)"));
		winMenu->Append(ME_SDK_HELP,_T("Lightsprint SDK manual"));
		winMenu->Append(ME_SUPPORT,_T("Report bug, get support"));
		winMenu->Append(ME_CHECK_SOLVER,_T("Log solver diagnose"),_T("For diagnostic purposes."));
		winMenu->Append(ME_CHECK_SCENE,_T("Log scene errors"),_T("For diagnostic purposes."));
		winMenu->Append(ME_LIGHTSPRINT,_T("Lightsprint web"));
		winMenu->Append(ME_ABOUT,_T("About"));
		menuBar->Append(winMenu, _T("Help"));
	}

	wxMenuBar* oldMenuBar = GetMenuBar();
	SetMenuBar(menuBar);
	delete oldMenuBar;
}

static std::string getSupportedLoaderExtensions()
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
	return wxextensions;
}

static void incrementFilename(std::string& filename)
{
	size_t i = filename.find_last_of('.');
	while (i>0)
	{
		i--;
		if (filename[i]=='/' || filename[i]=='\\' || filename[i]==':' || filename[i]=='.')
		{
			break;
		}
		if (filename[i]=='9')
		{
			filename[i] = '0';
			continue;
		}
		if (filename[i]=='Z')
		{
			filename[i] = 'A';
			continue;
		}
		if (filename[i]=='z')
		{
			filename[i] = 'a';
			continue;
		}
		filename[i]++;
		if (!(filename[i]>='a' && filename[i]<='z') && !(filename[i]>='A' && filename[i]<='Z') && !(filename[i]>='0' && filename[i]<='9') && filename[i]!='/' && filename[i]!='\\' && filename[i]!=':' && filename[i]!='.')
		{
			filename[i] = '0';
		}
		break;
	}
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
	if (m_canvas && m_canvas->solver)
		try
{
	rr_gl::RRDynamicSolverGL*& solver = m_canvas->solver;
	rr::RRLightField*& lightField = m_canvas->lightField;
	bool& fireballLoadAttempted = m_canvas->fireballLoadAttempted;
	int* windowCoord = m_canvas->windowCoord;
	bool& envToBeDeletedOnExit = m_canvas->envToBeDeletedOnExit;

	switch (event.GetId())
	{

		//////////////////////////////// FILE ///////////////////////////////

		case ME_FILE_OPEN_SCENE:
			{
				wxFileDialog dialog(this,"Choose a 3d scene to open","","",getSupportedLoaderExtensions().c_str(),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.sceneFilename);
				if (dialog.ShowModal()==wxID_OK)
				{
					svs.initialInputSolver = NULL;
					svs.sceneFilename = dialog.GetPath();
					UpdateEverything();
				}
			}
			break;
		case ME_FILE_MERGE_SCENE:
			{
				wxFileDialog dialog(this,"Choose a 3d scene to merge with current scene","","",getSupportedLoaderExtensions().c_str(),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.sceneFilename);
				if (dialog.ShowModal()==wxID_OK)
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,m_canvas->solver);

					rr::RRScene* scene = new rr::RRScene(dialog.GetPath(),&solver->aborting);
					scene->normalizeUnits(userPreferences.import.getUnitLength(dialog.GetPath()));
					scene->normalizeUpAxis(userPreferences.import.getUpAxis(dialog.GetPath()));
					m_canvas->addOrRemoveScene(scene,true);
					m_canvas->mergedScenes.push_back(scene);
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
						UpdateTitle();
						updateSceneTree();
						goto save_scene;
					}
				}
			}
			break;
		case ME_FILE_SAVE_SCREENSHOT:
			{
				if (!userPreferences.sshotEnhanced)
				{
					// Grab content of backbuffer to sshot.
					wxSize size = m_canvas->GetSize();
					rr::RRBuffer* sshot = rr::RRBuffer::create(rr::BT_2D_TEXTURE,size.x,size.y,1,rr::BF_RGB,true,NULL);
					unsigned char* pixels = sshot->lock(rr::BL_DISCARD_AND_WRITE);
					glReadBuffer(GL_BACK);
					glReadPixels(0,0,size.x,size.y,GL_RGB,GL_UNSIGNED_BYTE,pixels);
					sshot->unlock();

					// 8. fix empty filename
					if (userPreferences.sshotFilename.empty())
					{
						char screenshotFilename[1000]=".";
#ifdef _WIN32
						SHGetSpecialFolderPathA(NULL, screenshotFilename, CSIDL_DESKTOP, FALSE); // CSIDL_PERSONAL
#endif
						userPreferences.sshotFilename = screenshotFilename;
						time_t t = time(NULL);
						userPreferences.sshotFilename += tmpstr("/screenshot%04d.jpg",t%10000);
					}

					// 9. save
					rr::RRBuffer::SaveParameters saveParameters;
					saveParameters.jpegQuality = 100;
					if (sshot->save(userPreferences.sshotFilename.c_str(),NULL,&saveParameters))
						rr::RRReporter::report(rr::INF2,"Saved %s.\n",userPreferences.sshotFilename.c_str());
					else
						rr::RRReporter::report(rr::WARN,"Error: Failed to save %s.\n",userPreferences.sshotFilename.c_str());

					// 10. increment filename
					incrementFilename(userPreferences.sshotFilename);
					m_userProperties->updateProperties();

					// cleanup
					delete sshot;
				}
				else
				{
					// 1a. enable FSAA
					// (1*1*FSAA is ugly, worse than plain capture)
					// (2*2*FSAA works fine, but it's too much for Quadro)
					// (3*3*FSAA works fine too, 3*2560<8k, but it's probably too much for older cards)
					// (4*2560 would definitely exceed Radeon 4xxx limits)
					wxSize smallSize(userPreferences.sshotEnhancedWidth,userPreferences.sshotEnhancedHeight);
					const int AA = (unsigned)sqrtf((float)userPreferences.sshotEnhancedFSAA);
					wxSize bigSize = AA*smallSize;
					
					// 1b. enhance shadows
					rr::RRVector<RealtimeLight*>& lights = m_canvas->solver->realtimeLights;
					unsigned* shadowSamples = new unsigned[lights.size()];
					unsigned* shadowRes = new unsigned[lights.size()];
					for (unsigned i=0;i<lights.size();i++)
					{
						RealtimeLight* rl = lights[i];
						shadowRes[i] = rl->getShadowmapSize();
						shadowSamples[i] = rl->getNumShadowSamples();
						rl->setShadowmapSize(shadowRes[i]*userPreferences.sshotEnhancedShadowResolutionFactor);
						if (userPreferences.sshotEnhancedShadowSamples)
							rl->setNumShadowSamples(userPreferences.sshotEnhancedShadowSamples);
					}
					rr::RRDynamicSolver::CalculateParameters params;
					params.qualityIndirectStatic = 0; // set it to update only shadows
					params.qualityIndirectDynamic = 0;
					m_canvas->solver->calculate(&params); // renders into FBO, must go before FBO::setRenderTarget()

					// 2. alloc temporary textures
					rr::RRBuffer* bufColor = rr::RRBuffer::create(rr::BT_2D_TEXTURE,bigSize.x,bigSize.y,1,rr::BF_RGB,true,NULL);
					rr::RRBuffer* bufDepth = rr::RRBuffer::create(rr::BT_2D_TEXTURE,bigSize.x,bigSize.y,1,rr::BF_DEPTH,true,(unsigned char*)1);
					Texture texColor(bufColor,false,false);
					Texture texDepth(bufDepth,false,false);

					// 3a. set new rendertarget
					FBO oldFBOState = FBO::getState();
					FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,&texDepth);
					FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,&texColor);
					if (!FBO::isOk())
					{
						wxMessageBox("Try lower resolution or disable FSAA (both in User preferences / Screenshot).","Rendering screenshot failed.");
					}
					else
					{
						// 3b. propagate new size/aspect to renderer
						wxSize oldSize(m_canvas->winWidth,m_canvas->winHeight);
						//rr::RRVec2 oldCenter = svs.eye.screenCenter;
						//svs.eye.screenCenter = rr::RRVec2(0);
						m_canvas->winWidth = bigSize.x;
						m_canvas->winHeight = bigSize.y;
						glViewport(0,0,bigSize.x,bigSize.x);

						// 4. disable automatic tonemapping, uses FBO, would not work
						bool oldTonemapping = svs.tonemappingAutomatic;
						svs.tonemappingAutomatic = false;

						// 6. render to texColor
						wxPaintEvent e;
						m_canvas->Paint(e);

						// 7. downscale to sshot
						rr::RRBuffer* sshot = rr::RRBuffer::create(rr::BT_2D_TEXTURE,smallSize.x,smallSize.y,1,rr::BF_RGB,true,NULL);
						unsigned char* pixelsBig = bufColor->lock(rr::BL_DISCARD_AND_WRITE);
						glPixelStorei(GL_PACK_ALIGNMENT,1);
						glReadPixels(0,0,bigSize.x,bigSize.y,GL_RGB,GL_UNSIGNED_BYTE,pixelsBig);
						unsigned char* pixelsSmall = sshot->lock(rr::BL_DISCARD_AND_WRITE);
						for (int j=0;j<smallSize.y;j++)
							for (int i=0;i<smallSize.x;i++)
								for (unsigned k=0;k<3;k++)
								{
									unsigned a = 0;
									for (int y=0;y<AA;y++)
										for (int x=0;x<AA;x++)
											a += pixelsBig[k+3*(AA*i+x+AA*smallSize.x*(AA*j+y))];
									pixelsSmall[k+3*(i+smallSize.x*j)] = (a+AA*AA/2)/(AA*AA);
								}
						sshot->unlock();
						bufColor->unlock();

						// 8. fix empty filename
						if (userPreferences.sshotFilename.empty())
						{
							char screenshotFilename[1000]=".";
#ifdef _WIN32
							SHGetSpecialFolderPathA(NULL, screenshotFilename, CSIDL_DESKTOP, FALSE); // CSIDL_PERSONAL
#endif
							userPreferences.sshotFilename = screenshotFilename;
							time_t t = time(NULL);
							userPreferences.sshotFilename += tmpstr("/screenshot%04d.jpg",t%10000);
						}

						// 9. save
						rr::RRBuffer::SaveParameters saveParameters;
						saveParameters.jpegQuality = 100;
						if (sshot->save(userPreferences.sshotFilename.c_str(),NULL,&saveParameters))
							rr::RRReporter::report(rr::INF2,"Saved %s.\n",userPreferences.sshotFilename.c_str());
						else
							rr::RRReporter::report(rr::WARN,"Error: Failed to save %s.\n",userPreferences.sshotFilename.c_str());

						// 10. increment filename
						incrementFilename(userPreferences.sshotFilename);
						m_userProperties->updateProperties();

						// 7. cleanup
						delete sshot;


						// 4. cleanup
						svs.tonemappingAutomatic = oldTonemapping;

						// 3b. cleanup
						m_canvas->winWidth = oldSize.x;
						m_canvas->winHeight = oldSize.y;
						//svs.eye.screenCenter = oldCenter;
					}

					// 3a. cleanup
					oldFBOState.restore();
					glViewport(0,0,m_canvas->winWidth,m_canvas->winHeight);

					// 2. cleanup
					delete bufDepth;
					delete bufColor;

					// 1b. cleanup
					for (unsigned i=0;i<lights.size();i++)
					{
						RealtimeLight* rl = lights[i];
						rl->setShadowmapSize(shadowRes[i]);
						rl->setNumShadowSamples(shadowSamples[i]);
					}
					delete[] shadowSamples;
					delete[] shadowRes;
				}
			}
			break;
		case ME_EXIT:
			Close();
			break;


		//////////////////////////////// VIEW ///////////////////////////////

		case ME_VIEW_TOP:    svs.eye.setView(Camera::TOP   ,solver?solver->getMultiObjectCustom():NULL); break;
		case ME_VIEW_BOTTOM: svs.eye.setView(Camera::BOTTOM,solver?solver->getMultiObjectCustom():NULL); break;
		case ME_VIEW_LEFT:   svs.eye.setView(Camera::LEFT  ,solver?solver->getMultiObjectCustom():NULL); break;
		case ME_VIEW_RIGHT:  svs.eye.setView(Camera::RIGHT ,solver?solver->getMultiObjectCustom():NULL); break;
		case ME_VIEW_FRONT:  svs.eye.setView(Camera::FRONT ,solver?solver->getMultiObjectCustom():NULL); break;
		case ME_VIEW_BACK:   svs.eye.setView(Camera::BACK  ,solver?solver->getMultiObjectCustom():NULL); break;
		case ME_VIEW_RANDOM: svs.eye.setView(Camera::RANDOM,solver?solver->getMultiObjectCustom():NULL); svs.cameraMetersPerSecond = svs.eye.getFar()*0.08f; break;


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
						delete solver->getEnvironment(0);
					solver->setEnvironment(skybox,solver->getEnvironment(0));
					envToBeDeletedOnExit = true;
					m_canvas->timeWhenSkyboxBlendingStarted = GETSEC; // starts 3sec smooth transition in SVCanvas::Paint()
					if (svs.playVideos)
						skybox->play();
				}
			}
			break;
			
		//////////////////////////////// LIGHTS ///////////////////////////////

		case ME_LIGHT_SPOT: m_sceneTree->runContextMenuAction(CM_LIGHT_SPOT,EntityId(ST_LIGHT,0)); break;
		case ME_LIGHT_POINT: m_sceneTree->runContextMenuAction(CM_LIGHT_POINT,EntityId(ST_LIGHT,0)); break;
		case ME_LIGHT_DIR: m_sceneTree->runContextMenuAction(CM_LIGHT_DIR,EntityId(ST_LIGHT,0)); break;
		case ME_LIGHT_FLASH: m_sceneTree->runContextMenuAction(CM_LIGHT_FLASH,EntityId(ST_LIGHT,0)); break;
		case ME_LIGHT_DELETE: m_sceneTree->runContextMenuAction(CM_LIGHT_DELETE,EntityId(ST_LIGHT,svs.selectedLightIndex)); break;


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
			solver->reportDirectIlluminationChange(-1,true,true);
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
				// display log window with 'abort' while this function runs
				// don't display it if it is already displayed (we may be automatically called when scene loads and fireball is requested, log is already on)
				LogWithAbort logWithAbort(this,solver);

				// ask no questions, it's possible scene is loading right now and it's not safe to render/idle. dialog would render/idle on background
				solver->buildFireball(svs.fireballQuality,NULL);
				solver->reportDirectIlluminationChange(-1,true,true);
				// this would ask questions
				//OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,SVFrame::ME_REALTIME_FIREBALL_BUILD));
			}
			break;

		case ME_LIGHTING_INDIRECT_ARCHITECT:
			svs.renderLightIndirect = LI_REALTIME_ARCHITECT;
			if (svs.renderLightDirect==LD_STATIC_LIGHTMAPS) // direct must not stay lightmaps
				svs.renderLightDirect = LD_REALTIME;
			svs.renderLightmaps2d = 0;
			solver->reportDirectIlluminationChange(-1,true,true);
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
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver);

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
				if (getQuality("Fireball build",this,svs.fireballQuality))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver);

					svs.renderLightDirect = LD_REALTIME;
					svs.renderLightIndirect = LI_REALTIME_FIREBALL;
					svs.renderLightmaps2d = 0;
					solver->buildFireball(svs.fireballQuality,NULL);
					solver->reportDirectIlluminationChange(-1,true,true);
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
		case ME_STATIC_BUILD_UNWRAP:
			{
				unsigned res = 256;
				if (getResolution("Unwrap build",this,res,false))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver);

					solver->getStaticObjects().buildUnwrap(res,true,solver->aborting);
					// static objects may be modified even after abort (unwrap is not atomic)
					// so it's better if following setStaticObjects is not aborted
					solver->aborting = false;

					// buildUnwrap usually splits a few vertices, we have to send modified data to solver
					// solver would crash if we add/remove triangles/vertices silently
					solver->setStaticObjects(solver->getStaticObjects(),NULL);

					// fix dangling pointer in collisionHandler
					delete m_canvas->collisionHandler;
					m_canvas->collisionHandler = solver->getMultiObjectCustom()->createCollisionHandlerFirstVisible();

					// resize rtgi buffers, vertex counts may differ
					m_canvas->reallocateBuffersForRealtimeGI(true);
				}
			}
			break;

		case ME_STATIC_BUILD:
			{
				unsigned res = 256; // 0=vertex buffers
				unsigned quality = 100;
				if (getResolution("Lightmap build",this,res,true) && getQuality("Lightmap build",this,quality))
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver);

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
		case ME_WINDOW_USER_PROPERTIES:
			m_mgr.GetPane(wxT("userproperties")).Show(!m_userProperties->IsShown());
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
		case ME_WINDOW_RESIZE:
			{
				unsigned w = m_canvas->winWidth;
				unsigned h = m_canvas->winHeight;
				if (getResolution("3d viewport",this,w,h) && (w!=m_canvas->winWidth || h!=m_canvas->winHeight))
				{
					if (svs.fullscreen)
					{
						OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_FULLSCREEN));
					}
					if (IsMaximized())
					{
						Restore();
					}

					// don't touch window, panes
					//m_canvas->SetSize(w,h);
					// resize window, don't touch panes
					wxSize windowSize = GetClientSize();
					SetClientSize(windowSize.x+w-m_canvas->winWidth,windowSize.y+h-m_canvas->winHeight);
					if (m_canvas->winWidth!=w || m_canvas->winHeight!=h)
					{
						bool panes = m_sceneTree->IsShown() || m_userProperties->IsShown() || m_sceneProperties->IsShown() || m_lightProperties->IsShown() || m_objectProperties->IsShown() || m_materialProperties->IsShown();
						wxMessageBox(panes?"There's not enough space. You can make more space by resizing or closing panes.":"There's not enough space. Switch to fullscreen mode for maximal resolution.");
					}
				}
			}
			break;


		//////////////////////////////// HELP ///////////////////////////////

		case ME_HELP: svs.renderHelp = !svs.renderHelp; break;
		case ME_SDK_HELP:
			ShellExecuteA(NULL,"open",tmpstr("%s..\\..\\doc\\Lightsprint.chm",svs.pathToShaders),NULL,NULL,SW_SHOWNORMAL);
			break;
		case ME_SUPPORT:
			ShellExecuteA(NULL,"open",tmpstr("mailto:support@lightsprint.com?Subject=Bug in build %s %s%d %d",__DATE__,
	#if defined(_WIN32)
				"win",
	#else
				"linux",
	#endif
	#if defined(_M_X64) || defined(_LP64)
				64,
	#else
				32,
	#endif
	#if defined(_MSC_VER)
				_MSC_VER
	#else
				0
	#endif
				),NULL,NULL,SW_SHOWNORMAL);
			break;
		case ME_LIGHTSPRINT:
			ShellExecuteA(NULL,"open","http://lightsprint.com",NULL,NULL,SW_SHOWNORMAL);
			break;
		case ME_CHECK_SOLVER:
			solver->checkConsistency();
			break;
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

void SVFrame::selectEntityInTreeAndUpdatePanel(EntityId entity, SelectEntityAction action)
{
	bool alreadySelected = entity==getSelectedEntity();
	switch (entity.type)
	{
		case ST_LIGHT:
			// update light properties
			m_lightProperties->setLight(m_canvas->solver->realtimeLights[entity.index],svs.precision);
			m_canvas->selectedType = entity.type;
			svs.selectedLightIndex = entity.index;
			if (action==SEA_ACTION) OnMenuEvent(wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED,ME_WINDOW_LIGHT_PROPERTIES));
			break;

		case ST_STATIC_OBJECT:
		case ST_DYNAMIC_OBJECT:
			// update object properties
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

		default:
			// When user selects tree item with entity.type that has no function for us (e.g "0 lights" with type ST_FIRST),
			// we must still accept it and save it to selectedType. Otherwise Delete key would delete previously selected item,
			// according to old selectedType.
			m_canvas->selectedType = entity.type;
	}

	if (m_sceneTree)
	{
		m_sceneTree->selectEntityInTree(entity);
	}
}

// Ensures that index is in 0..size-1 range.
// If it can't be satisfied, sets it to 0 and returns false.
static bool validateIndex(unsigned& index, unsigned size)
{
	if (index>=size)
	{
		if (size)
			index = size-1;
		else
		{
			index = 0;
			return false;
		}
	}
	return true;
}

// Ensure that all svs.selectedXxxIndex are in range. Update all panels. (ok, not all, but the rest probably doesn't matter)
void SVFrame::updateAllPanels()
{
	if (m_canvas->solver)
	{
		// update selected light
		if (!validateIndex(svs.selectedLightIndex,m_canvas->solver->getLights().size()))
		{
			m_lightProperties->setLight(NULL,svs.precision);
			if (m_canvas->selectedType==ST_LIGHT)
				m_canvas->selectedType = ST_CAMERA;
		}
		else
		{
			m_lightProperties->setLight(m_canvas->solver->realtimeLights[svs.selectedLightIndex],svs.precision);
		}

		// update selected object
		if (!validateIndex(svs.selectedObjectIndex,m_canvas->solver->getStaticObjects().size()))
		{
			m_objectProperties->setObject(NULL,svs.precision);
		}
		else
		{
			m_objectProperties->setObject(m_canvas->solver->getStaticObjects()[svs.selectedObjectIndex],svs.precision);
		}
	}


	updateSceneTree();
}

void SVFrame::updateSceneTree()
{
	m_sceneTree->updateContent(m_canvas->solver);
}

void SVFrame::commitPropertyChanges()
{
	m_lightProperties->CommitChangesFromEditor();
	m_sceneProperties->CommitChangesFromEditor();
	m_objectProperties->CommitChangesFromEditor();
	m_materialProperties->CommitChangesFromEditor();
}

void SVFrame::simulateSun()
{
	if (svs.envSimulateSun && m_canvas->solver)
	{
		const rr::RRLights& lights = m_canvas->solver->getLights();
		for (unsigned i=0;i<lights.size();i++)
		{
			if (lights[i] && lights[i]->type==rr::RRLight::DIRECTIONAL)
			{
				double longitudeDeg = svs.envLongitudeDeg;
				double latitudeRad = RR_DEG2RAD(svs.envLatitudeDeg);
				double hour = svs.envDateTime.tm_hour+svs.envDateTime.tm_min/60.;
				unsigned daysBeforeMonth[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
				unsigned dayInYear = daysBeforeMonth[svs.envDateTime.tm_mon]+svs.envDateTime.tm_mday-1; // svs.envDateTime.tm_yday is not set, calculate it here
				double g = (M_PI*2/365.25)*(dayInYear + hour/24); // fractional year, rad
				double D = RR_DEG2RAD(0.396372-22.91327*cos(g)+4.02543*sin(g)-0.387205*cos(2*g)+0.051967*sin(2*g)-0.154527*cos(3*g) + 0.084798*sin(3*g)); // sun declination, rad
				double TC = 0.004297+0.107029*cos(g)-1.837877*sin(g)-0.837378*cos(2*g)-2.340475*sin(2*g); // time correction for solar angle, deg
				double SHA = RR_DEG2RAD((hour-12)*15 + TC); //+ longitudeDeg); // solar hour angle, rad. longitude correction is not necessary because hour is local time
				double cosSZA = sin(latitudeRad)*sin(D)+cos(latitudeRad)*cos(D)*cos(SHA); // sun zenith angle, cos
				RR_CLAMP(cosSZA,-1,1);
				double SZA = acos(cosSZA); // sun zenith angle, rad
				double cosAZ = -(sin(D)-sin(latitudeRad)*cosSZA)/(cos(latitudeRad)*sin(SZA)); // azimuth angle, cos
				RR_CLAMP(cosAZ,-1,1);
				double AZ = acos(cosAZ); // azimuth angle, rad
				if (SHA>0) AZ = -AZ;
				lights[i]->direction = rr::RRVec3((rr::RRReal)(sin(AZ)*sin(SZA)),(rr::RRReal)(-cosSZA),(rr::RRReal)(cosAZ*sin(SZA))); // north = Z+, east = X+, up = Y+
				m_canvas->solver->realtimeLights[i]->updateAfterRRLightChanges();
				break;
			}
		}
	}
}

BEGIN_EVENT_TABLE(SVFrame, wxFrame)
	EVT_MENU(wxID_EXIT, SVFrame::OnExit)
	EVT_MENU(wxID_ANY, SVFrame::OnMenuEvent)
	EVT_AUI_PANE_RESTORE(SVFrame::OnPaneOpenClose)
	EVT_AUI_PANE_CLOSE(SVFrame::OnPaneOpenClose)
END_EVENT_TABLE()

}; // namespace

#endif // SUPPORT_SCENEVIEWER
