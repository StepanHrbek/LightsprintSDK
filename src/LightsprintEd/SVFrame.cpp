// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - main window with menu and AUI based panels.
// --------------------------------------------------------------------------

#include "SVFrame.h"
#include "Lightsprint/RRScene.h"
#include "Lightsprint/GL/RRSolverGL.h"
#include "Lightsprint/GL/FBO.h"
#include "SVRayLog.h"
#include "SVUserProperties.h"
#include "SVSceneProperties.h"
#include "SVGIProperties.h"
#include "SVLightProperties.h"
#include "SVObjectProperties.h"
#include "SVMaterialProperties.h"
#include "SVSceneTree.h"
#include "SVLog.h"
#include "wx/aboutdlg.h"
#include "wx/display.h"
#include "wx/splash.h"
#ifdef _WIN32
	#include <shlobj.h> // SHGetSpecialFolderPath
	#include <process.h> // _beginthread in AlphaSplashScreen
#endif
#ifdef __WXMAC__
	#include <ApplicationServices/ApplicationServices.h> // TransformProcessType
#endif
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

namespace rr_ed
{


// reimplemention of rr_gl's internal Workaround::supportsSRGB()
bool supportsSRGB()
{
	return true
		&& (GLEW_EXT_framebuffer_sRGB || GLEW_ARB_framebuffer_sRGB || GLEW_VERSION_3_0) // added in GL 3.0
		&& (GLEW_EXT_texture_sRGB || GLEW_VERSION_2_1); // added in GL 2.1
}

static wxImage* loadImage(const wxString& filename)
{
	// Q: why do we read images via RRBuffer::load()?
	// A: wx file loaders are not compiled in, to reduce size
	//    wxIcon(fname.ico) works only in Windows, only on some icons and it reduces icon resolution
	//    wxBitmap(fname.bmp) works only in Windows and ignores alphachannel
	rr::RRBuffer* buffer = rr::RRBuffer::load(RR_WX2RR(filename));
	if (!buffer)
		return nullptr;
	buffer->flip(false,true,false);
	unsigned width = buffer->getWidth();
	unsigned height = buffer->getHeight();
	// filling wxImage per pixel rather than passing whole buffer to constructor is necessary with buggy wxWidgets 2.8.9
	wxImage* image = new wxImage(width,height,false);
	image->InitAlpha();
	for (unsigned j=0;j<height;j++)
		for (unsigned i=0;i<width;i++)
		{
			rr::RRVec4 element = buffer->getElement(j*width+i,nullptr);
			image->SetRGB(i,j,(unsigned)(element[0]*255),(unsigned)(element[1]*255),(unsigned)(element[2]*255));
			image->SetAlpha(i,j,(unsigned)(element[3]*255));
		}
	delete buffer;
	return image;
}

static wxIcon* loadIcon(const wxString& filename)
{
	wxImage* image = loadImage(filename);
	if (!image)
		return nullptr;
	wxBitmap bitmap(*image);
	wxIcon* icon = new wxIcon();
	icon->CopyFromBitmap(bitmap);
	delete image;
	return icon;
}

/////////////////////////////////////////////////////////////////////////////
//
// AlphaSplashScreen

bool g_alphaSplashOn = false;

#if defined(_WIN32) && _MSC_VER>=1400

class AlphaSplashScreen
{
public:
	AlphaSplashScreen(const wxString& filename, bool evenIfAplhaIgnored, int dx=0, int dy=0)
	{
		// load image
		hWnd = 0;
		rr::RRBuffer* buffer = rr::RRBuffer::load(RR_WX2RR(filename));
		if (!buffer)
			return;

		// register class
		WNDCLASSEX wcex;
		wcex.cbSize         = sizeof(WNDCLASSEX);
		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= DefWindowProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= nullptr;
		wcex.hIcon			= LoadIcon(nullptr, IDI_APPLICATION);
		wcex.hCursor		= LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
		wcex.lpszMenuName	= nullptr;
		wcex.lpszClassName	= TEXT("Splash");
		wcex.hIconSm		= LoadIcon(nullptr, IDI_APPLICATION);
		RegisterClassEx(&wcex);

		// create window
		RECT workArea;
		SystemParametersInfo(SPI_GETWORKAREA,0,&workArea,0);
		int x = workArea.left + RR_MAX(0,workArea.right-workArea.left-buffer->getWidth())/2 + dx;
		int y = workArea.top + RR_MAX(0,workArea.bottom-workArea.top-buffer->getHeight())/2 + dy;
		hWnd = CreateWindowEx(WS_EX_LAYERED|WS_EX_TOPMOST,TEXT("Splash"),TEXT("Splash"),WS_POPUPWINDOW|WS_VISIBLE,x,y,buffer->getWidth(),buffer->getHeight(),nullptr,nullptr,nullptr,nullptr);
		if (!hWnd)
		{
			RR_ASSERT(0);
			delete buffer;
			return;
		}

		// copy image to windows
		HDC hdcScreen = GetDC(nullptr);
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
		UpdateLayeredWindow(hWnd, nullptr, nullptr, &size, hdcBackBuffer, &ptSrc, 0, &bf, ULW_ALPHA);
		SelectObject(hdcBackBuffer, hbmOld);
		DeleteDC(hdcBackBuffer);
		_beginthread(windowThreadFunc,0,nullptr);
		g_alphaSplashOn = true;

		delete buffer;
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
		while (GetMessage(&msg,nullptr,0,0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	HWND hWnd;
};

#else // ! (defined(_WIN32) && _MSC_VER>=1400)

class AlphaSplashScreen
{
public:
	AlphaSplashScreen(const wxString& filename, bool evenIfAlphaIgnored, int dx=0, int dy=0)
	{
		splash = nullptr;
		if (!evenIfAlphaIgnored)
			return;
		wxImage* image = loadImage(filename);
		if (!image)
			return;
		wxBitmap bitmap(*image);
		splash = new wxSplashScreen(bitmap,wxSPLASH_CENTRE_ON_SCREEN|wxSPLASH_TIMEOUT,600000,nullptr,-1,wxDefaultPosition,wxDefaultSize,wxNO_BORDER|wxSTAY_ON_TOP);
	}
	~AlphaSplashScreen()
	{
		delete splash;
		g_alphaSplashOn = false;
	}
private:
	wxSplashScreen* splash;
};

#endif // ! (defined(_WIN32) && _MSC_VER>=1400)


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
bool getQuality(wxString title, wxWindow* parent, unsigned& quality)
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
bool getResolution(wxString title, wxWindow* parent, unsigned& resolution, bool offerPerVertex)
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
	sprintf(value,"%g",factor);
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

bool getMaterial(wxString title, wxWindow* parent, const rr::RRMaterials& materials, unsigned& materialIndex)
{
	wxArrayString choices;
	for (unsigned i=0;i<materials.size();i++)
		choices.Add(RR_RR2WX(materials[i]->name));
	wxSingleChoiceDialog dialog(parent,title,_("Please select material"),choices);
	dialog.SetSelection(materialIndex);
	if (dialog.ShowModal()==wxID_OK)
	{
		materialIndex = dialog.GetSelection();
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
	// stop processing events, if any come. probably not necessary, just being cautious
	if (m_canvas)
		m_canvas->fullyCreated = false;

	SVCanvas* nextCanvas = new SVCanvas(svs,this);
	bool firstUpdate = !m_canvas;

	// display log window with 'abort' while this function runs
	LogWithAbort logWithAbort(this,nextCanvas->solver,_("Loading scene..."));

	// stop showing properties of stuff we are going to delete
	m_objectProperties->setObject(nullptr,0);
	m_lightProperties->setLight(nullptr,0);
	m_materialProperties->setMaterial(nullptr,nullptr,0,rr::RRVec2(0));

	bool oldReleaseResources = svs.releaseResources;
	svs.releaseResources = true; // we are not returning yet, we should shutdown
	m_canvasWindow->svcanvas = nextCanvas;
	{
		rr::RRReportInterval report(rr::INF2,"Cleaning up...\n"); // temporary, we had crash somewhere around here, this will help locate it
		rr::RR_SAFE_DELETE(m_canvas);
	}
	svs.releaseResources = oldReleaseResources;

	// initialInputSolver may be changed only if canvas is nullptr
	// we nullptr it to avoid rendering solver contents again (new scene was opened)
	// it has also minor drawback: initialInputSolver->abort will be ignored
	if (!firstUpdate) svs.initialInputSolver = nullptr;

	// creates canvas
	m_canvas = nextCanvas;


	UpdateMenuBar();

	// should go as late as possible, we don't want window displayed yet
	// has to be called, otherwise m_isShown stays false -> SVSceneProperties::OnIdle IS_SHOWN is false -> automatic tonemapping never updates
	Show(true);

	// must go after Show() otherwise SetCurrent() in createContext() fails
	// loads scene if it is specified by filename
	m_canvas->createContext(); // contains fullyCreated = true; from now on, events are processed

	// without SetFocus, keyboard events may be sent to frame instead of canvas
	m_canvasWindow->SetFocus();

	if (!(svs.initialInputSolver && svs.initialInputSolver->aborting))
	{
		rr::RRCamera temp = svs.camera;
		OnMenuEventCore(ME_VIEW_RANDOM);
		if (!svs.autodetectCamera) // restore camera, keep only recalculated cameraMetersPerSecond
			svs.camera = temp;
	}


	UpdateTitle();
	m_giProperties->updateAfterGLInit();
	updateAllPanels();

	// start playing videos
	m_canvas->configureVideoPlayback(svs.playVideos);

	OnAnyChange(ES_MISC,nullptr,nullptr,0);

	m_canvas->renderEmptyFrames = 1;
}


void SVFrame::userPreferencesGatherFromWx()
{
	userPreferences.windowLayout[userPreferences.currentWindowLayout].fullscreen = svs.fullscreen;
	userPreferences.windowLayout[userPreferences.currentWindowLayout].maximized = IsMaximized();
	if (!svs.fullscreen && !IsMaximized())
		userPreferences.windowLayout[userPreferences.currentWindowLayout].rectangle = GetRect();
	userPreferences.windowLayout[userPreferences.currentWindowLayout].perspective = m_mgr.SavePerspective();
}

void SVFrame::userPreferencesApplyToWx()
{
	UserPreferences::WindowLayout& layout = userPreferences.windowLayout[userPreferences.currentWindowLayout];

	if (layout.fullscreen != svs.fullscreen)
	{
		OnMenuEventCore(ME_WINDOW_FULLSCREEN);
		RR_ASSERT(&layout==&userPreferences.windowLayout[userPreferences.currentWindowLayout]); // make sure that userPreferences.currentWindowLayout did not change
	}
	if (!svs.fullscreen && layout.maximized != IsMaximized())
		Maximize(!IsMaximized());
	if (!svs.fullscreen && !IsMaximized())
	{
		wxRect& rect = layout.rectangle;
		if (rect.IsEmpty())
			rect = GetRect();
		SetSize(rect);
	}
	m_mgr.LoadPerspective(layout.perspective,true);

	// if language selection did change, pane captions must be updated
	m_mgr.GetPane(m_lightProperties).Caption(_("Light"));
	m_mgr.GetPane(m_materialProperties).Caption(_("Material"));
	m_mgr.GetPane(m_giProperties).Caption(_("Global Illumination"));
	m_mgr.GetPane(m_objectProperties).Caption(_("Object"));
	m_mgr.GetPane(m_log).Caption(_("Log"));
	m_mgr.GetPane(m_sceneTree).Caption(_("Scene tree"));
	m_mgr.GetPane(m_userProperties).Caption(_("User preferences"));
	m_mgr.GetPane(m_sceneProperties).Caption(_("Scene properties"));

	UpdateMenuBar();
}


SVFrame* SVFrame::Create(SceneViewerStateEx& svs)
{
	// workaround for http://trac.wxwidgets.org/ticket/11787
	// fixed after 2.9.1, in 65666 (small detail finished in 65787)
	// solves crash in second Toolbench Relighting 
	wxPGEditor_TextCtrl = nullptr;
	wxPGEditor_Choice = nullptr;
	wxPGEditor_ComboBox = nullptr;
	wxPGEditor_TextCtrlAndButton = nullptr;
	wxPGEditor_CheckBox = nullptr;
	wxPGEditor_ChoiceAndButton = nullptr;
//	wxPGEditor_SpinCtrl = nullptr;
//	wxPGEditor_DatePickerCtrl = nullptr;


	// open at ~50% of screen size
	int x,y,width,height;
	::wxClientDisplayRect(&x,&y,&width,&height);
	const int border = (width+height)/25;
	return new SVFrame(nullptr, APP_NAME+" - "+_("loading"), wxPoint(x+2*border,y+border), wxSize(width-4*border,height-2*border), svs);
}

SVFrame::SVFrame(wxWindow* _parent, const wxString& _title, const wxPoint& _pos, const wxSize& _size, SceneViewerStateEx& _svs)
	: wxFrame(_parent, wxID_ANY, _title, _pos, _size, wxDEFAULT_FRAME_STYLE|wxMINIMIZE), svs(_svs), smoothDlg(this), importDlg(this), deleteDlg(this), bakeDlg(this), unwrapDlg(this)
{
	fullyInited = false;
	updateMenuBarNeeded = false;
	m_canvas = nullptr;
	m_canvasWindow = nullptr;
	LogWithAbort::logIsOn = !svs.openLogWindows;

	// zero at least the most important variables, before starting dangerous work
	m_userProperties = nullptr;
	m_sceneProperties = nullptr;
	m_giProperties = nullptr;
	m_lightProperties = nullptr;
	m_objectProperties = nullptr;
	m_materialProperties = nullptr;
	m_sceneTree = nullptr;

#ifdef NDEBUG
	rr::RRTime splashStart;
	AlphaSplashScreen splash(svs.pathToMaps+"sv_splash.png",false,230,-245);
#endif // NDEBUG

	// load preferences (must be done very early)
	userPreferences.load("");

#ifdef SUPPORT_OCULUS
	{
		rr::RRReportInterval report(rr::INF2,"Checking Oculus Rift...\n");
		ovrResult err = ovrHmd_Create(0,&oculusHMD);
	}
#endif // SUPPORT_OCULUS

	// create properties (based also on data from preferences)
	m_canvasWindow = new CanvasWindow(this);
	m_userProperties = new SVUserProperties(this);
	m_sceneProperties = new SVSceneProperties(this);
	m_giProperties = new SVGIProperties(this);
	m_lightProperties = new SVLightProperties(this);
	m_objectProperties = new SVObjectProperties(this);
	m_materialProperties = new SVMaterialProperties(this);
	m_sceneTree = new SVSceneTree(this);
	m_log = new SVLog(this);

	static const char* sample_xpm[] = {
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
	rr_gl::Program::logMessages(userPreferences.testingLogShaders);
	bool f1,f3;
	unsigned f2;
	rr::RRReporter::getFilter(f1,f2,f3);
	rr::RRReporter::setFilter(true,userPreferences.testingLogMore?3:RR_MAX(f2,2),true);

	textureLocator = rr::RRFileLocator::create();
	textureLocator->setAttempt(rr::RRFileLocator::ATTEMPT_STUB,RR_WX2RR(svs.pathToMaps+"sv_missing.png"));

	m_mgr.SetManagedWindow(this);

	UpdateEverything(); // slow. if specified by filename, loads scene from disk. creates context

	// setup dock art (colors etc)
	wxAuiDockArt* dockArt = new wxAuiDefaultDockArt;
	//dockArt->SetMetric(wxAUI_DOCKART_SASH_SIZE,4);
	wxColour sash = wxColour(190,190,190);
	dockArt->SetColor(wxAUI_DOCKART_SASH_COLOUR,sash);
	SetBackgroundColour(sash); // sash gets this color in osx

	//dockArt->SetMetric(wxAUI_DOCKART_GRIPPER_SIZE,0);
	//dockArt->SetColor(wxAUI_DOCKART_GRIPPER_COLOUR,wxColour(0,0,0));

	dockArt->SetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE,0);
	dockArt->SetColor(wxAUI_DOCKART_BORDER_COLOUR,wxColour(0,0,0));

	//dockArt->SetMetric(wxAUI_DOCKART_PANE_BUTTON_SIZE,20);
	//dockArt->SetColor(wxAUI_DOCKART_BACKGROUND_COLOUR,wxColour(250,0,0));

	//dockArt->SetColor(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR,wxColour(235,235,255));
	//dockArt->SetColor(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR,wxColour(140,140,160));
	//dockArt->SetColor(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR,wxColour(0,0,0));
	dockArt->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR,sash);//wxColour(225,225,225));
	//dockArt->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR,wxColour(255,255,255));
	dockArt->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR,wxColour(0,0,0));
	dockArt->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE,wxAUI_GRADIENT_NONE);

	dockArt->SetMetric(wxAUI_DOCKART_CAPTION_SIZE,30);
	static wxFont font(13,wxFONTFAMILY_SWISS,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD);
	dockArt->SetFont(wxAUI_DOCKART_CAPTION_FONT,font);
	m_mgr.SetArtProvider(dockArt);

	// create panes
	m_mgr.AddPane(m_canvasWindow, wxAuiPaneInfo().Name("glcanvas").CenterPane().PaneBorder(false));
	m_mgr.AddPane(m_sceneTree, wxAuiPaneInfo().Name("scenetree").CloseButton(true).Left());
	m_mgr.AddPane(m_userProperties, wxAuiPaneInfo().Name("userproperties").CloseButton(true).Left());
	m_mgr.AddPane(m_sceneProperties, wxAuiPaneInfo().Name("sceneproperties").CloseButton(true).Left());
	m_mgr.AddPane(m_giProperties, wxAuiPaneInfo().Name("giproperties").CloseButton(true).Left());
	m_mgr.AddPane(m_lightProperties, wxAuiPaneInfo().Name("lightproperties").CloseButton(true).Right());
	m_mgr.AddPane(m_objectProperties, wxAuiPaneInfo().Name("objectproperties").CloseButton(true).Right());
	m_mgr.AddPane(m_materialProperties, wxAuiPaneInfo().Name("materialproperties").CloseButton(true).Right());
	m_mgr.AddPane(m_log, wxAuiPaneInfo().Name("log").CloseButton(true).Bottom());

	// invisibly render first GL frame (it takes ages, all shaders are compiled, textures compressed etc)
	// here it does not work because window is minimized
	// it would work with restored or never minimized window, but
	//  - window 1x1 on screen is ugly
	//  - window moved outside screen is ok, not visible, but Centre() is later ignored if user moves other window meanwhile
	//m_canvas->Paint(false);

	// render first visible frame, with good panels, disabled glcanvas
	// window was created with wxMINIMIZE, this makes it visible
	Restore();

	// 1. setup frame size+location. it would be nice to do this before we make window visible, but SetSize does not work on minimized window
	// 2. synchronize fullscreen state between 3 places
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

	// SetFocus in UpdateEverything() is not sufficient, adding panes changes focus, so here we set it again
	m_canvasWindow->SetFocus();

	m_mgr.Update();

	// switch to normal rendering (with enabled glcanvas)
	// still, do at least 1 empty frame to quickly clear canvas (important in windows classic mode)
	fullyInited = true;
	m_canvas->renderEmptyFrames = 1;
}

SVFrame::~SVFrame()
{
	rr::RRReporter::report(rr::INF2,"~SVFrame()\n");
	if (fullyInited)
	{
		{
			userPreferencesGatherFromWx();
			userPreferences.save();
		}
	}
	m_mgr.UnInit();
	if (fullyInited)
	{
#ifdef SUPPORT_OCULUS
		if (oculusHMD)
			ovrHmd_Destroy(oculusHMD);
#endif
		rr::RR_SAFE_DELETE(textureLocator);
	}
}


void SVFrame::OnExit(wxCommandEvent& event)
{
	rr::RRReporter::report(rr::INF2,"OnExit()\n");
	// true is to force the frame to close
	Close(true);
}

void SVFrame::UpdateMenuBar()
{
	if (svs.fullscreen) return; // menu in fullscreen is disabled
#ifdef __WXMAC__
	static bool macInited = false;
	if (!macInited)
	{
		macInited = true;
		// makes menu visible in OSX
		//  other option is to build application as bundle
		ProcessSerialNumber PSN;
		GetCurrentProcess(&PSN);
		TransformProcessType(&PSN,kProcessTransformToForegroundApplication);
		// populates default OSX menu
		wxApp::s_macAboutMenuItemId = ME_ABOUT;
		//wxApp::s_macPreferencesMenuItemId = ME_;
		wxApp::s_macExitMenuItemId = ME_EXIT;
		wxApp::s_macHelpMenuTitleName = _("Help");
	}
#endif
	updateMenuBarNeeded = false;
	wxMenuBar *menuBar = new wxMenuBar;
	wxMenu *winMenu = nullptr;

	// File...
	if (rr::RRScene::getSupportedLoaderExtensions())
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_FILE_OPEN_SCENE,_("Open scene..."));
		winMenu->Append(ME_FILE_MERGE_SCENE,_("Merge scene..."),_("Merge automatically reduces lighting quality, click Global Illumination / Indirect: Firebal to restore it when you finish merging."));
		winMenu->Append(ME_FILE_SAVE_SCENE,_("Save scene"));
		winMenu->Append(ME_FILE_SAVE_SCENE_AS,_("Save scene as..."));
		winMenu->Append(ME_FILE_SAVE_SCREENSHOT,_("Save screenshot")+" (F9)",_("See options in user preferences."));
		winMenu->Append(ME_EXIT,_("Exit"));
		menuBar->Append(winMenu, _("File"));
	}

