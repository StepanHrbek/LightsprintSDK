#ifndef RRDEBUG_H
#define RRDEBUG_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRDebug.h
//! \brief LightsprintCore | tools for debugging
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2011
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cstdarg>
#include "RRMemory.h"

#define RR_LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if (times_done<times_max) {times_done++;action;}}
#define RR_STATIC_ASSERT(constant_expression,message) typedef char __C_ASSERT__[(constant_expression) ? 1 : -1]
#ifdef _DEBUG
	#define RR_ASSERT(a) {if (!(a)) RR_LIMITED_TIMES(10,rr::RRReporter::assertionFailed(#a,__FUNCTION__,__FILE__,__LINE__));}
#else
	#define RR_ASSERT(a) 0
#endif

namespace rr
{

	//! Type of reported message.
	//
	//! Custom reporters may use this information for example to
	//! - filter messages
	//! - layout messages
	//! - process important messages differently
	enum RRReportType
	{
		ERRO, ///< Error, most important message, describes problem you should immediately fix. Reported by all versions.
		ASSE, ///< Assertion failure - error or warning of uncommon circumstances. The biggest group of messages, reported only in debug version.
		WARN, ///< Warning, potential error. Reported by all versions.
		INF1, ///< Information, produced by valid programs. Reported by all versions. Rare and important events.
		INF2, ///< Information, produced by valid programs. Reported by all versions. Medium importance.
		INF3, ///< Information, produced by valid programs. Reported by all versions. Frequent or unimportant events.
		INF9, ///< So unimportant that it's to be skipped.
		TIMI, ///< Timing information.
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Reporting messages
	//
	//! When you or Lightsprint internals call static RRReporter::report(),
	//! message is passed to all existing reporters.
	//! At the beginning, there are no reporters, messages are lost.
	//! If you encounter problems, it could help to create reporter and
	//! check messages.
	//!
	//! Thread safe: yes with exception,
	//!   multiple threads may report at once,
	//!   multiple threads may create/delete reporters at once,
	//!   but you must not create/delete reporters and report at the same time (very unlikely case).
	//! All new implementations must be at least this thread safe too.
	//
	//////////////////////////////////////////////////////////////////////////////
	class RR_API RRReporter : public RRUniformlyAllocatedNonCopyable
	{
	public:
		/////////////////////////////////////////////////////////////
		// interface
		/////////////////////////////////////////////////////////////

		//! Generic report of message. The only function you implement in custom reporter.
		//
		//! It is usually called by Lightsprint internals with message for you.
		//! \param type
		//!  Type of message.
		//! \param indentation
		//!  Indentation for structured messages.
		//!  Submessages (e.g. messages from subtask) receive parent indentation + 1.
		//!  Default indentation is 0.
		//!  Current indentation is modified by indent().
		//! \param message
		//!   Message converted to plain text.
		//!
		//! Thread safe: yes, correct implementation must be thread safe.
		virtual void customReport(RRReportType type, int indentation, const char* message) = 0;

		RRReporter();
		virtual ~RRReporter();

		/////////////////////////////////////////////////////////////
		// tools
		/////////////////////////////////////////////////////////////

		//! Shortcut for customReport() with printf syntax.
		//! Usually called by Lightsprint internals with message for you.
		static void report(RRReportType type, const char* format, ...);

		//! Shortcut for customReport() with vprintf syntax.
		static void reportV(RRReportType type, const char* format, va_list& vars);

		//! Modifies indentation of future reports. +1 extends whitespace size, -1 reduces whitespace.
		static void indent(int delta);

		//! Shortcut for special type of message: assertion failure.
		//! Usually called by Lightsprint debug version internals.
		//! Assertion failures are not reported in release version.
		static void assertionFailed(const char* expression, const char* func, const char* file, unsigned line);

