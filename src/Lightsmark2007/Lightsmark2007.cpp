#include <windows.h>
#include <stdio.h>
#include <memory.h>
#include <tchar.h>
#include "resource.h"
#include <wininet.h>
#include <richedit.h>
#pragma comment(lib,"wininet")

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			// center window
			HWND hWnd = GetDesktopWindow();
			RECT r1, r2;
			GetClientRect(hWnd, &r1);
			GetWindowRect(hDlg, &r2);
			POINT pt;
			pt.x = (r1.right - r1.left)/2 - (r2.right - r2.left)/2;
			pt.y = (r1.bottom - r1.top)/2 - (r2.bottom - r2.top)/2;
			ClientToScreen(hWnd, &pt);
			SetWindowPos(hDlg, HWND_TOP, pt.x, pt.y, 0, 0, SWP_NOSIZE);

			// fill fullscreen
			SendDlgItemMessage(hDlg,IDC_FULLSCREEN,BM_SETCHECK,BST_CHECKED,0);

			// fill resolution
			DEVMODE currentMode;
			EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,&currentMode);
			unsigned modes = 0;
			for(unsigned i=0;1;i++)
			{
				DEVMODE mode;
				if(!EnumDisplaySettings(NULL,i,&mode)) break;
				if(mode.dmBitsPerPel>=15)
				{
					char buf[100];
					sprintf(buf,"%dx%d",mode.dmPelsWidth,mode.dmPelsHeight);
					if(SendDlgItemMessageA(hDlg,IDC_RESOLUTION,CB_FINDSTRING,0,(LPARAM)buf)<0)
					{
						SendDlgItemMessageA(hDlg,IDC_RESOLUTION,CB_ADDSTRING,0,(LPARAM)buf);
						if(mode.dmPelsWidth==currentMode.dmPelsWidth && mode.dmPelsHeight==currentMode.dmPelsHeight)
							SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_SETCURSEL,modes,0);
						modes++;
					}
				}
			}

			// fill penumbra
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("highest"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("hard (1 sample)"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("soft (4 samples)"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("12 samples"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("16, only some GPUs"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("20, only some GPUs"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("24, only some GPUs"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("28, only some GPUs"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("32, only some GPUs"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_SETCURSEL,0,0);

			// fill result
			CHARFORMAT cf;
			memset( &cf, 0, sizeof(CHARFORMAT) );
			cf.cbSize = sizeof(CHARFORMAT);
			cf.dwMask = CFM_BOLD | CFM_COLOR | CFM_SIZE;
			cf.dwEffects = CFE_BOLD;
			cf.crTextColor = 0x2222FF;
			cf.yHeight = 500;
			cf.yOffset = 0;
			SendDlgItemMessage( hDlg,IDC_SCORE,EM_SETCHARFORMAT,4,(LPARAM)&cf);
		}
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if(LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		if(LOWORD(wParam)==IDC_START)
		{
			bool fullscreen = SendDlgItemMessage(hDlg,IDC_FULLSCREEN,BM_GETCHECK,0,0)==BST_CHECKED;
			unsigned resolution = (unsigned)SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_GETCURSEL,0,0);
			unsigned penumbra = (unsigned)SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_GETCURSEL,0,0);
			char buf[100];
			sprintf(buf,"%d %d %d",fullscreen?1:0,resolution,penumbra);
			SendDlgItemMessage(hDlg,IDC_FULLSCREEN,BM_SETCHECK,BST_CHECKED,0);
			//!!! exec
			// predat parametry
			// pri chybe zobrazit log
			static unsigned score = 680;
			score+=111;
			sprintf(buf,"%d",score);
			SendDlgItemMessageA(hDlg,IDC_SCORE,WM_SETTEXT,0,(LPARAM)buf);
			SendDlgItemMessageA(hDlg,IDC_STATIC3,WM_SETTEXT,0,(LPARAM)"score = fps");
			RECT rect = {280,130,200,100};
			InvalidateRect(hDlg,&rect,true);
		}
		if(LOWORD(wParam)==IDC_LIGHTSPRINT)
		{
			ShellExecuteA( NULL, "open", "http://lightsprint.com", NULL, NULL, SW_SHOWNORMAL );
		}
		break;
	}
	return (INT_PTR)FALSE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	HMODULE hRE = LoadLibrary(_T("riched20.dll"));
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, About);
	return 0;
}