	/*
	// Global illumination...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_REALTIME_FIREBALL_BUILD,_("Build Fireball..."),_("(Re)builds Fireball, acceleration structure used by realtime GI."));
#ifdef DEBUG_TEXEL
		winMenu->Append(ME_STATIC_DIAGNOSE,_("Diagnose texel..."),_("For debugging purposes, shows rays traced from texel in final gather step."));
#endif
#ifdef SV_LIGHTFIELD
		winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_2D,_("Build 2d lightfield"),_("Lightfield is illumination captured in 3d, lightmap for freely moving dynamic objects. Not saved to disk, for testing only."));
		winMenu->Append(ME_STATIC_BUILD_LIGHTFIELD_3D,_("Build 3d lightfield"),_("Lightfield is illumination captured in 3d, lightmap for freely moving dynamic objects. Not saved to disk, for testing only."));
#endif
		menuBar->Append(winMenu, _("Global illumination"));
	}
	*/

	// Window...
	{
		winMenu = new wxMenu;
		winMenu->AppendCheckItem(ME_WINDOW_FULLSCREEN_META,_("Fullscreen")+" (F11)",_("Fullscreen mode uses full desktop resolution."));
		winMenu->Check(ME_WINDOW_FULLSCREEN_META,svs.fullscreen);
		winMenu->AppendCheckItem(ME_WINDOW_TREE,_("Scene tree"),_("Opens scene tree window."));
		winMenu->Check(ME_WINDOW_TREE,IS_SHOWN(m_sceneTree));
		winMenu->AppendCheckItem(ME_WINDOW_USER_PROPERTIES,_("User preferences"),_("Opens user preferences window."));
		winMenu->Check(ME_WINDOW_USER_PROPERTIES,IS_SHOWN(m_userProperties));
		winMenu->AppendCheckItem(ME_WINDOW_SCENE_PROPERTIES,_("Scene properties"),_("Opens scene properties window."));
		winMenu->Check(ME_WINDOW_SCENE_PROPERTIES,IS_SHOWN(m_sceneProperties));
		winMenu->AppendCheckItem(ME_WINDOW_GI_PROPERTIES,_("Global Illumination"),_("Opens global illumination properties window."));
		winMenu->Check(ME_WINDOW_GI_PROPERTIES,IS_SHOWN(m_giProperties));
		winMenu->AppendCheckItem(ME_WINDOW_LIGHT_PROPERTIES,_("Light"),_("Opens light properties window and starts rendering light icons."));
		winMenu->Check(ME_WINDOW_LIGHT_PROPERTIES,IS_SHOWN(m_lightProperties));
		winMenu->AppendCheckItem(ME_WINDOW_OBJECT_PROPERTIES,_("Object"),_("Opens object properties window."));
		winMenu->Check(ME_WINDOW_OBJECT_PROPERTIES,IS_SHOWN(m_objectProperties));
		winMenu->AppendCheckItem(ME_WINDOW_MATERIAL_PROPERTIES,_("Material"),_("Opens material properties window."));
		winMenu->Check(ME_WINDOW_MATERIAL_PROPERTIES,IS_SHOWN(m_materialProperties));
		winMenu->AppendCheckItem(ME_WINDOW_LOG,_("Log"),_("Opens log window."));
		winMenu->Check(ME_WINDOW_LOG,IS_SHOWN(m_log));
		winMenu->AppendSeparator();
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT1,_("Workspace")+" 1 (alt-1)",_("Your custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT2,_("Workspace")+" 2 (alt-2)",_("Your custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT3,_("Workspace")+" 3 (alt-3)",_("Your custom window layout, changes automatically saved per user."));
		winMenu->AppendRadioItem(ME_WINDOW_LAYOUT_RESET,_("Reset workspaces"),_("Resets your custom window layouts to defaults; it's the same as deleting .prefs file."));
		winMenu->Check(ME_WINDOW_LAYOUT1+userPreferences.currentWindowLayout,true);
		winMenu->AppendSeparator();
		winMenu->AppendCheckItem(ME_WINDOW_RESIZE,_("Set viewport size"),_("Lets you set exact viewport size in pixels."));
#ifdef _WIN32
		winMenu->AppendSeparator();
		winMenu->Append(ME_WINDOW_SMALLLUXGPU,"SmallLuxGPU",_("Open scene in SmallLuxGPU, GPU accelerated pathtracer. It can be configured via data/scenes/SmallLuxGpu/scene.cfg"));
#endif
		menuBar->Append(winMenu, _("Windows"));
	}

