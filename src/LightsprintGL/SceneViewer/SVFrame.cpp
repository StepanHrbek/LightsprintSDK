// --------------------------------------------------------------------------
// Scene viewer - main window with menu.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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
#include "../Workaround.h"
#include "wx/aboutdlg.h"
#include "wx/regex.h"
#ifdef _WIN32
	#include <shlobj.h> // SHGetFolderPath, SHGetSpecialFolderPath
	#include <process.h> // _beginthread in AlphaSplashScreen
#endif
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

	#define LOG_CAPTION NULL
// naming convention for lightmaps and ldm. final name is prefix+objectnumber+postfix
#define LMAP_PREFIX  (const char*)(wxString(svs.sceneFilename).BeforeLast('.')+"_precalculated/")
#define LMAP_POSTFIX "lightmap.png"
#define LDM_PREFIX   LMAP_PREFIX
#define LDM_POSTFIX  "ldm.png"

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
		if (data)
		{
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
		}
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

static bool s_logIsOn = false;

class LogWithAbort
{
public:
	LogWithAbort(wxWindow* _window, RRDynamicSolverGL*& _solver)
	{
		enabled = !s_logIsOn; // do nothing if log is already enabled
		if (enabled)
		{
			// display log window with 'abort'
			s_logIsOn = true;
			window = _window;
			localReporter = rr::RRReporter::createWindowedReporter(*(rr::RRDynamicSolver**)&_solver,LOG_CAPTION);
		}
	}
	~LogWithAbort()
	{
		if (enabled)
		{
			// restore old reporter, close log
			s_logIsOn = false;
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
	choices.Add("10 - "+_("low"));
	choices.Add("40");
	choices.Add("100 - "+_("medium"));
	choices.Add("350");
	choices.Add("1000 - "+_("high"));
	choices.Add("3000");
	choices.Add("10000 - "+_("very high"));
	choices.Add("30000");
	choices.Add("100000");
	wxSingleChoiceDialog dialog(parent,title,_("Please select quality"),choices);
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
	if (offerPerVertex)	choices.Add(_("per-vertex"));
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
	wxSingleChoiceDialog dialog(parent,title,_("Resolution"),choices);
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
	wxString result = wxGetTextFromUser(title,_("Resolution"),wxString::Format("%dx%d",width,height),parent);
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
	bool firstUpdate = !m_canvas;

	// display log window with 'abort' while this function runs
	LogWithAbort logWithAbort(this,nextCanvas->solver);

	// stop showing properties of stuff we are going to delete
	m_objectProperties->setObject(NULL,0);
	m_lightProperties->setLight(NULL,0);
	m_materialProperties->setMaterial(NULL,0,rr::RRVec2(0));

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
	m_canvas = nextCanvas;


	UpdateMenuBar();

	// should go as late as possible, we don't want window displayed yet
	Show(true);

	// must go after Show() otherwise SetCurrent() in createContext() fails
	// loads scene if it is specified by filename
	m_canvas->createContext();

	// without SetFocus, keyboard events may be sent to frame instead of canvas
	m_canvas->SetFocus();

	if (svs.autodetectCamera && !(svs.initialInputSolver && svs.initialInputSolver->aborting)) OnMenuEventCore(ME_VIEW_RANDOM);

	UpdateTitle();
	m_sceneProperties->updateAfterGLInit();
	updateAllPanels();

	m_mgr.AddPane(m_canvas, wxAuiPaneInfo().Name("glcanvas").CenterPane().PaneBorder(false));
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
		OnMenuEventCore(ME_WINDOW_FULLSCREEN);
	if (!svs.fullscreen && userPreferences.windowLayout[userPreferences.currentWindowLayout].maximized != IsMaximized())
		Maximize(!IsMaximized());

	// remove captions from layout so that LoadPerspective does not interfere with language selection
	// wx must be patched according to http://trac.wxwidgets.org/ticket/12528
	wxString perspective = userPreferences.windowLayout[userPreferences.currentWindowLayout].perspective;
	wxRegEx("caption=[^;]*;").ReplaceAll(&perspective, wxEmptyString);
	userPreferences.windowLayout[userPreferences.currentWindowLayout].perspective = perspective;

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
	return new SVFrame(NULL, APP_NAME+" - "+_("loading"), wxPoint(x+2*border,y+border), wxSize(width-4*border,height-2*border), svs);
}

SVFrame::SVFrame(wxWindow* _parent, const wxString& _title, const wxPoint& _pos, const wxSize& _size, SceneViewerStateEx& _svs)
	: wxFrame(_parent, wxID_ANY, _title, _pos, _size, wxDEFAULT_FRAME_STYLE|wxMINIMIZE), svs(_svs)
{
	fullyInited = false;
	updateMenuBarNeeded = false;
	m_canvas = NULL;
	s_logIsOn = !svs.openLogWindows;

	// load preferences (must be done very early)
	bool layoutLoaded = userPreferences.load("");

	// create properties (based also on data from preferences)
	m_userProperties = new SVUserProperties(this);
	m_sceneProperties = new SVSceneProperties(this);
	m_lightProperties = new SVLightProperties(this);
	m_objectProperties = new SVObjectProperties(this);
	m_materialProperties = new SVMaterialProperties(this);
	m_sceneTree = new SVSceneTree(this);
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
	enableTooltips(userPreferences.tooltips);

	textureLocator = rr::RRFileLocator::create();

#if defined(_WIN32) && _MSC_VER>=1400
#ifdef NDEBUG
	double splashStartSec = GETSEC;
	AlphaSplashScreen splash(wxString::Format("%s../maps/sv_splash.png",svs.pathToShaders),230,-245);
#endif
#endif

	m_mgr.SetManagedWindow(this);

	UpdateEverything(); // slow. if specified by filename, loads scene from disk

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
	m_mgr.AddPane(m_sceneTree, wxAuiPaneInfo().Name("scenetree").Caption(_("Scene tree")).CloseButton(true).Left());
	m_mgr.AddPane(m_userProperties, wxAuiPaneInfo().Name("userproperties").Caption(_("User preferences")).CloseButton(true).Left());
	m_mgr.AddPane(m_sceneProperties, wxAuiPaneInfo().Name("sceneproperties").Caption(_("Scene properties")).CloseButton(true).Left());
	m_mgr.AddPane(m_lightProperties, wxAuiPaneInfo().Name("lightproperties").Caption(_("Light properties")).CloseButton(true).Right());
	m_mgr.AddPane(m_objectProperties, wxAuiPaneInfo().Name("objectproperties").Caption(_("Object properties")).CloseButton(true).Right());
	m_mgr.AddPane(m_materialProperties, wxAuiPaneInfo().Name("materialproperties").Caption(_("Material properties")).CloseButton(true).Right());
	// invisibly render first GL frame (it takes ages, all shaders are compiled, textures compressed etc)
	// here it does not work because window is minimized
	// it would work with restored or never minimized window, but
	//  - window 1x1 on screen is ugly
	//  - window moved outside screen is ok, not visible, but Centre() is later ignored if user moves other window meanwhile
	//wxPaintEvent e;
	//m_canvas->Paint(e);

	// render first visible frame, with good panels, disabled glcanvas
	m_canvas->renderEmptyFrames = true;
	Restore();

	// synchronize fullscreen state between 3 places
	// - userPreferences.windowLayout[userPreferences.currentWindowLayout].fullscreen
	//   - was just loaded from file or initiaized to default
	//     this is what we want to set
	// - svs.fullscreen
	//   - may be set by user who calls sceneViewer()
	//     we will ignore it
	// - actual state of wx
	//   - is windowed right now
	// state will be in sync from now on
	svs.fullscreen = false;
	userPreferencesApplyToWx();

	m_mgr.Update();
	fullyInited = true;
	m_canvas->renderEmptyFrames = false;
}

SVFrame::~SVFrame()
{
	if (fullyInited)
	{
		userPreferencesGatherFromWx();
		userPreferences.save();
	}
	m_mgr.UnInit();
	RR_SAFE_DELETE(textureLocator);
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
	if (rr::RRScene::getSupportedLoaderExtensions())
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_FILE_OPEN_SCENE,_("Open scene..."));
		winMenu->Append(ME_FILE_MERGE_SCENE,_("Merge scene..."),_("Merge automatically reduces lighting quality, click Global Illumination / Indirect: Firebal to restore it when you finish merging."));
		winMenu->Append(ME_FILE_SAVE_SCENE,_("Save scene"));
		winMenu->Append(ME_FILE_SAVE_SCENE_AS,_("Save scene as..."));
		winMenu->Append(ME_FILE_SAVE_SCREENSHOT,_("Save screenshot")+" (F8)",_("See options in user preferences."));
		winMenu->Append(ME_EXIT,_("Exit"));
		menuBar->Append(winMenu, _("File"));
	}

