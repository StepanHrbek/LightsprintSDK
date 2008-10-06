// --------------------------------------------------------------------------
// Setup dialogue for offline lightmap build.
// Copyright 2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#ifdef _WIN32

#include <cstdio> // sprintf
#include <windows.h>
#include "RRReporter/reporterWindow.h"
#include "Preferences.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// OfflineSetup - dialogue

static bool     s_ok; // true if "Build" is hit, false if "Esc"
static bool     s_enabled;
static unsigned s_quality;

static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
			{
				SendDlgItemMessage(hWnd,IDC_CHECK_ENABLE_LIGHTSPRINT,BM_SETCHECK,s_enabled?BST_CHECKED:BST_UNCHECKED,0);
				char buf[10];
				sprintf(buf,"%d",s_quality);
				SendDlgItemMessage(hWnd,IDC_EDIT_QUALITY,WM_SETTEXT,0,(LPARAM)buf);
				SendDlgItemMessage(hWnd,IDC_EDIT_QUALITY,WM_ENABLE,(WPARAM)s_enabled,0);
			}
			break;
		case WM_COMMAND:
			if (LOWORD(wParam)==IDCANCEL)
			{
				s_enabled = SendDlgItemMessage(hWnd,IDC_CHECK_ENABLE_LIGHTSPRINT,BM_GETCHECK,0,0)==BST_CHECKED;
				char buf[10];
				SendDlgItemMessage(hWnd,IDC_EDIT_QUALITY,WM_GETTEXT,(WPARAM)10,(LPARAM)buf);
				s_quality = atoi(buf);
				s_ok = false;
				EndDialog(hWnd,0);
				return (INT_PTR)TRUE;
			}
			if (LOWORD(wParam)==IDOK)
			{
				s_enabled = SendDlgItemMessage(hWnd,IDC_CHECK_ENABLE_LIGHTSPRINT,BM_GETCHECK,0,0)==BST_CHECKED;
				char buf[10];
				SendDlgItemMessage(hWnd,IDC_EDIT_QUALITY,WM_GETTEXT,(WPARAM)10,(LPARAM)buf);
				s_quality = atoi(buf);
				s_ok = true;
				EndDialog(hWnd,0);
				return (INT_PTR)TRUE;
			}
			if (LOWORD(wParam)==IDC_CHECK_ENABLE_LIGHTSPRINT)
			{
				s_enabled = SendDlgItemMessage(hWnd,IDC_CHECK_ENABLE_LIGHTSPRINT,BM_GETCHECK,0,0)==BST_CHECKED;
				SendDlgItemMessage(hWnd,IDC_EDIT_QUALITY,WM_ENABLE,(WPARAM)s_enabled,0);
			}
			break;
	}
	return (INT_PTR)FALSE;
}

bool offlineSetup(bool& enabled, unsigned& quality)
{
	s_enabled = enabled;
	s_quality = quality;
	s_ok = true;
	static unsigned char dialogResource[] =
	{
		1,0,255,255,0,0,0,0,8,0,0,0,202,8,200,128,5,0,0,0,0,0,4,1,97,
		0,0,0,0,0,66,0,117,0,105,0,108,0,100,0,32,0,115,0,116,0,97,0,116,0,
		105,0,99,0,32,0,108,0,105,0,103,0,104,0,116,0,105,0,110,0,103,0,0,0,8,
		0,144,1,0,1,77,0,83,0,32,0,83,0,104,0,101,0,108,0,108,0,32,0,68,0,
		108,0,103,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,80,168,0,76,0,85,
		0,14,0,1,0,0,0,255,255,128,0,66,0,117,0,105,0,108,0,100,0,32,0,108,0,
		105,0,103,0,104,0,116,0,109,0,97,0,112,0,115,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,3,0,1,80,16,0,22,0,213,0,10,0,244,3,0,0,255,255,128,0,
		85,0,115,0,101,0,32,0,76,0,105,0,103,0,104,0,116,0,115,0,112,0,114,0,105,
		0,110,0,116,0,32,0,102,0,111,0,114,0,32,0,109,0,111,0,114,0,101,0,32,0,
		114,0,101,0,97,0,108,0,105,0,115,0,116,0,105,0,99,0,32,0,108,0,105,0,103,
		0,104,0,116,0,109,0,97,0,112,0,115,0,32,0,40,0,103,0,108,0,111,0,98,0,
		97,0,108,0,32,0,105,0,108,0,108,0,117,0,109,0,105,0,110,0,97,0,116,0,105,
		0,111,0,110,0,41,0,0,0,0,0,0,0,0,0,0,0,0,0,128,32,129,80,16,0,
		44,0,37,0,13,0,246,3,0,0,255,255,129,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,2,80,61,0,41,0,192,0,21,0,255,255,255,255,255,255,130,0,81,0,117,0,
		97,0,108,0,105,0,116,0,121,0,44,0,32,0,49,0,48,0,32,0,105,0,115,0,32,
		0,103,0,111,0,111,0,100,0,32,0,102,0,111,0,114,0,32,0,112,0,114,0,101,0,
		118,0,105,0,101,0,119,0,44,0,32,0,51,0,48,0,48,0,45,0,51,0,48,0,48,
		0,48,0,32,0,102,0,111,0,114,0,32,0,112,0,114,0,111,0,100,0,117,0,99,0,
		116,0,105,0,111,0,110,0,46,0,32,0,51,0,48,0,48,0,32,0,105,0,115,0,32,
		0,114,0,111,0,117,0,103,0,108,0,121,0,32,0,49,0,48,0,120,0,32,0,102,0,
		97,0,115,0,116,0,101,0,114,0,32,0,116,0,104,0,97,0,110,0,32,0,51,0,48,
		0,48,0,48,0,46,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,80,
		7,0,7,0,246,0,61,0,255,255,255,255,255,255,128,0,76,0,105,0,103,0,104,0,116,
		0,115,0,112,0,114,0,105,0,110,0,116,0,0,0,0,0
	};
	DialogBoxIndirectParam(GetModuleHandle(NULL),(LPDLGTEMPLATE)dialogResource,NULL,DlgProc,NULL);
	enabled = s_enabled;
	quality = s_quality;
	return s_ok;
}

bool offlineSetup(bool& enabled, unsigned& quality, bool finalQuality)
{
	// read preferences
	const char* location = "OfflineSetup";
	enabled = Preferences::getValue(location,"enabled",enabled?1.f:0)!=0;
	quality = (unsigned)Preferences::getValue(location,finalQuality?"qualityFinal":"qualityPreview",(float)quality);

	// ask user to confirm/change
	bool result = offlineSetup(enabled,quality);

	// store preferences
	Preferences::setValue(location,"enabled",enabled?1.f:0);
	Preferences::setValue(location,finalQuality?"qualityFinal":"qualityPreview",(float)quality);
	return result;
}

} //namespace

#else // _WIN32

bool rr::offlineSetup(bool& enabled, unsigned& quality)
{
	return false;
}

bool offlineSetup(const char* preferences, bool finalQuality)
{
	return false;
}

#endif