	// About...
	{
		winMenu = new wxMenu;
		winMenu->Append(ME_HELP,_("Help - controls")+" (h)");
#ifdef _WIN32
		winMenu->Append(ME_SDK_HELP,_("Lightsprint SDK manual"));
		winMenu->Append(ME_SUPPORT,_("Report bug, get support"));
		winMenu->Append(ME_LIGHTSPRINT,_("Lightsprint web"));
#endif
		winMenu->Append(ME_CHECK_SOLVER,_("Log solver diagnose"),_("For diagnostic purposes."));
		winMenu->Append(ME_CHECK_SCENE,_("Log scene errors"),_("For diagnostic purposes."));
		winMenu->Append(ME_RENDER_DDI,_("Render solver DDI"),_("For diagnostic purposes."));
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
	if (i!=std::string::npos) // filenames without '.' are skipped, they should not exist
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

void SVFrame::saveScreenshot(rr::RRBuffer* sshot)
{
	// fix empty filename
	if (userPreferences.sshotFilename.empty())
	{
		time_t t = time(nullptr);
		userPreferences.sshotFilename += wxString::Format("screenshot%04d.jpg",(int)(t%10000));
	}

	// if path is relative, prepend scene dir
	wxString finalFilename = userPreferences.sshotFilename;
	if (bf::path(RR_WX2PATH(finalFilename)).is_relative())
	{
		bf::path tmp(RR_WX2PATH(svs.sceneFilename));
		tmp.remove_filename();
#ifdef _WIN32
		// if scene dir is empty, prepend desktop dir
		if (tmp.empty())
		{
			// there is nothing like wxStandardPaths::Get().GetDesktopDir()
			char screenshotFilename[1000]=".";
			SHGetSpecialFolderPathA(nullptr, screenshotFilename, CSIDL_DESKTOP, FALSE); // CSIDL_PERSONAL
			tmp = screenshotFilename;
		}
#endif
		tmp /= RR_WX2PATH(finalFilename);
		finalFilename = RR_PATH2WX(tmp);
	}

	// save
	rr::RRBuffer::SaveParameters saveParameters;
	saveParameters.jpegQuality = 100;
	if (sshot->save(RR_WX2RR(finalFilename),nullptr,&saveParameters))
		rr::RRReporter::report(rr::INF2,"Saved %ls.\n",RR_WX2WCHAR(finalFilename));
	else
	{
		rr::RRReporter::report(rr::WARN,"Error: Failed to save %ls.\n",RR_WX2WCHAR(finalFilename));
	}

	// increment filename
	incrementFilename(userPreferences.sshotFilename);
	m_userProperties->updateProperties();
}

void SVFrame::TogglePane(wxWindow* window)
{
	if (window)
	{
		m_mgr.GetPane(window).Show(!m_mgr.GetPane(window).IsShown());
		m_mgr.Update();
	}
}

void SVFrame::saveBakedLayers()
{
	// resave baked layers under new name's directory
	rr::RRObjects allObjects = m_canvas->solver->getObjects();
	allObjects.saveLayer(svs.layerBakedLightmap,LAYER_PREFIX,LMAP_POSTFIX);
	allObjects.saveLayer(svs.layerBakedAmbient,LAYER_PREFIX,AMBIENT_POSTFIX);
	allObjects.saveLayer(svs.layerBakedEnvironment,LAYER_PREFIX,ENV_POSTFIX);
	allObjects.saveLayer(svs.layerBakedLDM,LAYER_PREFIX,LDM_POSTFIX);
}

bool SVFrame::chooseSceneFilename(wxString fileSelectorCaption, wxString& selectedFilename)
{
	// wildcard format: "BMP and GIF files (*.bmp;*.gif)|*.bmp;*.gif|PNG files (*.png)|*.png"
	wxString wxextensions;
	{
		wxString extensions = rr::RRScene::getSupportedSaverExtensions();
		if (extensions.empty())
		{
			wxMessageBox(_("Program built without saving."),_("No savers registered."),wxOK);
			return false;
		}
		while (!extensions.empty())
		{
			size_t i = extensions.find(';');
			wxString ext = (i==-1) ? extensions : extensions.substr(0,i);
			if (!wxextensions.empty())
				wxextensions += "|";
			wxextensions += ext+'|'+ext;
			extensions.erase(0,ext.size()+1);
		}
	}

	// replace extension if current one can't be saved
	//  windows would append valid extension if we just delete bad one, wx 2.9.1 @ osx needs valid ext from us
	bf::path presetFilename = RR_WX2PATH(selectedFilename);
	{
		wxString extension = RR_PATH2WX(presetFilename.extension());
		wxString extensions = rr::RRScene::getSupportedSaverExtensions();
		bool extensionSupportsSave = !extension.empty() && extensions.find(extension)!=-1;
		if (!extensionSupportsSave) presetFilename.replace_extension(".rr3");
	}

	// dialog
	wxFileDialog dialog(this,fileSelectorCaption,"","",wxextensions,wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
	dialog.SetPath(RR_PATH2WX(presetFilename));
	if (dialog.ShowModal()!=wxID_OK)
		return false;
	selectedFilename = dialog.GetPath();
	return true;
}

bool SVFrame::saveScene(wxString sceneFilename)
{
	bool result = false;
	if (m_canvas->solver)
	{
		rr::RRScene scene;
		scene.objects = m_canvas->solver->getObjects();
		scene.lights = m_canvas->solver->getLights();
		// export to .scn for SmallLuxGPU4 has some tweaks
		// (it's the only format that saves env to new file, rather than passing just filename. so it can export env rotate+blend)
		bool exportToSlg4 = sceneFilename.EndsWith(".scn");

		scene.environment = m_canvas->solver->getEnvironment();
		if (exportToSlg4 && m_canvas->solver)
		{
			rr::RRReal angleRad0 = 0;
			rr::RRReal angleRad1 = 0;
			rr::RRReal blendFactor = m_canvas->solver->getEnvironmentBlendFactor();
			rr::RRBuffer* environment0 = m_canvas->solver->getEnvironment(0,&angleRad0);
			rr::RRBuffer* environment1 = m_canvas->solver->getEnvironment(1,&angleRad1);
			scene.environment = rr::RRBuffer::createEnvironmentBlend(environment0,environment1,angleRad0,angleRad1,blendFactor);
		}
		scene.cameras.push_back(svs.camera);
		if (exportToSlg4 && !svs.renderDof)
			scene.cameras[0].apertureDiameter = 0;
		result = scene.save(RR_WX2RR(sceneFilename));
		if (exportToSlg4)
			delete scene.environment;
		scene.environment = nullptr; // would be deleted in destructor otherwise
	}
	return result;
}

void SVFrame::OnAnyChange(EventSource eventSource, const wxPGProperty* property, const wxEvent* event, unsigned menuEvent)
{
	stateVersion++;
	// restart pathtracer unless
	if (!(eventSource==ES_PROPERTY && property==m_giProperties->propGITechnique) // changing GI technique
		&& !(eventSource==ES_MENU && (menuEvent==ME_LIGHTING_INDIRECT_FIREBALL || menuEvent==ME_LIGHTING_INDIRECT_ARCHITECT)) // changing GI technique
		&& !(eventSource==ES_KEYBOARD_MID_MOVEMENT && ((wxKeyEvent*)event)->GetKeyCode()==WXK_F9) // F9 saving screenshot
		&& !(eventSource==ES_MENU && (menuEvent==ME_FILE_SAVE_SCREENSHOT || menuEvent==ME_FILE_SAVE_SCREENSHOT_ORIGINAL || menuEvent==ME_FILE_SAVE_SCREENSHOT_ENHANCED))
		)
	{
		lastInteractionTime.setNow();
		m_canvas->solver->reportInteraction();
		m_canvas->pathTracedAccumulator = 0;
	}
}


void SVFrame::OnMenuEvent(wxCommandEvent& event)
{
	OnMenuEventCore(event.GetId());
}

wxString convertSlashes(wxString path)
{
	// wxLaunchDefaultApplication (and underlying ShellExecute) have strange behaviour
	// foo/bar/baz.chm fails in windows with error code file not found
	// foo/bar\\baz.chm and foo\\bar\\baz.chm work
	// workaround: calling this function on wxLaunchDefaultApplication arguments
#ifdef _WIN32
	path.Replace('/','\\');
#else
	path.Replace('\\','/');
#endif
	return path;
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
	rr_gl::RRSolverGL*& solver = m_canvas->solver;
	bool& fireballLoadAttempted = m_canvas->fireballLoadAttempted;
	int* windowCoord = m_canvas->windowCoord;

	switch (eventCode)
	{

		//////////////////////////////// FILE ///////////////////////////////

		case ME_FILE_OPEN_SCENE:
			{
				wxFileDialog dialog(this,_("Choose a 3d scene to open"),"","",getSupportedLoaderExtensions(svs),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				if (dialog.ShowModal()==wxID_OK)
				{
					svs.sceneFilename = dialog.GetPath();
					svs.autodetectCamera = true;
					UpdateEverything();
				}
			}
			break;
		case ME_FILE_MERGE_SCENE:
			{
				wxFileDialog dialog(this,_("Choose a 3d scene to merge with current scene"),"","",getSupportedLoaderExtensions(svs),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				if (dialog.ShowModal()==wxID_OK)
				{
					// [x] objects, [x] lights dialog
					if (importDlg.ShowModal()==wxID_OK)
					{
						// display log window with 'abort' while this function runs
						LogWithAbort logWithAbort(this,m_canvas->solver,_("Merging scene..."));

						rr::RRScene* scene = loadScene(dialog.GetPath(),userPreferences.import.getUnitLength(dialog.GetPath()),userPreferences.import.getUpAxis(dialog.GetPath()),true);
						if (!importDlg.objects->GetValue())
							scene->objects.clear();
						if (!importDlg.lights->GetValue())
							scene->lights.clear();

						m_canvas->addOrRemoveScene(scene,true,false);
						m_canvas->mergedScenes.push_back(scene);
					}
				}
			}
			break;
		case ME_FILE_SAVE_SCENE:
			// no name yet?
			if (svs.sceneFilename.empty())
				goto save_scene_as;
			// name that can't be saved?
			{
				wxString extension = RR_PATH2WX(bf::path(RR_WX2PATH(svs.sceneFilename)).extension());
				wxString extensions = rr::RRScene::getSupportedSaverExtensions();
				bool extensionSupportsSave = !extension.empty() && extensions.find(extension)!=-1;
				if (!extensionSupportsSave) goto save_scene_as;
			}
			// valid name, save it
save_scene:
			if (!saveScene(svs.sceneFilename))
				wxMessageBox(_("Scene save failed."),_("Not saved."),wxOK|wxICON_ERROR);
			break;
		case ME_FILE_SAVE_SCENE_AS:
save_scene_as:
			if (chooseSceneFilename(_("Save as"),svs.sceneFilename))
			{
				UpdateTitle();
				updateSceneTree();
				saveBakedLayers();
				goto save_scene;
			}
			break;
		case ME_FILE_SAVE_SCREENSHOT:
			OnMenuEventCore(userPreferences.sshotEnhanced?SVFrame::ME_FILE_SAVE_SCREENSHOT_ENHANCED:SVFrame::ME_FILE_SAVE_SCREENSHOT_ORIGINAL);
			break;
		case ME_FILE_SAVE_SCREENSHOT_ORIGINAL:
			{
				// Grab content of backbuffer to sshot.
				wxSize size = m_canvasWindow->GetSize();
				rr::RRBuffer* sshot = rr::RRBuffer::create(rr::BT_2D_TEXTURE,size.x,size.y,1,rr::BF_RGB,true,nullptr);
				glReadBuffer(GL_BACK);
				rr_gl::readPixelsToBuffer(sshot);
					//unsigned char* pixels = sshot->lock(rr::BL_DISCARD_AND_WRITE);
					//glReadPixels(0,0,size.x,size.y,GL_RGB,GL_UNSIGNED_BYTE,pixels);
					//sshot->unlock();

				// save sshot
				// when user manually changes sshot extension to exr or hdr, he probably want's floats,
				// so let's directly save pathTracedBuffer instead of screen (even though it is not tonemapped, has no icons etc)
				wxString sshotExt = userPreferences.sshotFilename.Right(4).Lower();
				bool sshotHdr = sshotExt=="hdr" || sshotExt=="exr";
				saveScreenshot((svs.pathEnabled && sshotHdr)?m_canvas->pathTracedBuffer:sshot);

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
				rr::RRVector<rr_gl::RealtimeLight*>& lights = m_canvas->solver->realtimeLights;
				unsigned* shadowSamples = new unsigned[lights.size()];
				unsigned* shadowRes = new unsigned[lights.size()];
				for (unsigned i=0;i<lights.size();i++)
				{
					rr_gl::RealtimeLight* rl = lights[i];
					shadowRes[i] = rl->getRRLight().rtShadowmapSize;
					shadowSamples[i] = rl->getNumShadowSamples();
					rl->setShadowmapSize(shadowRes[i]*userPreferences.sshotEnhancedShadowResolutionFactor);
					if (userPreferences.sshotEnhancedShadowSamples)
						rl->setNumShadowSamples(userPreferences.sshotEnhancedShadowSamples);
				}

				// 2. alloc temporary textures
				//    (it must have alpha, because mirror needs render target with alpha. radeons work even without alpha, geforces need it)
				rr::RRBuffer* bufColor = rr::RRBuffer::create(rr::BT_2D_TEXTURE,bigSize.x,bigSize.y,1,rr::BF_RGBA,true,nullptr);
				rr::RRBuffer* bufDepth = rr::RRBuffer::create(rr::BT_2D_TEXTURE,bigSize.x,bigSize.y,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
				rr_gl::Texture texColor(bufColor,false,false);
				rr_gl::Texture texDepth(bufDepth,false,false);
				bool srgb = supportsSRGB() && svs.srgbCorrect;
				if (srgb)
				{
					// prepare framebuffer for sRGB correct rendering
					// (GL_FRAMEBUFFER_SRGB will be enabled inside Paint())
					texColor.reset(false,false,true);
				}

				// 3a. set new rendertarget
				rr_gl::FBO oldFBOState = rr_gl::FBO::getState();
				rr_gl::FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,&texDepth,oldFBOState);
				rr_gl::FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,&texColor,oldFBOState);
				if (!rr_gl::FBO::isOk())
				{
					wxMessageBox(_("Try lower resolution or disable FSAA (both in User preferences / Screenshot)."),_("Rendering screenshot failed."));
				}
				else
				{
					// 3b. propagate new size/aspect to renderer, correct screenCenter so that camera still points to the same area
					wxSize oldSize(m_canvas->winWidth,m_canvas->winHeight);
					rr::RRCamera oldEye = svs.camera;
					svs.camera.setAspect(bigSize.x/(float)bigSize.y,(bigSize.x*m_canvas->winHeight>m_canvas->winWidth*bigSize.y)?1:0);
					svs.camera.setScreenCenter(RRVec2(
						svs.camera.getScreenCenter().x * tan(oldEye.getFieldOfViewHorizontalRad()/2)/tan(svs.camera.getFieldOfViewHorizontalRad()/2),
						svs.camera.getScreenCenter().y * tan(oldEye.getFieldOfViewVerticalRad()/2)/tan(svs.camera.getFieldOfViewVerticalRad()/2)
						));
					m_canvas->winWidth = bigSize.x;
					m_canvas->winHeight = bigSize.y;
					glViewport(0,0,bigSize.x,bigSize.y);

					// 4. disable automatic tonemapping, uses FBO, would not work
					bool oldTonemapping = svs.tonemappingAutomatic;
					svs.tonemappingAutomatic = false;

					// 6. render to texColor
					m_canvas->Paint(true,"");

					// 7. downscale to sshot
					rr::RRBuffer* sshot = rr::RRBuffer::create(rr::BT_2D_TEXTURE,smallSize.x,smallSize.y,1,rr::BF_RGB,true,nullptr);
					unsigned char* pixelsBig = bufColor->lock(rr::BL_DISCARD_AND_WRITE);
					glPixelStorei(GL_PACK_ALIGNMENT,1);
					glReadPixels(0,0,bigSize.x,bigSize.y,GL_RGBA,GL_UNSIGNED_BYTE,pixelsBig);
					unsigned char* pixelsSmall = sshot->lock(rr::BL_DISCARD_AND_WRITE);
					for (int j=0;j<smallSize.y;j++)
						for (int i=0;i<smallSize.x;i++)
							for (unsigned k=0;k<3;k++)
							{
								unsigned a = 0;
								for (int y=0;y<AA;y++)
									for (int x=0;x<AA;x++)
										a += pixelsBig[k+4*(AA*i+x+AA*smallSize.x*(AA*j+y))];
								pixelsSmall[k+3*(i+smallSize.x*j)] = (a+AA*AA/2)/(AA*AA);
							}
					sshot->unlock();
					bufColor->unlock();

					// 8. save sshot
					saveScreenshot(sshot);

					// 7. cleanup
					delete sshot;

					// 4. cleanup
					svs.tonemappingAutomatic = oldTonemapping;

					// 3b. cleanup
					m_canvas->winWidth = oldSize.x;
					m_canvas->winHeight = oldSize.y;
					svs.camera = oldEye;
				}

				// 3a. cleanup
				oldFBOState.restore();
				m_canvas->OnSizeCore(true); //glViewport(0,0,m_canvas->winWidth,m_canvas->winHeight);

				// 2. cleanup
				delete bufDepth;
				delete bufColor;

				// 1b. cleanup
				for (unsigned i=0;i<lights.size();i++)
				{
					rr_gl::RealtimeLight* rl = lights[i];
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

		///////////////////////////////// MATERIAL ////////////////////////////////

		case ME_MATERIAL_LOAD:
			{
				wxFileDialog dialog(this,_("Enter material filename"),"","",rr::RRMaterials::getSupportedLoaderExtensions(),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				if (dialog.ShowModal()!=wxID_OK)
					break;
				if (dialog.GetPath().empty())
					break;
				rr::RRMaterials* materials = rr::RRMaterials::load(RR_WX2RR(dialog.GetPath()),nullptr);
				if (materials)
				{
					if (materials->size())
					{
						unsigned selectedMaterialIndex = 0;
						if (materials->size()==1 || getMaterial("Available materials:",this,*materials,selectedMaterialIndex))
						{
							m_materialProperties->material->copyFrom(*(*materials)[selectedMaterialIndex]);
							m_materialProperties->material->convertToLinear(m_canvas->solver->getColorSpace());
						}
						for (unsigned i=0;i<materials->size();i++)
							delete (*materials)[i];
					}
					delete materials;
				}
			}
			break;

		case ME_MATERIAL_SAVE:
			{
				wxFileDialog dialog(this,_("Enter material filename"),"","",rr::RRMaterials::getSupportedSaverExtensions(),wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
				if (dialog.ShowModal()!=wxID_OK)
					break;
				if (dialog.GetPath().empty())
					break;
				m_materialProperties->material->save(RR_WX2RR(dialog.GetPath()));
			}
			break;

		case ME_MATERIAL_SAVE_ALL:
			{
				wxFileDialog dialog(this,_("Enter material library filename"),"","",rr::RRMaterials::getSupportedSaverExtensions(),wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
				if (dialog.ShowModal()!=wxID_OK)
					break;
				if (dialog.GetPath().empty())
					break;
				rr::RRMaterials materials;
				m_canvas->solver->getObjects().getAllMaterials(materials);
				materials.save(RR_WX2RR(dialog.GetPath()));
			}
			break;


		//////////////////////////////// VIEW ///////////////////////////////

		case ME_VIEW_TOP:    svs.camera.setView(rr::RRCamera::TOP   ,solver,nullptr,nullptr); break;
		case ME_VIEW_BOTTOM: svs.camera.setView(rr::RRCamera::BOTTOM,solver,nullptr,nullptr); break;
		case ME_VIEW_LEFT:   svs.camera.setView(rr::RRCamera::LEFT  ,solver,nullptr,nullptr); break;
		case ME_VIEW_RIGHT:  svs.camera.setView(rr::RRCamera::RIGHT ,solver,nullptr,nullptr); break;
		case ME_VIEW_FRONT:  svs.camera.setView(rr::RRCamera::FRONT ,solver,nullptr,nullptr); break;
		case ME_VIEW_BACK:   svs.camera.setView(rr::RRCamera::BACK  ,solver,nullptr,nullptr); break;
		case ME_VIEW_RANDOM: svs.camera.setView(rr::RRCamera::RANDOM,solver,nullptr,nullptr); svs.cameraMetersPerSecond = svs.camera.getFar()*0.08f; break;


		//////////////////////////////// ENVIRONMENT ///////////////////////////////

		case ME_ENV_WHITE:
			svs.skyboxFilename = RR_WX2RR(svs.pathToMaps+"skybox/white.png");
			goto reload_skybox;
		case ME_ENV_BLACK:
			svs.skyboxFilename = RR_WX2RR(svs.pathToMaps+"skybox/black.png");
			goto reload_skybox;
		case ME_ENV_WHITE_TOP:
			svs.skyboxFilename = RR_WX2RR(svs.pathToMaps+"skybox/white_top.png");
			goto reload_skybox;
		case ME_ENV_OPEN:
			{
				wxFileDialog dialog(this,_("Choose a skybox image"),"","","*.*",wxFD_OPEN|wxFD_FILE_MUST_EXIST);
				dialog.SetPath(svs.skyboxFilename.empty()?svs.pathToMaps+"skybox/":wxString(RR_RR2WX(svs.skyboxFilename)));
				if (dialog.ShowModal()!=wxID_OK)
					break;

				svs.skyboxFilename = RR_WX2RR(dialog.GetPath());
			}
			goto reload_skybox;
		case ME_ENV_RELOAD: // not a menu item, just command we can call from outside
			{
reload_skybox:
				rr::RRBuffer* skybox = rr::RRBuffer::loadCube(svs.skyboxFilename,textureLocator);
				if (skybox)
				{
					solver->setEnvironment(skybox,solver->getEnvironment(0),svs.skyboxRotationRad,svs.skyboxRotationRad);
					delete skybox; // after solver creates its own reference, we can delete our skybox, we don't need it anymore
					m_canvas->skyboxBlendingInProgress = true;
					m_canvas->skyboxBlendingStartTime.setNow(); // starts 3sec smooth transition in SVCanvas::Paint()
					if (svs.playVideos)
						skybox->play();
				}
				else
				{
					solver->setEnvironment(nullptr,nullptr);
				}
			}
			break;
			
		//////////////////////////////// LIGHTS ///////////////////////////////

		case ME_LIGHT_SPOT: m_sceneTree->runContextMenuAction(CM_LIGHT_SPOT,EntityIds()); break;
		case ME_LIGHT_POINT: m_sceneTree->runContextMenuAction(CM_LIGHT_POINT,EntityIds()); break;
		case ME_LIGHT_DIR: m_sceneTree->runContextMenuAction(CM_LIGHT_DIR,EntityIds()); break;
		case ME_LIGHT_FLASH: m_sceneTree->runContextMenuAction(CM_LIGHT_FLASH,EntityIds()); break;


		//////////////////////////////// GLOBAL ILLUMINATION - INDIRECT ///////////////////////////////

		case ME_LIGHTING_INDIRECT_FIREBALL:
			svs.renderLightIndirect = LI_REALTIME_FIREBALL;
			svs.renderLightmaps2d = 0;
			solver->reportDirectIlluminationChange(-1,true,true,false);
			if (solver->getInternalSolverType()!=rr::RRSolver::FIREBALL && solver->getInternalSolverType()!=rr::RRSolver::BOTH)
			{
				if (!fireballLoadAttempted)
				{
					fireballLoadAttempted = true;
					solver->loadFireball("",true);
				}
			}
			if (solver->getInternalSolverType()!=rr::RRSolver::FIREBALL && solver->getInternalSolverType()!=rr::RRSolver::BOTH)
			{
				// display log window with 'abort' while this function runs
				// don't display it if it is already displayed (we may be automatically called when scene loads and fireball is requested, log is already on)
				LogWithAbort logWithAbort(this,solver,_("Building Fireball..."));

				// ask no questions, it's possible scene is loading right now and it's not safe to render/idle. dialog would render/idle on background
				solver->buildFireball(svs.fireballQuality,"");
				solver->reportDirectIlluminationChange(-1,true,true,false);
				// this would ask questions
				//OnMenuEventCore(ME_REALTIME_FIREBALL_BUILD);
			}
			break;

		case ME_LIGHTING_INDIRECT_ARCHITECT:
			svs.renderLightIndirect = LI_REALTIME_ARCHITECT;
			svs.renderLightmaps2d = 0;
			solver->reportDirectIlluminationChange(-1,true,true,false);
			fireballLoadAttempted = false;
			solver->leaveFireball();
			break;


		//////////////////////////////// GLOBAL ILLUMINATION - BUILD ///////////////////////////////

		case ME_REALTIME_FIREBALL_BUILD:
			{
				{
					// display log window with 'abort' while this function runs
					LogWithAbort logWithAbort(this,solver,_("Building Fireball..."));

					svs.renderLightIndirect = LI_REALTIME_FIREBALL;
					svs.renderLightmaps2d = 0;
					solver->buildFireball(svs.fireballQuality,"");
					solver->reportDirectIlluminationChange(-1,true,true,false);
					fireballLoadAttempted = true;
				}
			}
			break;

#ifdef SV_LIGHTFIELD
		case ME_STATIC_BUILD_LIGHTFIELD_2D:
			{
				// create solver if it doesn't exist yet
				if (solver->getInternalSolverType()==rr::RRSolver::NONE)
				{
					solver->calculate();
				}

				rr::RRVec4 aabbMin,aabbMax;
				solver->getMultiObject()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,nullptr);
				aabbMin.y = aabbMax.y = svs.camera.pos.y;
				aabbMin.w = aabbMax.w = 0;
				delete m_canvas->lightField;
				m_canvas->lightField = rr::RRLightField::create(aabbMin,aabbMax-aabbMin,1);
				m_canvas->lightField->captureLighting(solver,0,false);
			}
			break;
		case ME_STATIC_BUILD_LIGHTFIELD_3D:
			{
				// create solver if it doesn't exist yet
				if (solver->getInternalSolverType()==rr::RRSolver::NONE)
				{
					solver->calculate();
				}

				rr::RRVec4 aabbMin,aabbMax;
				solver->getMultiObject()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,nullptr);
				aabbMin.w = aabbMax.w = 0;
				delete m_canvas->lightField;
				m_canvas->lightField = rr::RRLightField::create(aabbMin,aabbMax-aabbMin,1);
				m_canvas->lightField->captureLighting(solver,0,false);
			}
			break;
#endif
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
						rr::RRSolver::UpdateParameters params(quality);
						params.debugObject = m_canvas->centerObject;
						params.debugTexel = m_canvas->centerTexel;
						params.debugTriangle = (m_canvas->centerTexel==UINT_MAX)?m_canvas->centerTriangle:UINT_MAX;
						params.debugRay = SVRayLog::push_back;
						SVRayLog::size = 0;
						solver->updateLightmaps(svs.layerBakedLightmap,-1,-1,&params,nullptr);
					}
				}
				else
				{
					rr::RRReporter::report(rr::WARN,"No lightmap in center of screen.\n");
				}
			}
			break;
#endif

		///////////////////////////////// WINDOW ////////////////////////////////

		case ME_WINDOW_FULLSCREEN_META:
		case ME_WINDOW_FULLSCREEN:
			svs.fullscreen = !svs.fullscreen;
			if (svs.fullscreen)
			{
				// wxFULLSCREEN_ALL does not work for menu (probably wx error), hide it manualy
				wxMenuBar* oldMenuBar = GetMenuBar();
				SetMenuBar(nullptr);
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
		case ME_WINDOW_TREE:                 TogglePane(m_sceneTree); break;
		case ME_WINDOW_USER_PROPERTIES:      TogglePane(m_userProperties); break;
		case ME_WINDOW_SCENE_PROPERTIES:     TogglePane(m_sceneProperties); break;
		case ME_WINDOW_GI_PROPERTIES:        TogglePane(m_giProperties); break;
		case ME_WINDOW_LIGHT_PROPERTIES:     TogglePane(m_lightProperties); break;
		case ME_WINDOW_OBJECT_PROPERTIES:    TogglePane(m_objectProperties); break;
		case ME_WINDOW_MATERIAL_PROPERTIES:  TogglePane(m_materialProperties); break;
		case ME_WINDOW_LOG:                  TogglePane(m_log); break;
		case ME_WINDOW_LAYOUT_RESET:
			userPreferences.resetLayouts();
			userPreferencesApplyToWx();
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
						bool panes = IS_SHOWN(m_sceneTree) || IS_SHOWN(m_userProperties) || IS_SHOWN(m_sceneProperties) || IS_SHOWN(m_giProperties) || IS_SHOWN(m_lightProperties) || IS_SHOWN(m_objectProperties) || IS_SHOWN(m_materialProperties);
						wxMessageBox(panes?_("There's not enough space. You can make more space by resizing or closing panes."):_("There's not enough space. Switch to fullscreen mode for maximal resolution."));
					}
				}
			}
			break;
#ifdef _WIN32
		case ME_WINDOW_SMALLLUXGPU:
			{
				if (!svs.srgbCorrect)
					rr::RRReporter::report(rr::WARN,"With Global illumination/sRGB correctness unchecked, lighting differes from pathtracer a lot.\n");
				_chdir("../../data/scenes/SmallLuxGpu");
				saveScene("scene.scn");
#if defined(_M_X64) || defined(_LP64)
				_spawnl(_P_NOWAIT,"../../../bin/x64/slg4.exe","../../../bin/x64/slg4.exe","scene.cfg",nullptr);
				_chdir("../../../bin/x64");
#else
				_spawnl(_P_NOWAIT,"../../../bin/win32/slg4.exe","../../../bin/win32/slg4.exe","scene.cfg",nullptr);
				_chdir("../../../bin/win32");
#endif
			}
			break;
#endif


		//////////////////////////////// HELP ///////////////////////////////

		case ME_HELP: svs.renderHelp = !svs.renderHelp; break;
		case ME_SDK_HELP:
			wxLaunchDefaultApplication(convertSlashes(svs.pathToData+"../doc/Lightsprint.chm"));
			break;
		case ME_SUPPORT:
			wxLaunchDefaultApplication(wxString::Format("mailto:support@lightsprint.com?Subject=Bug in build %s %s%d %d",__DATE__,
				"win",
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
				));
			break;
		case ME_LIGHTSPRINT:
			wxLaunchDefaultBrowser("http://lightsprint.com");
			break;
		case ME_CHECK_SOLVER:
			solver->checkConsistency();
			break;
		case ME_CHECK_SCENE:
			solver->getStaticObjects().checkConsistency("static");
			solver->getDynamicObjects().checkConsistency("dynamic");
			break;
		case ME_RENDER_DDI:
			svs.renderDDI = !svs.renderDDI;
			break;
		case ME_ABOUT:
			{
#ifdef __WXMAC__
				wxIcon* icon = nullptr; // icon asserts in wx 2.9.1 @ OSX 10.6
#else
				wxIcon* icon = loadIcon(svs.pathToMaps+"sv_logo.png");
#endif
				wxAboutDialogInfo info;
				if (icon) info.SetIcon(*icon);
				info.SetName("Lightsprint SDK");
				info.SetCopyright("(c) 1999-2015 Stepan Hrbek, Lightsprint\n\n"+_("3rd party contributions - see Lightsprint SDK manual.")+"\n");
				info.SetWebSite("http://lightsprint.com");
				wxAboutBox(info);
				delete icon;
			}
			break;

			
	}

	UpdateMenuBar();

	OnAnyChange(ES_MENU,nullptr,nullptr,eventCode);
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
		case ST_OBJECT:
			return EntityId(m_canvas->selectedType,svs.selectedObjectIndex);
		case ST_CAMERA:
			return EntityId(m_canvas->selectedType,0);
		default:
			return EntityId(m_canvas->selectedType,0);
	}
}

void SVFrame::selectEntityInTreeAndUpdatePanel(EntityId entity, SelectEntityAction action)
{
	switch (entity.type)
	{
		case ST_LIGHT:
			// update light properties
			m_lightProperties->setLight(m_canvas->solver->realtimeLights[entity.index],svs.precision);
			m_canvas->selectedType = entity.type;
			svs.selectedLightIndex = entity.index;
			if (action==SEA_ACTION) OnMenuEventCore(ME_WINDOW_LIGHT_PROPERTIES);
			break;

		case ST_OBJECT:
			// update object+material properties
			{
				rr::RRObject* object = m_canvas->solver->getObject(entity.index);
				m_objectProperties->setObject(object,svs.precision);
				if (object && object->faceGroups.size())
				{
					// start of optional code
					// lets panel find both custom+physical materials for static objects
					// (advantage: changes will be propagated to physical materials used by pathtracer
					//             there is also chance occasional FB divergence was caused by not-updated physical materials)
					if (!object->isDynamic)
					{
						unsigned hitTriangle = 0; // some triangle of selected object in multiobject
						if (m_canvas->solver && m_canvas->solver->getMultiObject())
						{
							hitTriangle = m_canvas->solver->getMultiObject()->getCollider()->getMesh()->getPostImportTriangle(rr::RRMesh::PreImportNumber(entity.index,0));
						}
						m_materialProperties->setMaterial(m_canvas->solver,nullptr,hitTriangle,rr::RRVec2(0));
					}
					else
					// end of optional code
					{
						// dynamic object -> send only custom material, there is no physical one permanently created
						m_materialProperties->setMaterial(object->faceGroups[0].material);
					}
				}
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


	if (action!=SEA_NOTHING && m_sceneTree)
	{
		m_sceneTree->selectEntityInTree(entity); // cycle: selectEntityInTree() -> ctrl::SelectItem() -> OnSelChanged() -> selectEntityInTreeAndUpdatePanel()
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
			m_lightProperties->setLight(nullptr,svs.precision);
			if (m_canvas->selectedType==ST_LIGHT)
				m_canvas->selectedType = ST_CAMERA;
		}
		else
		{
			m_lightProperties->setLight(m_canvas->solver->realtimeLights[svs.selectedLightIndex],svs.precision);
		}

		// update selected object
		if (!validateIndex(svs.selectedObjectIndex,m_canvas->solver->getStaticObjects().size()+m_canvas->solver->getDynamicObjects().size()))
		{
			m_objectProperties->setObject(nullptr,svs.precision);
		}
		else
		{
			m_objectProperties->setObject(m_canvas->solver->getObject(svs.selectedObjectIndex),svs.precision);
		}
	}

	m_objectProperties->updateProperties();
	m_materialProperties->updateProperties();
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
	m_giProperties->CommitChangesFromEditor();
	m_objectProperties->CommitChangesFromEditor();
	m_materialProperties->CommitChangesFromEditor();
}

rr::RRScene* SVFrame::loadScene(const wxString& _filename, float _units, unsigned _upAxis, bool _popup)
{
	rr::RRScene* scene = new rr::RRScene(RR_WX2RR(_filename),textureLocator,&m_canvas->solver->aborting);
	scene->objects.removeEmptyObjects();
	scene->normalizeUnits(_units);
	scene->normalizeUpAxis(_upAxis);
	scene->objects.flipFrontBack(userPreferences.import.flipFrontBack,true);
	return scene;
}

bool SVFrame::oculusActive()
{
#ifdef SUPPORT_OCULUS
	return !svs.renderLightmaps2d && svs.renderStereo && (userPreferences.stereoMode==rr::RRCamera::SM_OCULUS_RIFT) && oculusHMD;
#else
	return !svs.renderLightmaps2d && svs.renderStereo && (userPreferences.stereoMode==rr::RRCamera::SM_OCULUS_RIFT);
#endif
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
				double hour = svs.envDateTime.tm_hour+svs.envDateTime.tm_min/60.+svs.envDateTime.tm_sec/3600.;
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
				m_canvas->solver->realtimeLights[i]->getCamera()->setDirection(rr::RRVec3((rr::RRReal)(-sin(AZ)*sin(SZA)),(rr::RRReal)(-cosSZA),(rr::RRReal)(-cosAZ*sin(SZA)))); // north = Z-, east = X-, up = Y+
				m_canvas->solver->reportDirectIlluminationChange(i,true,true,false);
				break;
			}
		}
	}
}


void SVFrame::enableTooltips(bool enable)
{
	wxToolTip::Enable(enable); // necessary, some tooltips don't disappear without this
	m_lightProperties->enableTooltips(enable);
	m_objectProperties->enableTooltips(enable);
	m_materialProperties->enableTooltips(enable);
	m_sceneProperties->enableTooltips(enable);
	m_giProperties->enableTooltips(enable);
	m_userProperties->enableTooltips(enable);
}

BEGIN_EVENT_TABLE(SVFrame, wxFrame)
	EVT_MENU(wxID_EXIT, SVFrame::OnExit)
	EVT_MENU(wxID_ANY, SVFrame::OnMenuEvent)
	EVT_AUI_PANE_RESTORE(SVFrame::OnPaneOpenClose)
	EVT_AUI_PANE_CLOSE(SVFrame::OnPaneOpenClose)
END_EVENT_TABLE()

}; // namespace
