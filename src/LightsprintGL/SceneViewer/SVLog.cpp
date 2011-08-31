// --------------------------------------------------------------------------
// Scene viewer - log.
// Copyright (C) 2009-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVLog.h"
#include "SVFrame.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// SVLog

SVLog::SVLog(SVFrame* _svframe)
	: wxTextCtrl(_svframe, wxID_ANY, "", wxDefaultPosition, wxSize(300,300), wxTE_MULTILINE|wxTE_AUTO_URL|wxTE_READONLY|SV_SUBWINDOW_BORDER)
{
//	SetWindowStyle(SV_SUBWINDOW_BORDER);
	for (unsigned i=rr::ERRO;i<=rr::TIMI;i++)
	{
		attr[i].SetTextColour(*wxBLACK);
		attr[i].SetFontWeight(wxFONTWEIGHT_NORMAL);
	}
	attr[rr::ERRO].SetTextColour(*wxRED);
	attr[rr::ERRO].SetFontWeight(wxFONTWEIGHT_BOLD);
	attr[rr::WARN].SetTextColour(*wxRED);
	attr[rr::ASSE].SetTextColour(*wxRED);
	attr[rr::INF1].SetFontWeight(wxFONTWEIGHT_BOLD);
	attr[rr::TIMI].SetTextColour(wxColour(128,128,0));
}

void SVLog::customReportImmediate(rr::RRReportType type, int indentation, const char* message)
{
	// called only from main thread
	SetDefaultStyle(attr[type]);
	AppendText(wxString::Format("%*s%s",2*indentation,"",message));
}

void SVLog::flushQueue()
{
	if (!queue.empty() && IsShownOnScreen())
	{
		// pop: called only from main thread
		wxCriticalSectionLocker locker(critSec);
		while (queue.size())
		{
			SavedLog& sl = queue.front();
			customReportImmediate(sl.type,sl.indentation,sl.message);
			free((char*)sl.message);
			queue.pop();
		}
	}
}

void SVLog::customReport(rr::RRReportType type, int indentation, const char* message)
{
	if (type<rr::ERRO || type>rr::TIMI) type = rr::INF9;
	if (!wxIsMainThread() || !IsShownOnScreen())
	{
		SavedLog sl;
		sl.type = type;
		sl.indentation = indentation;
		sl.message = _strdup(message);
		// push: may be called from multiple non-main threads at once
		wxCriticalSectionLocker locker(critSec);
		queue.push(sl);
		return;
	}
	flushQueue();
	customReportImmediate(type,indentation,message);
}

SVLog::~SVLog()
{
	while (queue.size())
	{
		free((char*)queue.front().message);
		queue.pop();
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// LogWithAbort

bool LogWithAbort::logIsOn = false;

LogWithAbort::LogWithAbort(wxWindow* _window, RRDynamicSolverGL*& _solver, const char* _caption)
{
	enabled = !logIsOn; // do nothing if log is already enabled
	if (enabled)
	{
		// display log window with 'abort'
		logIsOn = true;
		window = _window;
		localReporter = rr::RRReporter::createWindowedReporter(*(rr::RRDynamicSolver**)&_solver,_caption,true);
	}
}

LogWithAbort::~LogWithAbort()
{
	if (enabled)
	{
		// restore old reporter, close log
		logIsOn = false;
		delete localReporter;
		// When windowed reporter shuts down, z-order changes (why?), SV drops below toolbench.
		// This bring SV back to front.
		window->Raise();
	}
}

}; // namespace

#endif // SUPPORT_SCENEVIEWER
