#include <windows.h>
#include <stdio.h>
#include <memory.h>
#include <tchar.h>
#include "resource.h"
#include <wininet.h>
#include <richedit.h>
#pragma comment(lib,"wininet")

HINSTANCE g_hInst;

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			// set icon for alt-tab
			// dialog properties must have system_menu=false to avoid ugly small icon in dialog
			SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(g_hInst,MAKEINTRESOURCE(IDI_MAIN_ICON),IMAGE_ICON,0,0,0));

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
				if(mode.dmBitsPerPel==32 && mode.dmPelsWidth>=480 && mode.dmPelsHeight>=480)
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
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("highest supported"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("4 samples"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("12 samples"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("16 samples"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("20 samples"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("24 samples"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("28 samples"));
			SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("32 samples"));
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
			// prepare params
			bool fullscreen = SendDlgItemMessage(hDlg,IDC_FULLSCREEN,BM_GETCHECK,0,0)==BST_CHECKED;
			unsigned resolutionIdx = (unsigned)SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_GETCURSEL,0,0);
			char resolutionStr[1000];
			resolutionStr[0] = 0;
			SendDlgItemMessageA(hDlg,IDC_RESOLUTION,CB_GETLBTEXT,resolutionIdx,(LPARAM)resolutionStr);
			unsigned penumbraIdx = (unsigned)SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_GETCURSEL,0,0);
			char buf[1000];
			if(penumbraIdx)
				sprintf(buf,"%s %s penumbra%d",fullscreen?"fullscreen":"window",resolutionStr,penumbraIdx+((penumbraIdx>1)?1:0));
			else
				sprintf(buf,"%s %s",fullscreen?"fullscreen":"window",resolutionStr);

			// minimize
			RECT rect;
			GetWindowRect(hDlg, &rect);
			ShowWindow(hDlg, SW_MINIMIZE);

			// run
			//ShellExecuteA( NULL, "open", "bin\\win32\fcss_sr.exe", params, NULL, SW_SHOWNORMAL );

			// run & wait
			SHELLEXECUTEINFOA ShExecInfo = {0};
			ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			ShExecInfo.hwnd = NULL;
			ShExecInfo.lpVerb = NULL;
			ShExecInfo.lpFile = "..\\bin\\win32\\fcss_sr.exe";
			ShExecInfo.lpParameters = buf;
			ShExecInfo.lpDirectory = "data";
			ShExecInfo.nShow = SW_SHOW;
			ShExecInfo.hInstApp = NULL;
			DWORD score = 0;
			if(ShellExecuteExA(&ShExecInfo))
			{
				if(WaitForSingleObject(ShExecInfo.hProcess,INFINITE)!=WAIT_FAILED)
				{
					if(!GetExitCodeProcess(ShExecInfo.hProcess,&score)) score = 0;
					if(score>10000) score = 0;
				}
			}

			// restore
			ShowWindow(hDlg, SW_RESTORE);
			SetWindowPos(hDlg, HWND_TOP, rect.left, rect.top, 0, 0, SWP_NOSIZE);

			// show result
			if(score==10000)
			{
				// escaped
				sprintf(buf,"");
			}
			else
			if(!score)
			{
				// error
				if(ShellExecuteA( NULL, "open", "log.txt", NULL, NULL, SW_SHOWNORMAL ))
					sprintf(buf,"-");
				else
					sprintf(buf,":");
			}
			else
			{
				// finished
				sprintf(buf,"%d",score);
				SendDlgItemMessageA(hDlg,IDC_STATIC3,WM_SETTEXT,0,(LPARAM)"score = fps");
			}
			SendDlgItemMessageA(hDlg,IDC_SCORE,WM_SETTEXT,0,(LPARAM)buf);
			RECT rect2 = {280,130,200,100};
			InvalidateRect(hDlg,&rect2,true);
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
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, About);
	return 0;
}
