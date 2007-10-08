#include <windows.h>
#include <stdio.h>
#include <memory.h>
#include <tchar.h>
#include "resource.h"

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
		HWND hWnd = GetDesktopWindow();
		RECT r1, r2;
		GetClientRect(hWnd, &r1);
		GetWindowRect(hDlg, &r2);
		POINT pt;
		pt.x = (r1.right - r1.left)/2 - (r2.right - r2.left)/2;
		pt.y = (r1.bottom - r1.top)/2 - (r2.bottom - r2.top)/2;
		ClientToScreen(hWnd, &pt);
		SetWindowPos(hDlg, HWND_TOP, pt.x, pt.y, 0, 0, SWP_NOSIZE);
		}
		SendDlgItemMessage(hDlg,IDC_FULLSCREEN,BM_SETCHECK,BST_CHECKED,0);
		//!!! zjistit spravny rozliseni
		SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_ADDSTRING,0,(LPARAM)_T("800x600"));
		SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_ADDSTRING,0,(LPARAM)_T("1680x1050"));
		//!!! deteknout rozliseni desktopu
		SendDlgItemMessage(hDlg,IDC_RESOLUTION,CB_SETCURSEL,0,0);
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("auto"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("hard (1 sample)"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("soft (4 samples)"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("12 samples"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("16, only some GPUs"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("20, only some GPUs"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("24, only some GPUs"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("28, only some GPUs"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_ADDSTRING,0,(LPARAM)_T("32, only some GPUs"));
		SendDlgItemMessage(hDlg,IDC_PENUMBRA,CB_SETCURSEL,0,0);
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
			// pri uspechu vypsat fps
		}
		if(LOWORD(wParam)==IDC_LIGHTSPRINT)
		{
			//!!! browser
		}
		break;
	}
	return (INT_PTR)FALSE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, About);
	return 0;
}