	// Global illumination...
	{
		winMenu = new wxMenu;
		winMenu->AppendRadioItem(ME_LIGHTING_DIRECT_REALTIME,_("Direct illumination: realtime"));
		winMenu->AppendRadioItem(ME_LIGHTING_DIRECT_STATIC,_("Direct illumination: static lightmap"));
		winMenu->AppendRadioItem(ME_LIGHTING_DIRECT_NONE,_("Direct illumination: none"));
		switch (svs.renderLightDirect)
		{
			case LD_REALTIME: winMenu->Check(ME_LIGHTING_DIRECT_REALTIME,true); break;
			case LD_STATIC_LIGHTMAPS: winMenu->Check(ME_LIGHTING_DIRECT_STATIC,true); break;
			case LD_NONE: winMenu->Check(ME_LIGHTING_DIRECT_NONE,true); break;
		}
		winMenu->AppendSeparator();
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_FIREBALL_LDM,_("Indirect illumination: realtime Fireball+LDM (fast+detailed)"),_("Changes lighting technique to Fireball with LDM, fast and detailed realtime GI that supports lights, emissive materials, skylight."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_FIREBALL,_("Indirect illumination: realtime Fireball (fast)"),_("Changes lighting technique to Fireball, fast realtime GI that supports lights, emissive materials, skylight."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_ARCHITECT,_("Indirect illumination: realtime Architect (no precalc)"),_("Changes lighting technique to Architect, legacy realtime GI that supports lights, emissive materials."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_STATIC,_("Indirect illumination: static lightmap"),_("Changes lighting technique to precomputed lightmaps. If you haven't built lightmaps yet, everything will be dark."));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_CONST,_("Indirect illumination: constant ambient"));
		winMenu->AppendRadioItem(ME_LIGHTING_INDIRECT_NONE,_("Indirect illumination: none"));
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
		winMenu->Append(ME_REALTIME_FIREBALL_BUILD,_("Build Fireball..."),_("(Re)builds Fireball, acceleration structure used by realtime GI."));
		winMenu->Append(ME_REALTIME_LDM_BUILD,_("Build LDM (light detail map)..."),_("(Re)builds LDM, structure that adds per-pixel details to realtime GI. Takes tens of minutes to build. LDM is efficient only with good unwrap in scene."));
		winMenu->AppendSeparator();
		winMenu->Append(ME_STATIC_BUILD_UNWRAP,_("Build unwrap..."),_("(Re)builds unwrap. Unwrap is necessary for lightmaps and LDM."));
		winMenu->Append(ME_STATIC_BUILD,_("Build lightmaps..."),_("(Re)builds per-vertex or per-pixel lightmaps. Per-pixel is efficient only with good unwrap in scene."));
		winMenu->Append(ME_STATIC_2D,_("Inspect unwrap+lightmaps in 2D"),_("Shows unwrap and lightmap in 2D."));
		winMenu->Append(ME_STATIC_BILINEAR,_("Toggle lightmap bilinear interpolation"));
		//winMenu->Append(ME_STATIC_BUILD_1OBJ,_("Build lightmap for selected obj, only direct..."),_("For testing only."));
#ifdef DEBUG_TEXEL
		winMenu->Append(ME_STATIC_DIAGNOSE,_("Diagnose texel..."),_("For debugging purposes, shows rays traced from texel in final gather step."));
#endif
		winMenu->AppendSeparator();
		winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_2D,_("Build 2d lightfield"),_("Lightfield is illumination captured in 3d, lightmap for freely moving dynamic objects. Not saved to disk, for testing only."));
		winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_3D,_("Build 3d lightfield"),_("Lightfield is illumination captured in 3d, lightmap for freely moving dynamic objects. Not saved to disk, for testing only."));
		menuBar->Append(winMenu, _("Global illumination"));
	}

	// Window...
	{
		winMenu = new wxMenu;
		winMenu->AppendCheckItem(ME_WINDOW_FULLSCREEN_META,_("Fullscreen")+" (F11)",_("Fullscreen mode uses full desktop resolution."));
		winMenu->Check(ME_WINDOW_FULLSCREEN_META,svs.fullscreen);
		winMenu->AppendCheckItem(ME_WINDOW_TREE,_("Scene tree"),_("Opens scene tree window."));
		winMenu->Check(ME_WINDOW_TREE,m_sceneTree->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_USER_PROPERTIES,_("User preferences"),_("Opens user preferences window."));
		winMenu->Check(ME_WINDOW_USER_PROPERTIES,m_userProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_SCENE_PROPERTIES,_("Scene properties"),_("Opens scene properties window."));
		winMenu->Check(ME_WINDOW_SCENE_PROPERTIES,m_sceneProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_LIGHT_PROPERTIES,_("Light properties"),_("Opens light properties window and starts rendering light icons."));
		winMenu->Check(ME_WINDOW_LIGHT_PROPERTIES,m_lightProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_OBJECT_PROPERTIES,_("Object properties"),_("Opens object properties window."));
		winMenu->Check(ME_WINDOW_OBJECT_PROPERTIES,m_objectProperties->IsShown());
		winMenu->AppendCheckItem(ME_WINDOW_MATERIAL_PROPERTIES,_("Material properties"),_("Opens material properties window."));
		winMenu->Check(ME_WINDOW_MATERIAL_PROPERTIES,m_materialProperties->IsShown());
		winMenu->AppendSeparator();
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT1,_("Workspace")+" 1 (alt-1)",_("Your custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT2,_("Workspace")+" 2 (alt-2)",_("Your custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT3,_("Workspace")+" 3 (alt-3)",_("Your custom window layout, changes automatically saved per user."));
		winMenu->Check(ME_WINDOW_LAYOUT1+userPreferences.currentWindowLayout,true);
		winMenu->AppendSeparator();
		winMenu->AppendCheckItem(ME_WINDOW_RESIZE,_("Set viewport size"),_("Lets you set exact viewport size in pixels."));
		menuBar->Append(winMenu, _("Windows"));
	}

	// About...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_HELP,_("Help - controls")+" (h)");
		winMenu->Append(ME_SDK_HELP,_("Lightsprint SDK manual"));
		winMenu->Append(ME_SUPPORT,_("Report bug, get support"));
		winMenu->Append(ME_CHECK_SOLVER,_("Log solver diagnose"),_("For diagnostic purposes."));
		winMenu->Append(ME_CHECK_SCENE,_("Log scene errors"),_("For diagnostic purposes."));
		winMenu->Append(ME_LIGHTSPRINT,_("Lightsprint web"));
		winMenu->Append(ME_ABOUT,_("About"));
		menuBar->Append(winMenu, _("Help"));
	}

	wxMenuBar* oldMenuBar = GetMenuBar();
	SetMenuBar(menuBar);
	delete oldMenuBar;
}

