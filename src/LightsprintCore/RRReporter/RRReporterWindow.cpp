// --------------------------------------------------------------------------
// Reporting messages.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#ifdef _WIN32

#include <ctime> // time
#include <cstdio> // sprintf
#include <process.h> // _beginthread
#include <windows.h>
#include <richedit.h> // setting text color
#include "reporterWindow.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "../Preferences.h"

#define LOCATION "WindowedReporter"

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
	// data that live with window instance
	// when reporter is deleted, window and instance data may still exist
	struct InstanceData
	{
		bool shown; // set by WM_SHOWWINDOW
		bool abortRequested; // user attemted to close window
		bool reporterDeleted; // code deleted reporter
		HWND* hWndInReporter; // pointer to HWND in reporter; set at window open so reporter can send us messages
		RRCallback* abortCallback; // <deleted with reporter>
		unsigned numLines[TIMI+1];

		unsigned numCharsShort; // number of characters in short log edit control
		unsigned numCharsDetailed; // number of characters in detailed log edit control
	};

	// what to do when work is done = when reporter is deleted
	enum WhenDone
	{
		WD_CLOSE = 0, // close window and continue
		WD_WAIT,      // wait until user closes the window
		WD_CONTINUE,  // continue, keep window open
	};

public:
	RRReporterWindow(RRCallback* _abortCallback)
	{
		// necessary for changing text color
		LoadLibrary("riched20.dll");

		hWnd = 0;
		// instance data are allocated here, deleted at the end of thread life
		InstanceData* instanceData = new InstanceData;
		memset(instanceData,0,sizeof(InstanceData));
		instanceData->hWndInReporter = &hWnd;
		instanceData->abortCallback = _abortCallback;
		_beginthread(windowThreadFunc,0,instanceData);

		// Wait until window initializes.
		// It is necessary: in case of immediate end, first messages would be sent to uninitialized window and lost.
		while (!instanceData->shown) Sleep(1);
		time_t t = time(NULL);
		localReport(INF1,"STARTED %s",asctime(localtime(&t)));

		// When helper thread opens dialog, main thread loses right to create foreground windows.
		// Temporary fix: Main thread opens dialog that closes itself immediately.
		//                Must be done after first window receives WM_SHOWWINDOW.
		// Tested: XP SP2 32bit, Vista SP1 x64
		// If you know proper fix, please help.
		DialogBoxIndirect(GetModuleHandle(NULL),getDialogResource(),NULL,BringMainThreadToForeground);
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
		if (type<ERRO || type>TIMI) type = INF9;
		static const char* typePrefix[] = {"ERROR: ","Assertion failed: ","Warning: ",""                    ,""           ,""           ,""           ,""      };
		static COLORREF         color[] = {0x2222FF ,0x2222FF            ,0x2222FF   ,0                     ,0            ,0            ,0            ,0x777700};
		static DWORD          effects[] = {CFE_BOLD ,0                   ,0          ,CFE_BOLD|CFE_AUTOCOLOR,CFE_AUTOCOLOR,CFE_AUTOCOLOR,CFE_AUTOCOLOR,0       };
		strcat(space,typePrefix[type]);
		// statistics
		InstanceData* instanceData = (InstanceData*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
		RR_ASSERT(instanceData);
		if (instanceData)
		{
			instanceData->numLines[type]++;
		}
		// message
		strcat(space,message);
		// crlf
		space[strlen(space)-1]=0;
		strcat(space,"\r\n");
		
		// send to short log
		if (type!=INF2 && type!=INF3 && type!=TIMI)
		{
			int dlgItem = IDC_LOG_SHORT;
			int pos = instanceData->numCharsShort;//GetWindowTextLength(GetDlgItem(hWnd,dlgItem));
			SendDlgItemMessageA(hWnd,dlgItem,EM_SETSEL,pos,pos);
			instanceData->numCharsShort += (unsigned)strlen(space);

			CHARFORMAT cf;
			memset( &cf, 0, sizeof(CHARFORMAT) );
			cf.cbSize = sizeof(CHARFORMAT);
			cf.dwMask = (type==ERRO?CFM_BOLD:0) | CFM_COLOR;
			cf.dwEffects = effects[type];
			cf.crTextColor = color[type];
			SendDlgItemMessageA(hWnd,dlgItem,EM_SETCHARFORMAT,(WPARAM)SCF_SELECTION,(LPARAM)&cf);

			SendDlgItemMessageA(hWnd,dlgItem,EM_REPLACESEL,(WPARAM)FALSE,(LPARAM)space);
			SendDlgItemMessageA(hWnd,dlgItem,WM_VSCROLL,SB_BOTTOM,0);
		}

		// send to detailed log
		{
			int dlgItem = IDC_LOG_DETAILED;
			int pos = instanceData->numCharsDetailed;//GetWindowTextLength(GetDlgItem(hWnd,dlgItem));
			SendDlgItemMessageA(hWnd,dlgItem,EM_SETSEL,pos,pos);
			instanceData->numCharsDetailed += (unsigned)strlen(space);

			CHARFORMAT cf;
			memset( &cf, 0, sizeof(CHARFORMAT) );
			cf.cbSize = sizeof(CHARFORMAT);
			cf.dwMask = CFM_BOLD | CFM_COLOR;
			cf.dwEffects = effects[type];
			cf.crTextColor = color[type];
			SendDlgItemMessageA(hWnd,dlgItem,EM_SETCHARFORMAT,(WPARAM)SCF_SELECTION,(LPARAM)&cf);

			SendDlgItemMessageA(hWnd,dlgItem,EM_REPLACESEL,(WPARAM)FALSE,(LPARAM)space);
			SendDlgItemMessageA(hWnd,dlgItem,WM_VSCROLL,SB_BOTTOM,0);
		}
	}

	virtual ~RRReporterWindow()
	{
		time_t t = time(NULL);
		localReport(INF1,"FINISHED %s",asctime(localtime(&t)));
		SetWindowText(hWnd,"Lightsprint log [FINISHED]");
		SendDlgItemMessageA(hWnd,IDC_BUTTON_ABORT_CLOSE,WM_SETTEXT,0,(LPARAM)"Close");
		InstanceData* instanceData = (InstanceData*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
		RR_ASSERT(instanceData);
		if (instanceData)
		{
			if (instanceData->numLines[ERRO]) localReport(WARN," %d ERRORS\n",instanceData->numLines[ERRO]);
			if (instanceData->numLines[ASSE]) localReport(WARN," %d ASSERTS\n",instanceData->numLines[ASSE]);
			if (instanceData->numLines[WARN]) localReport(WARN," %d WARNINGS\n",instanceData->numLines[WARN]);
			instanceData->reporterDeleted = true;
			instanceData->abortCallback = NULL;

			// manual abort was requested in past (window stayed open because reporterDeleted = false)
			// now we finally reached "delete reporter"
			// call abort button again, this time it will save prefs and close window (becuase reporterDeleted = true)
			if (instanceData->abortRequested)
			{
				SendMessage(hWnd,WM_COMMAND,IDC_BUTTON_ABORT_CLOSE,0);
			}
			else
			// tasks completed with WD_CLOSE -> simulate manual close
			if ((WhenDone)SendDlgItemMessage(hWnd,IDC_WHENDONE,CB_GETCURSEL,0,0)==WD_CLOSE)
			{
				SendMessage(hWnd,WM_COMMAND,IDC_BUTTON_ABORT_CLOSE,0);
			}
			else
			// tasks completed with WD_WAIT -> wait for manual close
			if ((WhenDone)SendDlgItemMessage(hWnd,IDC_WHENDONE,CB_GETCURSEL,0,0)==WD_WAIT)
			{
				// we must not repeatedly query window state, window may be deleted while we sleep for 1 ms
				// so we install callback and let window notify us that it is closed
				class CloseWindowCallback : public RRCallback
				{
				public:
					bool closed;
					virtual void run() {closed=true;}
				};
				CloseWindowCallback window;
				window.closed = false;
				instanceData->abortCallback = &window;
				while (!window.closed)
				{
					Sleep(1);
				}
			}
		}
	}

private:
	static LPDLGTEMPLATE getDialogResource()
	{
		static unsigned char dialogResource[] =
		{
			1,0,255,255,0,0,0,0,0,0,0,0,200,10,192,128,6,0,0,0,0,0,97,1,13,
			1,0,0,0,0,76,0,105,0,103,0,104,0,116,0,115,0,112,0,114,0,105,0,110,0,
			116,0,32,0,108,0,111,0,103,0,0,0,8,0,144,1,0,1,77,0,83,0,32,0,83,
			0,104,0,101,0,108,0,108,0,32,0,68,0,108,0,103,0,0,0,0,0,0,0,0,0,
			0,0,0,0,196,8,177,80,1,0,1,0,95,1,252,0,235,3,0,0,82,0,105,0,99,
			0,104,0,69,0,100,0,105,0,116,0,50,0,48,0,87,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,1,0,1,80,34,1,0,1,62,0,12,0,241,3,0,0,255,255,128,
			0,65,0,98,0,111,0,114,0,116,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			3,0,1,80,1,0,0,1,59,0,12,0,243,3,0,0,255,255,128,0,68,0,101,0,116,
			0,97,0,105,0,108,0,101,0,100,0,32,0,108,0,111,0,103,0,0,0,0,0,0,0,
			0,0,0,0,0,0,196,8,177,64,1,0,1,0,95,1,252,0,236,3,0,0,82,0,105,
			0,99,0,104,0,69,0,100,0,105,0,116,0,50,0,48,0,87,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,3,0,33,80,132,0,0,1,64,0,99,0,248,3,0,0,255,
			255,133,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,80,87,0,2,1,42,0,
			10,0,255,255,255,255,255,255,130,0,87,0,104,0,101,0,110,0,32,0,100,0,111,0,110,
			0,101,0,44,0,0,0,0,0
		};
		return (LPDLGTEMPLATE)dialogResource;
	}

	static void windowThreadFunc(void* instanceData)
	{
		DialogBoxIndirectParam(GetModuleHandle(NULL),getDialogResource(),NULL,DlgProc,(LPARAM)instanceData);
		delete instanceData;
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
					InstanceData* instanceData = (InstanceData*)lParam;
					RR_ASSERT(instanceData);
					if (instanceData)
					{
						*instanceData->hWndInReporter = hWnd; // set at window birth
						instanceData->hWndInReporter = NULL; // we don't need it anymore
						SetWindowLongPtr(hWnd,GWLP_USERDATA,(LONG_PTR)instanceData);

						// load preferences
						bool detailed = Preferences::getValue(LOCATION,"detailed",0)!=0;
						SendDlgItemMessage(hWnd,IDC_CHECK_DETAILED,BM_SETCHECK,detailed?BST_CHECKED:BST_UNCHECKED,0);
						if (detailed)
						{
							ShowWindow(GetDlgItem(hWnd,IDC_LOG_DETAILED),SW_SHOWNORMAL);
							ShowWindow(GetDlgItem(hWnd,IDC_LOG_SHORT),SW_HIDE);
						}

						WhenDone whenDone = (WhenDone)(int)Preferences::getValue(LOCATION,"whendone",WD_CONTINUE);
						SendDlgItemMessage(hWnd,IDC_WHENDONE,CB_ADDSTRING,0,(LPARAM)"close log");
						SendDlgItemMessage(hWnd,IDC_WHENDONE,CB_ADDSTRING,0,(LPARAM)"wait");
						SendDlgItemMessage(hWnd,IDC_WHENDONE,CB_ADDSTRING,0,(LPARAM)"continue");
						SendDlgItemMessage(hWnd,IDC_WHENDONE,CB_SETCURSEL,whenDone,0);
					}
				}
				return (INT_PTR)TRUE;

			case WM_SHOWWINDOW:
				{
					InstanceData* instanceData = (InstanceData*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
					RR_ASSERT(instanceData);
					if (instanceData)
					{
						instanceData->shown = true;
					}
				}
				break;

			case WM_COMMAND:
				if (LOWORD(wParam)==IDCANCEL || LOWORD(wParam)==IDC_BUTTON_ABORT_CLOSE)
				{
					// Request abort, but do not necessarily close the window now.
					InstanceData* instanceData = (InstanceData*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
					RR_ASSERT(instanceData);
					if (instanceData)
					{
						if (instanceData->abortCallback)
						{
							instanceData->abortCallback->run();
						}
						instanceData->abortRequested = true;

						// close window only if reporter was already deleted
						if (instanceData->reporterDeleted)
						{
							// save preferences
							bool detailed = SendDlgItemMessage(hWnd,IDC_CHECK_DETAILED,BM_GETCHECK,0,0)==BST_CHECKED;
							Preferences::setValue(LOCATION,"detailed",detailed?1.f:0);
							WhenDone whenDone = (WhenDone)SendDlgItemMessage(hWnd,IDC_WHENDONE,CB_GETCURSEL,0,0);
							Preferences::setValue(LOCATION,"whendone",(float)whenDone);

							EndDialog(hWnd,0);
						}
					}
					return (INT_PTR)TRUE;
				}
				if (LOWORD(wParam)==IDC_CHECK_DETAILED)
				{
					bool detailed = SendDlgItemMessage(hWnd,IDC_CHECK_DETAILED,BM_GETCHECK,0,0)==BST_CHECKED;
					if (detailed)
					{
						ShowWindow(GetDlgItem(hWnd,IDC_LOG_DETAILED),SW_SHOWNORMAL);
						ShowWindow(GetDlgItem(hWnd,IDC_LOG_SHORT),SW_HIDE);
					}
					else
					{
						ShowWindow(GetDlgItem(hWnd,IDC_LOG_SHORT),SW_SHOWNORMAL);
						ShowWindow(GetDlgItem(hWnd,IDC_LOG_DETAILED),SW_HIDE);
					}
				}
				break;
		}

		return (INT_PTR)FALSE;
	}

	void localReport(RRReportType type, const char* format, ...)
	{
		va_list argptr;
		va_start (argptr,format);
		char msg[1000];
		_vsnprintf(msg,999,format,argptr);
		msg[999] = 0;
		RRReporterWindow::customReport(type,0,msg);
		va_end (argptr);
	}

	HWND hWnd;
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
	Abort(RRDynamicSolver** _solver)
	{
		solver = _solver;
		aborted = false;
	}
	// This is called from any other thread when user requests abort (e.g. by closing log window).
	// It is important to have solver set, otherwise abort has no effect.
	virtual void run()
	{
		// Ask solver to abort all actions.
		if (solver[0])
		{
			RRReporter::report(INF1,"******** abort ********\n");
			solver[0]->aborting = true;
			aborted = true;
		}
	}
	// This is called from main thread when work is done (finished or aborted).
	~Abort()
	{
		// Stop aborting solver functions.
		if (aborted && solver[0])
		{
			// This is very dangerous place.
			// It's common error in main program to 'delete solver' rather than RR_SAFE_DELETE(solver),
			// it makes our pointer invalid and we overwrite freed memory here.
			// TODO: enforce some form of smart pointer on solver to prevent this error
			solver[0]->aborting = false;
		}
	}
private:
	RRDynamicSolver** solver;
	bool aborted;
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
