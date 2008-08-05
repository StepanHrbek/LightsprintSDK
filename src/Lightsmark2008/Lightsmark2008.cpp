// Lightsmark 2008 front-end, (C) Stepan Hrbek

#include <windows.h>
#include <stdio.h>
#include <memory.h>
#include <tchar.h>
#include "resource.h"
#include <wininet.h>
#include <richedit.h>
#pragma comment(lib,"wininet")
#include <sys\types.h> 
#include <sys\stat.h> 
#include <set>

__int64 FileSize64( const char * szFileName ) 
{ 
	struct __stat64 fileStat;
	fileStat.st_size = 0;
	int err = _stat64( szFileName, &fileStat );
	if(err) return 0;
	return fileStat.st_size;
}

HINSTANCE g_hInst;
bool g_extendedOptions = false;

struct Mode
{
	unsigned w,h,fullscr;
	bool operator <(const Mode& m) const
	{
		return w*h+((1-fullscr)<<30) < m.w*m.h+((1-m.fullscr)<<30);
	}
};

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			// set icon for alt-tab
			SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(g_hInst,MAKEINTRESOURCE(IDI_MAIN_ICON),IMAGE_ICON,0,0,0));

			// fill resolution
			DEVMODE currentMode;
			EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,&currentMode);

			typedef std::set<Mode> Modes;
			Modes modes;
			for(unsigned i=0;1;i++)
			{
				DEVMODE mode;
				if(!EnumDisplaySettings(NULL,i,&mode)) break;
				if(mode.dmBitsPerPel==32 && mode.dmPelsWidth>=480 && mode.dmPelsHeight>=480)
				{
					Mode m;
					m.w = mode.dmPelsWidth;
					m.h = mode.dmPelsHeight;
					m.fullscr = 1;
					modes.insert(m);
					if(mode.dmPelsWidth<=currentMode.dmPelsWidth-22 && mode.dmPelsHeight<=currentMode.dmPelsHeight-22)
					{
						m.fullscr = 0;
						modes.insert(m);
					}
				}
			}
			unsigned modeIdx = 0;
			bool defaultFound = 0;
			for(Modes::const_iterator i=modes.begin();i!=modes.end();i++)
			{
				char buf[100];
				sprintf(buf,"%4dx%d%s",i->w,i->h,i->fullscr?" fullscreen":" window");
				if(SendDlgItemMessageA(hDlg,IDC_RESOLUTION,CB_FINDSTRING,0,(LPARAM)buf)<0)
				{
					SendDlgItemMessageA(hDlg,IDC_RESOLUTION,CB_ADDSTRING,0,(LPARAM)buf);
					// if possible, set 1280x1024
					if(i->w==1280 && i->h==1024 && i->fullscr)
					{
						SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_SETCURSEL,modeIdx,0);
						defaultFound = true;
					}
					// if 1280x1024 was not found yet, set desktop res
					if(!defaultFound && i->w==currentMode.dmPelsWidth && i->h==currentMode.dmPelsHeight && i->fullscr)
					{
						SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_SETCURSEL,modeIdx,0);
					}
				}
				modeIdx++;
			}

			// fill music
			SendDlgItemMessage(hDlg,IDC_MUSIC,BM_SETCHECK,BST_CHECKED,0);

			// fill stability
			SendDlgItemMessage(hDlg,IDC_STABILITY,CB_ADDSTRING,0,(LPARAM)_T("highest supported"));
			SendDlgItemMessage(hDlg,IDC_STABILITY,CB_ADDSTRING,0,(LPARAM)_T("low"));
			SendDlgItemMessage(hDlg,IDC_STABILITY,CB_ADDSTRING,0,(LPARAM)_T("high"));
			SendDlgItemMessage(hDlg,IDC_STABILITY,CB_SETCURSEL,0,0);

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
		if(wParam==0xffff) // click on image
		{
			ShowWindow(GetDlgItem(hDlg,IDC_X64),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(hDlg,IDC_MUSIC),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(hDlg,IDC_STABILITY),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(hDlg,IDC_STATIC4),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(hDlg,IDC_PENUMBRA),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(hDlg,IDC_STATIC2),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(hDlg,IDC_EDITOR),SW_SHOWNORMAL);
			g_extendedOptions = true;
		}
		if(LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		if(LOWORD(wParam)==IDC_START)
		{
			// prepare params
			char buf[1000];
			buf[0] = 0;

			unsigned resolutionIdx = (unsigned)SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_GETCURSEL,0,0);
			SendDlgItemMessageA(hDlg,IDC_RESOLUTION,CB_GETLBTEXT,resolutionIdx,(LPARAM)buf);

			if(g_extendedOptions)
			{
				unsigned penumbraIdx = (unsigned)SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_GETCURSEL,0,0);
				if(penumbraIdx)
					sprintf(buf+strlen(buf)," penumbra%d",penumbraIdx+((penumbraIdx>1)?1:0));

				unsigned stabilityIdx = (unsigned)SendDlgItemMessage(hDlg,IDC_STABILITY,CB_GETCURSEL,0,0);
				if(stabilityIdx)
				{
					char stabilityStr[100];
					stabilityStr[0] = 0;
					SendDlgItemMessageA(hDlg,IDC_STABILITY,CB_GETLBTEXT,stabilityIdx,(LPARAM)stabilityStr);
					sprintf(buf+strlen(buf)," stability=%s",stabilityStr);
				}

				bool editor = SendDlgItemMessage(hDlg,IDC_EDITOR,BM_GETCHECK,0,0)==BST_CHECKED;
				if(editor)
					strcat(buf," editor");

				bool music = SendDlgItemMessage(hDlg,IDC_MUSIC,BM_GETCHECK,0,0)==BST_CHECKED;
				if(!music)
					strcat(buf," silent");
			}

			// minimize
			RECT rect;
			GetWindowRect(hDlg, &rect);
			ShowWindow(hDlg, SW_MINIMIZE);

			// run
			//ShellExecuteA( NULL, "open", "backend.exe", params, NULL, SW_SHOWNORMAL );

			// run & wait
			SHELLEXECUTEINFOA ShExecInfo = {0};
			ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			ShExecInfo.hwnd = NULL;
			ShExecInfo.lpVerb = NULL;
			bool x64 = SendDlgItemMessage(hDlg,IDC_X64,BM_GETCHECK,0,0)==BST_CHECKED;
			ShExecInfo.lpFile = x64 ? "..\\x64\\backend.exe" : "..\\win32\\backend.exe";
			ShExecInfo.lpParameters = buf;
			ShExecInfo.lpDirectory = NULL; //"..\\..\\data"; WINE can't emulate and GPUShaderPerf can't stand non-NULL here, let's rather change dir in backend
			ShExecInfo.nShow = SW_SHOW;
			ShExecInfo.hInstApp = NULL;
			DWORD score = 0;
			bool showLog = 0;
			if(ShellExecuteExA(&ShExecInfo))
			{
				if(WaitForSingleObject(ShExecInfo.hProcess,INFINITE)!=WAIT_FAILED)
				{
					if(!GetExitCodeProcess(ShExecInfo.hProcess,&score)) score = 0;
						else showLog = score==0; // show log only if demo successfully returns 0, other errors are reported by OS
					if(score>30000) score = 0;
				}
			}

			// show result
			if(score>0 && score<30000)
			{
				// finished with score
				//if(score<300)
					sprintf(buf,"%d.%d",score/10,score%10);
				//else
				//	sprintf(buf,"%d",score/10);
				SendDlgItemMessageA(hDlg,IDC_SCORE,WM_SETTEXT,0,(LPARAM)buf);
				SendDlgItemMessageA(hDlg,IDC_STATIC3,WM_SETTEXT,0,(LPARAM)"average fps");
			}
			else
			{
				buf[0] = 0;
				SendDlgItemMessageA(hDlg,IDC_SCORE,WM_SETTEXT,0,(LPARAM)buf);
				SendDlgItemMessageA(hDlg,IDC_STATIC3,WM_SETTEXT,0,(LPARAM)buf);
			}
			RECT rect2 = {280,130,200,100};
			InvalidateRect(hDlg,&rect2,true);
			if(showLog)
			{
				// finished with error
				// try to open log at location 1
				const char* log1 = "..\\..\\log.txt";
				if(FileSize64(log1))
					ShellExecuteA( NULL, "open", log1, NULL, NULL, SW_SHOWNORMAL );
				else
				{
					// try to open log at location 2, used when location 1 is not writeable
					// (e.g. program files in vista)
					const char* appdata = getenv("LOCALAPPDATA");
					if(appdata)
					{
						char log2[1000];
						sprintf(log2,"%s\\Lightsmark 2008\\log.txt",appdata);
						if(FileSize64(log2))
							ShellExecuteA( NULL, "open", log2, NULL, NULL, SW_SHOWNORMAL );
					}
				}
			}

			// restore
			ShowWindow(hDlg, SW_RESTORE);
			SetWindowPos(hDlg, HWND_TOP, rect.left, rect.top, 0, 0, SWP_NOSIZE);
		}
		if(LOWORD(wParam)==IDC_WEB)
		{
			ShellExecuteA( NULL, "open", "http://dee.cz/lightsmark", NULL, NULL, SW_SHOWNORMAL );
		}
		if(LOWORD(wParam)==IDC_README)
		{
			ShellExecuteA( NULL, "open", "..\\..\\readme.txt", NULL, NULL, SW_SHOWNORMAL );
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
