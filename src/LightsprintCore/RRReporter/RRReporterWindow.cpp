// --------------------------------------------------------------------------
// Reporting messages.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#ifdef _WIN32

#include <windows.h>
#include <process.h> // _beginthread
#include "reporterWindow.h"
#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporterWindow

//! Custom callback interface.
class RRCallback
{
public:
	virtual void run() = 0;
};

class RRReporterWindow : public RRReporter
{
public:
	RRReporterWindow(RRCallback* _exitCallback)
	{
		hWnd = 0;
		exitCallback = _exitCallback;
		shown = false;
		_beginthread(windowThreadFunc,0,this);

		// When helper thread opens dialog, main thread loses right to create foreground windows.
		// Temporary fix: Main thread opens dialog that closes itself immediately.
		// Tested: XP SP2 32bit
		// If you know proper fix, please help.
		while (!shown) Sleep(1);
		DialogBoxIndirect(GetModuleHandle(NULL),getDialogResource(),NULL,BringMainThreadToForeground);

		// If we manage to get rid of hack above, we will have to:
		// Wait for thread to create window.
		// If we don't wait, first messages may be sent to uninitialized hWnd and lost.
		//while (!hWnd) Sleep(1);
	}

	static LPDLGTEMPLATE getDialogResource()
	{
		static unsigned char dialogResource[] =
		{
			1,0,255,255,0,0,0,0,0,0,0,0,200,10,200,128,2,0,0,0,0,0,95,1,13,
			1,0,0,0,0,76,0,105,0,103,0,104,0,116,0,115,0,112,0,114,0,105,0,110,0,
			116,0,32,0,108,0,111,0,103,0,0,0,8,0,144,1,0,1,77,0,83,0,32,0,83,
			0,104,0,101,0,108,0,108,0,32,0,68,0,108,0,103,0,0,0,0,0,0,0,0,0,
			0,0,0,0,2,0,2,88,7,0,254,0,81,1,8,0,255,255,255,255,255,255,130,0,46,
			0,46,0,97,0,98,0,111,0,114,0,116,0,32,0,97,0,99,0,116,0,105,0,111,0,
			110,0,32,0,98,0,121,0,32,0,99,0,108,0,111,0,115,0,105,0,110,0,103,0,32,
			0,116,0,104,0,105,0,115,0,32,0,119,0,105,0,110,0,100,0,111,0,119,0,46,0,
			46,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,196,8,177,80,7,0,7,0,81,
			1,246,0,235,3,0,0,255,255,129,0,0,0,0,0
		};
		return (LPDLGTEMPLATE)dialogResource;
	}

	static void windowThreadFunc(void* _thisReporter)
	{
		DialogBoxIndirectParam(GetModuleHandle(NULL),getDialogResource(),NULL,DlgProc,(LPARAM)_thisReporter);
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

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
			case WM_INITDIALOG:
				{
					RRReporterWindow* thisReporter = (RRReporterWindow*)lParam;
					RR_ASSERT(thisReporter);
					thisReporter->hWnd = hWnd;
					SetWindowLongPtr(hWnd,GWLP_USERDATA,(LONG_PTR)thisReporter);
				}
				return (INT_PTR)TRUE;

			case WM_SHOWWINDOW:
				{
					RRReporterWindow* thisReporter = (RRReporterWindow*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
					RR_ASSERT(thisReporter);
					thisReporter->shown = true;
				}
				break;

			case WM_COMMAND:
				if (LOWORD(wParam)==IDCANCEL)
				{
					// Request abort, but do not close this window yet.
					// It will close only when user manually deletes window.
					RRReporterWindow* thisReporter = (RRReporterWindow*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
					RR_ASSERT(thisReporter);
					if (thisReporter->exitCallback)
					{
						thisReporter->exitCallback->run();
					}
					return (INT_PTR)TRUE;
				}
				break;
		}
		return (INT_PTR)FALSE;
	}

	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		// indentation
		char space[1000];
		space[0] = 0;
		indentation *= 2;
		if (indentation>0 && indentation<999)
		{
			memset(space,' ',indentation);
			space[indentation] = 0;
		}
		// type
		if (type<rr::ERRO || type>rr::TIMI) type = rr::INF9;
		static const char* typePrefix[] = {"ERROR: ","Assertion failed: ","Warning: ","","","","","",""};
		strcat(space,typePrefix[type]);
		// message
		strcat(space,message);
		// crlf
		space[strlen(space)-1]=0;
		strcat(space,"\r\n");
		// send
		int pos = GetWindowTextLength(GetDlgItem(hWnd,IDC_LOG));
		SendDlgItemMessageA(hWnd,IDC_LOG,EM_SETSEL,(pos>29000)?0:pos,pos);
		SendDlgItemMessageA(hWnd,IDC_LOG,EM_REPLACESEL,(WPARAM)FALSE,(LPARAM)space);
		SendDlgItemMessageA(hWnd,IDC_LOG,WM_VSCROLL,SB_BOTTOM,0);
	}

	virtual ~RRReporterWindow()
	{
		EndDialog(hWnd, 0);
	}

private:
	HWND hWnd;
	RRCallback* exitCallback;
	bool shown;
};

/*
RRReporter* RRReporter::createWindowedReporter(RRCallback* abortCallback)
{
#ifdef _WIN32
	return new RRReporterWindow(abortCallback);
#else
	return NULL;
#endif
}
*/


/////////////////////////////////////////////////////////////////////////////
//
// RRReporterWindowAbortSolver
//
// RRReporterWindow with abort callback that sets solver->aborting

class Abort : public RRCallback
{
public:
	// This is called from main thread when work begins.
	Abort(rr::RRDynamicSolver** _solver)
	{
		solver = _solver;
	}
	// This is called from any other thread when user requests abort (e.g. by closing log window).
	// It is important to have solver set, otherwise abort has no effect.
	virtual void run()
	{
		// Ask solver to abort all actions.
		if (solver[0])
		{
			rr::RRReporter::report(rr::INF2,"******** abort ********\n");
			solver[0]->aborting = true;
		}
	}
	// This is called from main thread when work is done (finished or aborted).
	~Abort()
	{
		// Stop aborting solver functions.
		if (solver[0])
		{
			solver[0]->aborting = false;
		}
	}
private:
	RRDynamicSolver** solver;
};

class RRReporterWindowAbortSolver : public RRReporterWindow
{
public:
	RRReporterWindowAbortSolver(class RRDynamicSolver** _solver)
		: abort(_solver), RRReporterWindow(&abort)
	{
	}
private:
	Abort abort;
};

RRReporter* RRReporter::createWindowedReporter(class RRDynamicSolver*& _solver)
{
	return new RRReporterWindowAbortSolver(&_solver);
}

} //namespace

#else // _WIN32

rr::RRReporter* rr::RRReporter::createWindowedReporter(rr::RRDynamicSolver*& _solver)
{
	return NULL;
}

#endif
