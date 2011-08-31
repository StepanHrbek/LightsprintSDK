// --------------------------------------------------------------------------
// Scene viewer - log.
// Copyright (C) 2009-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVLOG_H
#define SVLOG_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVFrame.h"

namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVLog - permanent dockable window with complete log, wx based

	class SVLog : public wxTextCtrl, public rr::RRReporter
	{
	public:
		SVLog(SVFrame* svframe);
		virtual void customReport(rr::RRReportType type, int indentation, const char* message);
		void flushQueue();
		~SVLog();

	private:
		wxCriticalSection critSec;
		wxTextAttr attr[rr::TIMI+1];
		void customReportImmediate(rr::RRReportType type, int indentation, const char* message);
		struct SavedLog
		{
			rr::RRReportType type;
			int indentation;
			const char* message;
		};
		std::queue<SavedLog> queue;
	};


	/////////////////////////////////////////////////////////////////////////////
	//
	// LogWithAbort - temporary log window with local log and abort button, winapi based

	class LogWithAbort
	{
	public:
		LogWithAbort(wxWindow* window, RRDynamicSolverGL*& solver, const char* caption);
		~LogWithAbort();
		static bool logIsOn;
	private:
		bool enabled;
		wxWindow* window;
		rr::RRReporter* localReporter;
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
