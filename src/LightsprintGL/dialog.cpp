#include <windows.h> // CreateDialog
#include <process.h> // _beginthread
#include "dialog.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/SceneViewer.h"

static rr::RRDynamicSolver* g_solver;
static rr::RRDynamicSolver::UpdateParameters* g_updateParams;
static rr::RRReporter* g_oldReporter;
static HWND g_hDlg;
static bool g_shown;
static bool g_cmdBuild;
static bool g_cmdCustom;
static bool g_cmdViewer;
static bool g_cmdEnd;
static unsigned char g_dialogResource[] = {
1,0,255,255,0,0,0,0,0,0,0,0,200,10,200,128,7,0,0,0,0,0,60,1,134,
0,0,0,0,0,76,0,105,0,103,0,104,0,116,0,115,0,112,0,114,0,105,0,110,0,
116,0,32,0,108,0,105,0,103,0,104,0,116,0,109,0,97,0,112,0,32,0,98,0,117,
0,105,0,108,0,100,0,0,0,8,0,144,1,0,1,77,0,83,0,32,0,83,0,104,0,
101,0,108,0,108,0,32,0,68,0,108,0,103,0,0,0,0,0,0,0,0,0,0,0,1,
0,1,80,88,0,7,0,50,0,14,0,1,0,0,0,255,255,128,0,66,0,85,0,73,0,
76,0,68,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,80,4,1,7,
0,49,0,14,0,233,3,0,0,255,255,128,0,100,0,101,0,98,0,117,0,103,0,103,0,
101,0,114,0,0,0,0,0,0,0,0,0,0,0,0,0,128,0,129,80,35,0,7,0,40,
0,14,0,234,3,0,0,255,255,129,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
2,80,7,0,9,0,24,0,8,0,255,255,255,255,255,255,130,0,81,0,117,0,97,0,108,
0,105,0,116,0,121,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,196,8,177,80,
7,0,29,0,46,1,98,0,235,3,0,0,255,255,129,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,1,64,147,0,7,0,50,0,14,0,236,3,0,0,255,255,128,0,65,0,
98,0,111,0,114,0,116,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
80,205,0,7,0,50,0,14,0,237,3,0,0,255,255,128,0,99,0,117,0,115,0,116,0,
111,0,109,0,0,0,0,0
};