		//! Sets filter for reported messages.
		//
		//! \param warnings
		//!  Enables processing of warning messages (WARN).
		//! \param infLevel
		//!  Enables processing of INFx messages for x<=infLevel. E.g. 1 enables only the most important INF1 messages,
		//!  2 enables also less important INF2 messages.
		//!  It's good to set at least level 1, preferrably 2, because some error messages could make better sense
		//!  in context of surrounding info messages.
		//! \param timing
		//!  Enables processing of timing messages (TIMI).
		static void setFilter(bool warnings = true, unsigned infLevel = 2, bool timing = true);

		//! Creates reporter that logs all messages to plain text file.
		//! Optional caching makes writes faster but last messages may be lost when program crashes.
		static RRReporter* createFileReporter(const char* filename, bool caching);

		//! Creates reporter that calls printf() on each message, with priority highlighting.
		static RRReporter* createPrintfReporter();

		//! Creates reporter that calls OutputDebugString() on each message.
		static RRReporter* createOutputDebugStringReporter();

		//! Creates reporter with its own window to display messages.
		//
		//! Supported only in Windows.
		//! If user tries to close the window manually, solver->aborting is set; it's cleared later in reporter destructor.
		//! Window is closed when both user attempts to close the window and you delete returned reporter.
		//! This means that window may exist even when you delete reporter.
		//! \n Usage example: \code
		//! // create window
		//! RRReporter* reporter = RRReporter::createWindowedReporter(solver);
		//! // do any work here, it is logged to window, may be aborted
		//! solver->updateLightmaps();
		//! // you don't have to delete solver here, but if you do, do it safely, reporter still references solver.
		//! // plain 'delete solver;' would make reporter destructor write to freed memory!
		//! RR_SAFE_DELETE(solver);
		//! // leave window, set current reporter to NULL. window passively exists until user closes it
		//! delete reporter;
		//! // here solver is no longer referenced (even if window may still exist)
		//! \endcode
		//! \param solver
		//!  Pointer to solver. It may be NULL or non-NULL, you are free to change it at any moment, but restrictions exist:
		//!  - The pointer must exist at least as long as the reporter,
		//!    make sure that you don't pass local pointer that is going out of scope before you delete the reporter.
		//!  - The pointer must be NULL or point to valid solver, make sure that you NULL it when you delete the solver.
		//! \param caption
		//!  Optional custom window caption, NULL for default one.
		//! \param closeWhenDone
		//!  Closes window immediately after reporting ends, rather than asking user what to do.
		static RRReporter* createWindowedReporter(class RRDynamicSolver*& solver, const char* caption = NULL, bool closeWhenDone = false);

		//! Helper, converts number of bytes to human readable string, e.g. 12345678 to "12 MB".
		static const char* bytesToString(size_t bytes);
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Accurate timer.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRTime
	{
	public:
		//! By default, constructor updates time to current time. You can pass false to keep time uninitialized, but then don't call any other time function before setNow(), results would be random.
		explicit RRTime(bool setNow = true);
		//! Updates this time, "this = now".
		void setNow();
		//! Modifies this time, "this += seconds".
		void addSeconds(float seconds);
		//! Returns number of seconds passed, "now - this".
		float secondsPassed() const;
		//! Returns number of seconds since a, "this - a".
		float secondsSince(const RRTime& a) const;
		//! Returns number of seconds passed, "now - this", and updates this time, "this = now".
		float secondsSinceLastQuery();
	private:
		unsigned char data[16];
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Reporting events with time taken
	//
	//! Helper for reporting event with automatic indented subevents, reported duration.
	//! Simply create instance of RRReportInterval to report event.
	//! When instance dies, end of event is reported automatically.
	//
	//////////////////////////////////////////////////////////////////////////////
	
	class RR_API RRReportInterval
	{
	public:
		//! Reports information and increases indentation.
		RRReportInterval(RRReportType type, const char* format, ...);
		//! Reports time elapsed since constructor call and decreases indentation.
		~RRReportInterval();
	protected:
		bool enabled;
		RRTime time;
	};


	//! Will be described later.
	RR_API bool offlineSetup(bool& enabled, unsigned& quality, bool finalQuality);


} // namespace

#endif