static wxString getSupportedLoaderExtensions(SceneViewerStateEx& svs)
{
	// wildcard format: "BMP and GIF files (*.bmp;*.gif)|*.bmp;*.gif|PNG files (*.png)|*.png"
	wxString extensions = rr::RRScene::getSupportedLoaderExtensions();
	wxString wxextensions = _("All scene formats")+"|"+extensions;
	while (!extensions.empty())
	{
		size_t i = extensions.find(';');
		wxString ext = (i==-1) ? extensions : extensions.substr(0,i);
		wxextensions += wxString("|")+ext+'|'+ext;
		extensions.erase(0,ext.size()+1);
	}
	return wxextensions;
}

static void incrementFilename(wxString& filename)
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
		filename[i] = ((unsigned)filename[i])+1;
		if (!(filename[i]>='a' && filename[i]<='z') && !(filename[i]>='A' && filename[i]<='Z') && !(filename[i]>='0' && filename[i]<='9') && filename[i]!='/' && filename[i]!='\\' && filename[i]!=':' && filename[i]!='.')
		{
			filename[i] = '0';
		}
		break;
	}
}

void SVFrame::OnMenuEvent(wxCommandEvent& event)
{
	OnMenuEventCore(event.GetId());
}

void SVFrame::OnMenuEventCore(unsigned eventCode)
{
#ifdef _WIN32
	__try
	{
		OnMenuEventCore2(eventCode);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		rr::RRReporter::report(rr::ERRO,"Processing event crashed.\n");
	}
#else
	OnMenuEventCore2(eventCode);
#endif
}