static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		g_hDlg = hDlg;
		// init quality
		if(g_updateParams)
		{
			char buf[20];
			_itoa(g_updateParams->quality,buf,10);
			SendDlgItemMessageA(g_hDlg,IDC_QUALITY,WM_SETTEXT,0,(LPARAM)buf);
		}
		else
		{
			ShowWindow(GetDlgItem(hDlg,IDC_QUALITY),SW_HIDE);
		}
		// init custom button
		ShowWindow(GetDlgItem(hDlg,IDC_UEBUILD),SW_HIDE);
		// auto start building
		goto build;

	case WM_SHOWWINDOW:
		g_shown = true;
		break;

	case WM_COMMAND:
		if(LOWORD(wParam)==IDOK)
		{
build:
			rr::RRReporter::report(rr::INF1,"--- BUILD ---\n");
			SendDlgItemMessageA(hDlg,IDC_QUALITY,EM_SETREADONLY,(WPARAM)true,0);
			ShowWindow(GetDlgItem(hDlg,IDOK),SW_HIDE);
			ShowWindow(GetDlgItem(hDlg,IDC_UEBUILD),SW_HIDE);
			ShowWindow(GetDlgItem(hDlg,IDC_VIEWER),SW_HIDE);
			ShowWindow(GetDlgItem(hDlg,IDC_ABORT),SW_SHOWNORMAL);
			g_cmdBuild = true;
			return (INT_PTR)TRUE;
		}
		if(LOWORD(wParam)==IDC_UEBUILD)
		{
			g_cmdCustom = true;
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		if(LOWORD(wParam)==IDC_VIEWER)
		{
			rr::RRReporter::report(rr::INF1,"--- VIEWER ---\n");
			SendDlgItemMessageA(hDlg,IDC_QUALITY,EM_SETREADONLY,(WPARAM)true,0);
			ShowWindow(GetDlgItem(hDlg,IDOK),SW_HIDE);
			ShowWindow(GetDlgItem(hDlg,IDC_UEBUILD),SW_HIDE);
			ShowWindow(GetDlgItem(hDlg,IDC_VIEWER),SW_HIDE);
			ShowWindow(GetDlgItem(hDlg,IDC_ABORT),SW_SHOWNORMAL);
			g_cmdViewer = true;
			return (INT_PTR)TRUE;
		}
		if(LOWORD(wParam)==IDC_ABORT)
		{
			rr::RRReporter::report(rr::INF1,"ABORTING\n");
			g_solver->aborting = true;
			ShowWindow(GetDlgItem(hDlg,IDC_ABORT),SW_HIDE);
			return (INT_PTR)TRUE;
		}
		if(LOWORD(wParam)==IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

static INT_PTR CALLBACK BringMainThreadToForeground(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
			return (INT_PTR)TRUE;
		case WM_SHOWWINDOW:
			EndDialog(hDlg, LOWORD(wParam));
	}
	return (INT_PTR)FALSE;
}

static void dialog(void* w)
{
	DialogBoxIndirect(GetModuleHandle(NULL),(LPDLGTEMPLATE)g_dialogResource,NULL,DlgProc);
	g_solver->aborting = true;
	g_cmdEnd = true;
}

class RRReporterDialog : public rr::RRReporter
{
public:
	virtual void customReport(rr::RRReportType type, int indentation, const char* message)
	{
		// send it also to old reporter
		if(type!=rr::INF2 && type!=rr::INF3 && type!=rr::TIMI) // send only most important messages
			g_oldReporter->customReport(type,indentation,message);

		// indentation
		char space[1000];
		space[0] = 0;
		indentation *= 2;
		if(indentation>0 && indentation<999)
		{
			memset(space,' ',indentation);
			space[indentation] = 0;
		}
		// type
		if(type<rr::ERRO || type>rr::TIMI) type = rr::INF9;
		static const char* typePrefix[] = {"ERROR: ","Assertion failed: ","Warning: ","","","","","",""};
		strcat(space,typePrefix[type]);
		// message
		strcat(space,message);
		// crlf
		space[strlen(space)-1]=0;
		strcat(space,"\r\n");
		// send
		int pos = GetWindowTextLength(GetDlgItem(g_hDlg,IDC_LOG));
		SendDlgItemMessageA(g_hDlg,IDC_LOG,EM_SETSEL,(pos>29000)?0:pos,pos);
		SendDlgItemMessageA(g_hDlg,IDC_LOG,EM_REPLACESEL,(WPARAM)FALSE,(LPARAM)space);
		SendDlgItemMessageA(g_hDlg,IDC_LOG,WM_VSCROLL,SB_BOTTOM,0);
	}
};

rr_gl::UpdateResult rr_gl::updateLightmapsWithDialog(rr::RRDynamicSolver* solver,
				  int layerNumberLighting, int layerNumberDirectionalLighting, int layerNumberBentNormals, rr::RRDynamicSolver::UpdateParameters* paramsDirect, rr::RRDynamicSolver::UpdateParameters* paramsIndirect, const rr::RRDynamicSolver::FilteringParameters* filtering,
				  bool createWindow, const char* pathToShaders,
				  const char* customButton)
{
	g_solver = solver;
	g_updateParams = paramsDirect ? paramsDirect : paramsIndirect;
	g_cmdBuild = false;
	g_cmdCustom = false;
	g_cmdViewer = false;
	g_solver->aborting = false;
	g_cmdEnd = false;
	bool updated = false;

	RRReporterDialog newReporter;
	g_oldReporter = rr::RRReporter::getReporter();
	rr::RRReporter::setReporter(&newReporter);

	g_shown = false;
	_beginthread(dialog,0,NULL);

	// When helper thread opens dialog, main thread loses right to create foreground windows.
	// Temporary fix: Main thread opens dialog that closes itself immediately.
	// Tested: XP SP2 32bit
	// If you know proper fix, please help.
	while(!g_shown) Sleep(10);
	DialogBoxIndirect(GetModuleHandle(NULL),(LPDLGTEMPLATE)g_dialogResource,NULL,BringMainThreadToForeground);

	while(!g_cmdEnd)
	{
		if(g_cmdBuild)
		{
			g_cmdBuild = false;
			char buf[100];
			buf[0] = 0;
			SendDlgItemMessageA(g_hDlg,IDC_QUALITY,WM_GETTEXT,99,(LPARAM)buf);
			if(paramsDirect) paramsDirect->quality = atoi(buf);
			if(paramsIndirect) paramsIndirect->quality = atoi(buf);
			g_solver->updateLightmaps(layerNumberLighting,layerNumberDirectionalLighting,layerNumberBentNormals,paramsDirect,paramsIndirect,filtering);
			SendDlgItemMessageA(g_hDlg,IDC_QUALITY,EM_SETREADONLY,(WPARAM)false,0);
			ShowWindow(GetDlgItem(g_hDlg,IDOK),SW_SHOWNORMAL);
			if(customButton)
			{
				SendDlgItemMessageA(g_hDlg,IDC_UEBUILD,WM_SETTEXT,0,(LPARAM)customButton);
				ShowWindow(GetDlgItem(g_hDlg,IDC_UEBUILD),SW_SHOWNORMAL);
			}
			ShowWindow(GetDlgItem(g_hDlg,IDC_VIEWER),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(g_hDlg,IDC_ABORT),SW_HIDE);
			updated = !g_solver->aborting;
			if(!g_solver->aborting)
				EndDialog(g_hDlg, IDCANCEL);
			g_solver->aborting = false;
		}
		if(g_cmdViewer)
		{
			g_cmdViewer = false;
			rr_gl::SceneViewerState svs;
			svs.renderRealtime = 0; // switch from default realtime GI to static GI
			svs.staticLayerNumber = layerNumberLighting; // switch from default layer to our layer
			svs.adjustTonemapping = false; // turn off tonemapping, it mostly impractical in ue3 scenes
			rr_gl::sceneViewer(g_solver,createWindow,pathToShaders,&svs);
			SendDlgItemMessageA(g_hDlg,IDC_QUALITY,EM_SETREADONLY,(WPARAM)false,0);
			ShowWindow(GetDlgItem(g_hDlg,IDOK),SW_SHOWNORMAL);
			if(customButton)
			{
				SendDlgItemMessageA(g_hDlg,IDC_UEBUILD,WM_SETTEXT,0,(LPARAM)customButton);
				ShowWindow(GetDlgItem(g_hDlg,IDC_UEBUILD),SW_SHOWNORMAL);
			}
			ShowWindow(GetDlgItem(g_hDlg,IDC_VIEWER),SW_SHOWNORMAL);
			ShowWindow(GetDlgItem(g_hDlg,IDC_ABORT),SW_HIDE);
			g_solver->aborting = false;
		}
		Sleep(1);
	}

	g_solver->aborting = false;
	rr::RRReporter::setReporter(g_oldReporter);
	return g_cmdCustom?UR_CUSTOM:(updated?UR_UPDATED:UR_ABORTED);
}
