// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - log.
// --------------------------------------------------------------------------

#ifndef SVLOG_H
#define SVLOG_H

#include "Lightsprint/Ed/Ed.h"
#include "SVFrame.h"
#include <queue>

namespace rr_ed
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
		LogWithAbort(wxWindow* window, rr_gl::RRSolverGL*& solver, const char* caption);
		~LogWithAbort();
		static bool logIsOn;
	private:
		bool enabled;
		wxWindow* window;
		rr::RRReporter* localReporter;
	};

}; // namespace

#endif
