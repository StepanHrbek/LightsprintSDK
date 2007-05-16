#ifndef RRDEBUG_H
#define RRDEBUG_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRDebug.h
//! \brief RRDebug - tools for debugging
//! \version 2007.5.14
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRMesh.h"

#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}

#ifdef _DEBUG
	#define RR_ASSERT(a) {if(!(a)) LIMITED_TIMES(10,rr::RRReporter::assertionFailed(#a,__FUNCTION__,__FILE__,__LINE__));}
#else
	#define RR_ASSERT(a) 0
#endif

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Reporting messages
	//
	//! This system is used by Lightsprint internals to send
	//! messages to you.
	//! 
	//! By default, all messages are ignored.
	//! If you encounter problems, it could help to 
	//! set reporter and read system messages.
	//!
	//! Thread safe: yes, may be accessed by any number of threads simultaneously.
	//! All new implementations must be thread safe too.
	//
	//////////////////////////////////////////////////////////////////////////////
	class RR_API RRReporter
	{
	public:
		/////////////////////////////////////////////////////////////
		// interface
		/////////////////////////////////////////////////////////////

		//! Type of message.
		enum Type
		{
			ERRO, ///< Error, most important message, describes problem you should immediately fix. Reported by all versions.
			ASSE, ///< Assertion failure - error or warning of uncommon circumstances. The biggest group of messages, reported only in debug version.
			WARN, ///< Warning, potential error. Reported by all versions.
			INFO, ///< Information, produced by valid programs. Reported by all versions.
			CONT, ///< Continuation of previous message.
		};

		//! Generic report of message.
		//! Usually called by Lightsprint internals with message for you.
		//!
		//! Thread safe: yes, correct implementation must be thread safe.
		virtual void customReport(Type type, const char* message) = 0;


		/////////////////////////////////////////////////////////////
		// tools
		/////////////////////////////////////////////////////////////

		//! Shortcut for customReport() with printf syntax.
		//! Usually called by Lightsprint internals with message for you.
		static void report(Type type, const char* format, ...);

		//! Shortcut for special type of message: assertion failure.
		//! Usually called by Lightsprint debug version internals.
		//! Assertion failures are not reported in release version.
		static void assertionFailed(const char* expression, const char* func, const char* file, unsigned line);

		//! Sets custom reporter, NULL for none.
		static void setReporter(RRReporter* reporter);

		//! Returns current active reporter, NULL for none.
		static RRReporter* getReporter();

		//! Creates reporter that calls printf() on each message.
		static RRReporter* createPrintfReporter();

		//! Creates reporter that calls OutputDebugString() on each message.
		static RRReporter* createOutputDebugStringReporter();
	};

} // namespace

#endif