void SVFrame::OnMenuEventCore2(unsigned eventCode)
{
	if (m_canvas && m_canvas->solver)
		try
{
	rr_gl::RRDynamicSolverGL*& solver = m_canvas->solver;
	rr::RRLightField*& lightField = m_canvas->lightField;
	bool& fireballLoadAttempted = m_canvas->fireballLoadAttempted;
	int* windowCoord = m_canvas->windowCoord;
	bool& envToBeDeletedOnExit = m_canvas->envToBeDeletedOnExit;

	switch (eventCode)
	{

		//////////////////////////////// FILE ///////////////////////////////

		case ME_FILE_OPEN_SCENE:
			{
				wxFileDialog dialog(this,_("Choose a 3d scene to open"),"","",getSupportedLoaderExtensions(svs),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
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
				wxFileDialog dialog(this,_("Choose a 3d scene to merge with current scene"),"","",getSupportedLoaderExtensions(svs),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.sceneFilename);
				if (dialog.ShowModal()==wxID_OK)
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,m_canvas->solver);

					rr::RRScene* scene = loadScene(dialog.GetPath(),userPreferences.import.getUnitLength(dialog.GetPath()),userPreferences.import.getUpAxis(dialog.GetPath()));
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
				wxString extension = PATH2WX(bf::path(WX2PATH(svs.sceneFilename)).extension());
				wxString extensions = rr::RRScene::getSupportedSaverExtensions();
				bool extensionSupportsSave = !extension.empty() && extensions.find(extension)!=-1;
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
				if (!scene.save(svs.sceneFilename))
					wxMessageBox(_("Scene save failed."),_("Not saved."),wxOK|wxICON_ERROR);
				scene.environment = NULL; // would be deleted in destructor otherwise
			}
			break;
		case ME_FILE_SAVE_SCENE_AS:
			{
save_scene_as:
				// wildcard format: "BMP and GIF files (*.bmp;*.gif)|*.bmp;*.gif|PNG files (*.png)|*.png"
				wxString extensions = rr::RRScene::getSupportedSaverExtensions();
				if (extensions.empty())
				{
					wxMessageBox(_("Program built without saving."),_("No savers registered."),wxOK);
				}
				else
				{
					wxString wxextensions;
					while (!extensions.empty())
					{
						size_t i = extensions.find(';');
						wxString ext = (i==-1) ? extensions : extensions.substr(0,i);
						if (!wxextensions.empty())
							wxextensions += "|";
						wxextensions += ext+'|'+ext;
						extensions.erase(0,ext.size()+1);
					}

					// delete extension if it can't be saved, dialog will automatically append supported one
					wxString presetFilename = svs.sceneFilename;
					wxString::size_type lastDot = svs.sceneFilename.find_last_of(".");
					wxString extension = (lastDot==std::string::npos) ? presetFilename : presetFilename.substr(lastDot);
					wxString extensions = rr::RRScene::getSupportedSaverExtensions();
					bool extensionSupportsSave = !extension.empty() && extensions.find(extension)!=-1;
					if (!extensionSupportsSave) presetFilename = presetFilename.substr(0,presetFilename.size()-extension.size());

					wxFileDialog dialog(this,_("Save as"),"","",wxextensions,wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
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
			OnMenuEventCore(userPreferences.sshotEnhanced?SVFrame::ME_FILE_SAVE_SCREENSHOT_ENHANCED:SVFrame::ME_FILE_SAVE_SCREENSHOT_ORIGINAL);
			break;
		case ME_FILE_SAVE_SCREENSHOT_ORIGINAL:
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
					userPreferences.sshotFilename += wxString::Format("/screenshot%04d.jpg",(int)(t%10000));
				}

				// 9. save
				rr::RRBuffer::SaveParameters saveParameters;
				saveParameters.jpegQuality = 100;
				if (sshot->save(userPreferences.sshotFilename,NULL,&saveParameters))
					rr::RRReporter::report(rr::INF2,"Saved %s.\n",WX2CHAR(userPreferences.sshotFilename));
				else
					rr::RRReporter::report(rr::WARN,"Error: Failed to save %s.\n",WX2CHAR(userPreferences.sshotFilename));

				// 10. increment filename
				incrementFilename(userPreferences.sshotFilename);
				m_userProperties->updateProperties();

				// cleanup
				delete sshot;
			}
			break;
		case ME_FILE_SAVE_SCREENSHOT_ENHANCED:
			{
				rr::RRReportInterval report(rr::INF2,"Saving enhanced screenshot...\n");

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
					shadowRes[i] = rl->getRRLight().rtShadowmapSize;
					shadowSamples[i] = rl->getNumShadowSamples();
					rl->setShadowmapSize(shadowRes[i]*userPreferences.sshotEnhancedShadowResolutionFactor);
					if (userPreferences.sshotEnhancedShadowSamples)
						rl->setNumShadowSamples(userPreferences.sshotEnhancedShadowSamples);
				}

				// 2. alloc temporary textures
				rr::RRBuffer* bufColor = rr::RRBuffer::create(rr::BT_2D_TEXTURE,bigSize.x,bigSize.y,1,rr::BF_RGB,true,NULL);
				rr::RRBuffer* bufDepth = rr::RRBuffer::create(rr::BT_2D_TEXTURE,bigSize.x,bigSize.y,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
				Texture texColor(bufColor,false,false);
				Texture texDepth(bufDepth,false,false);
				bool srgb = Workaround::supportsSRGB() && svs.srgbCorrect;
				if (srgb)
				{
					// prepare framebuffer for sRGB correct rendering
					// (GL_FRAMEBUFFER_SRGB will be enabled inside Paint())
					texColor.reset(false,false,true);
				}

				// 3a. set new rendertarget
				FBO oldFBOState = FBO::getState();
				FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,&texDepth);
				FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,&texColor);
				if (!FBO::isOk())
				{
					wxMessageBox(_("Try lower resolution or disable FSAA (both in User preferences / Screenshot)."),_("Rendering screenshot failed."));
				}
				else
				{
					// 3b. propagate new size/aspect to renderer, correct screenCenter so that camera still points to the same area
					wxSize oldSize(m_canvas->winWidth,m_canvas->winHeight);
					Camera oldEye = svs.eye;
					svs.eye.setAspect(bigSize.x/(float)bigSize.y,(bigSize.x*m_canvas->winHeight>m_canvas->winWidth*bigSize.y)?1:0);
					svs.eye.screenCenter.x *= tan(oldEye.getFieldOfViewHorizontalRad()/2)/tan(svs.eye.getFieldOfViewHorizontalRad()/2);
					svs.eye.screenCenter.y *= tan(oldEye.getFieldOfViewVerticalRad()/2)/tan(svs.eye.getFieldOfViewVerticalRad()/2);
					m_canvas->winWidth = bigSize.x;
					m_canvas->winHeight = bigSize.y;
					glViewport(0,0,bigSize.x,bigSize.y);

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
						userPreferences.sshotFilename += wxString::Format("/screenshot%04d.jpg",(int)(t%10000));
					}

					// 9. save
					rr::RRBuffer::SaveParameters saveParameters;
					saveParameters.jpegQuality = 100;
					if (sshot->save(userPreferences.sshotFilename,NULL,&saveParameters))
						rr::RRReporter::report(rr::INF2,"Saved %s.\n",WX2CHAR(userPreferences.sshotFilename));
					else
						rr::RRReporter::report(rr::WARN,"Error: Failed to save %s.\n",WX2CHAR(userPreferences.sshotFilename));

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
					svs.eye = oldEye;
				}

				// 3a. cleanup
				oldFBOState.restore();
				m_canvas->OnSizeCore(); //glViewport(0,0,m_canvas->winWidth,m_canvas->winHeight);

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
				wxFileDialog dialog(this,_("Choose a skybox image"),"","","*.*",wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.skyboxFilename);
				if (dialog.ShowModal()!=wxID_OK)
					break;

				svs.skyboxFilename = dialog.GetPath();
			}
			goto reload_skybox;
		case ME_ENV_RELOAD: // not a menu item, just command we can call from outside
			{
reload_skybox:
				rr::RRBuffer* skybox = rr::RRBuffer::loadCube(svs.skyboxFilename,textureLocator);
				if (envToBeDeletedOnExit && solver->getEnvironment(0))
				{
					solver->getEnvironment(0)->stop();
					delete solver->getEnvironment(0); // env is refcounted and usually still exists after delete
				}
				if (skybox)
				{
					solver->setEnvironment(skybox,solver->getEnvironment(0));
					envToBeDeletedOnExit = true;
					m_canvas->timeWhenSkyboxBlendingStarted = GETSEC; // starts 3sec smooth transition in SVCanvas::Paint()
					if (svs.playVideos)
						skybox->play();
				}
				else
				{
					solver->setEnvironment(NULL,NULL);
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
			OnMenuEventCore(ME_LIGHTING_INDIRECT_FIREBALL);
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
				//OnMenuEventCore(ME_REALTIME_FIREBALL_BUILD);
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
					rr::RRDynamicSolver::FilteringParameters filtering = svs.lightmapFilteringParameters;
					filtering.smoothingAmount = 0;
					filtering.backgroundColor = rr::RRVec4(0.5f);
					solver->updateLightmaps(svs.ldmLayerNumber,-1,-1,&paramsDirect,&paramsIndirect,&filtering); 
					solver->setEnvironment(oldEnv);
					delete newEnv;

					// save ldm to disk
					solver->getStaticObjects().saveLayer(svs.ldmLayerNumber,LDM_PREFIX,LDM_POSTFIX);

					// switch to fireball+ldm
					OnMenuEventCore(ME_LIGHTING_INDIRECT_FIREBALL_LDM);
				}
			}
			break;
		case ME_REALTIME_FIREBALL_BUILD:
			{
				if (getQuality(_("Fireball build"),this,svs.fireballQuality))
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
				lightField->captureLighting(solver,0,false);
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
				lightField->captureLighting(solver,0,false);
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
					if (getQuality(_("Lightmap diagnose"),this,quality))
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
				if (getResolution(_("Unwrap build"),this,res,false))
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
				if (getResolution(_("Lightmap build"),this,res,true) && getQuality(_("Lightmap build"),this,quality))
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
								? rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGBA,true,NULL) // A in RGBA is only for debugging
								: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
						}
					}

					// calculate all
					solver->leaveFireball();
					fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(quality);
					solver->updateLightmaps(svs.staticLayerNumber,-1,-1,&params,&params,&svs.lightmapFilteringParameters);
					svs.renderLightDirect = LD_STATIC_LIGHTMAPS;
					svs.renderLightIndirect = LI_STATIC_LIGHTMAPS;
					// propagate computed data from buffers to textures
					for (unsigned i=0;i<solver->getStaticObjects().size();i++)
					{
						rr::RRBuffer* buf = solver->getStaticObjects()[i]->illumination.getLayer(svs.staticLayerNumber);
						if (buf && buf->getType()==rr::BT_2D_TEXTURE)
						{
							getTexture(buf,false,false); // don't mipmap lmaps(light would leak), don't compres lmaps(ugly 4x4 blocks on HD2400)
						}
					}

					// save calculated lightmaps
					solver->getStaticObjects().saveLayer(svs.staticLayerNumber,LMAP_PREFIX,LMAP_POSTFIX);
				}
			}
			break;


		///////////////////////////////// WINDOW ////////////////////////////////

		case ME_WINDOW_FULLSCREEN_META:
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
			m_mgr.GetPane("scenetree").Show(!m_sceneTree->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_USER_PROPERTIES:
			m_mgr.GetPane("userproperties").Show(!m_userProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_SCENE_PROPERTIES:
			m_mgr.GetPane("sceneproperties").Show(!m_sceneProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_LIGHT_PROPERTIES:
			m_mgr.GetPane("lightproperties").Show(!m_lightProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_OBJECT_PROPERTIES:
			m_mgr.GetPane("objectproperties").Show(!m_objectProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_MATERIAL_PROPERTIES:
			m_mgr.GetPane("materialproperties").Show(!m_materialProperties->IsShown());
			m_mgr.Update();
			break;
		case ME_WINDOW_LAYOUT1:
		case ME_WINDOW_LAYOUT2:
		case ME_WINDOW_LAYOUT3:
			userPreferencesGatherFromWx();
			userPreferences.currentWindowLayout = eventCode-ME_WINDOW_LAYOUT1;
			userPreferencesApplyToWx();
			break;
		case ME_WINDOW_RESIZE:
			{
				unsigned w = m_canvas->winWidth;
				unsigned h = m_canvas->winHeight;
				if (getResolution(_("3d viewport"),this,w,h) && (w!=m_canvas->winWidth || h!=m_canvas->winHeight))
				{
					if (svs.fullscreen)
					{
						OnMenuEventCore(ME_WINDOW_FULLSCREEN_META);
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
						wxMessageBox(panes?_("There's not enough space. You can make more space by resizing or closing panes."):_("There's not enough space. Switch to fullscreen mode for maximal resolution."));
					}
				}
			}
			break;


		//////////////////////////////// HELP ///////////////////////////////

		case ME_HELP: svs.renderHelp = !svs.renderHelp; break;
		case ME_SDK_HELP:
			ShellExecuteA(NULL,"open",wxString::Format("%s..\\..\\doc\\Lightsprint.chm",svs.pathToShaders),NULL,NULL,SW_SHOWNORMAL);
			break;
		case ME_SUPPORT:
			ShellExecuteA(NULL,"open",wxString::Format("mailto:support@lightsprint.com?Subject=Bug in build %s %s%d %d",__DATE__,
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
				wxIcon* icon = loadIcon(wxString::Format("%s../maps/sv_logo.png",svs.pathToShaders));
				wxAboutDialogInfo info;
				if (icon) info.SetIcon(*icon);
				info.SetName("Lightsprint SDK");
				info.SetCopyright("(c) 1999-2011 Stepan Hrbek, Lightsprint\n\n"+_("3rd party contributions - see Lightsprint SDK manual.")+"\n");
				info.SetWebSite("http://lightsprint.com");
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
			if (action==SEA_ACTION) OnMenuEventCore(ME_WINDOW_LIGHT_PROPERTIES);
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
			if (action==SEA_ACTION) OnMenuEventCore(ME_WINDOW_OBJECT_PROPERTIES);
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

rr::RRScene* SVFrame::loadScene(const char* _filename, float _units, unsigned _upAxis) const
{
	rr::RRScene* scene = new rr::RRScene(_filename,textureLocator,&m_canvas->solver->aborting);
	scene->normalizeUnits(_units);
	scene->normalizeUpAxis(_upAxis);
	scene->objects.flipFrontBack(3,true);
	return scene;
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
				lights[i]->direction = rr::RRVec3((rr::RRReal)(-sin(AZ)*sin(SZA)),(rr::RRReal)(-cosSZA),(rr::RRReal)(-cosAZ*sin(SZA))); // north = Z-, east = X-, up = Y+
				m_canvas->solver->realtimeLights[i]->updateAfterRRLightChanges();
				break;
			}
		}
	}
}

void SVFrame::enableTooltips(bool enable)
{
	m_lightProperties->enableTooltips(enable);
	m_objectProperties->enableTooltips(enable);
	m_materialProperties->enableTooltips(enable);
	m_sceneProperties->enableTooltips(enable);
	m_userProperties->enableTooltips(enable);
}

BEGIN_EVENT_TABLE(SVFrame, wxFrame)
	EVT_MENU(wxID_EXIT, SVFrame::OnExit)
	EVT_MENU(wxID_ANY, SVFrame::OnMenuEvent)
	EVT_AUI_PANE_RESTORE(SVFrame::OnPaneOpenClose)
	EVT_AUI_PANE_CLOSE(SVFrame::OnPaneOpenClose)
END_EVENT_TABLE()

}; // namespace

#endif // SUPPORT_SCENEVIEWER
