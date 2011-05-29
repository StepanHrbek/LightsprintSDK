// --------------------------------------------------------------------------
// Accurate timing.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#if defined(LINUX) || defined(linux)

	// 1ns precision
	// in linux: correct
	#include <time.h>
	#define TIME          timespec
	#define GETTIME(t)    clock_gettime(CLOCK_MONOTONIC, &t)
	#define SUBTRACT(a,b) ((a.tv_sec-b.tv_sec)+(a.tv_nsec-b.tv_nsec)*1e-9f)
	#define ADD(t,s)      {int is=floor(s+t.tv_nsec*1e-9f);t.tv_sec+=is;t.tv_nsec+=(s-is)*1e9f;}

#elif !defined(_WIN32)

	// 1us precision
	// in osx: correct
	#include <sys/time.h>
	#define TIME          timeval
	#define GETTIME(t)    gettimeofday(&t,NULL);
	#define SUBTRACT(a,b) ((a.tv_sec-b.tv_sec)+(a.tv_usec-b.tv_usec)*1e-6f)
	#define ADD(t,s)      {int is=floor(s+t.tv_usec*1e-6f);t.tv_sec+=is;t.tv_usec+=(s-is)*1e6f;}

#elif defined(_OPENMP)

	// precision decreases when system runs longer
	// in linux, osx: sometimes runs slower than real time
	#include <omp.h>
	#include <cstring> // NULL
	#define TIME          double
	#define GETTIME(t)    t=omp_get_wtime()
	#define SUBTRACT(a,b) (a-b)
	#define ADD(t,s)      t+=s

#elif defined(_WIN32)

	// 1-10ms precision
	#define WINDOWS_TIME
	#include <windows.h>
	#include <mmsystem.h> // might be necessary with WIN32_LEAN_AND_MEAN
	#define TIME          DWORD
	#define GETTIME(t)    t=timeGetTime()
	#define SUBTRACT(a,b) (int(a-b)*1e-3f)
	#define ADD(t,s)      t+=s*1e3f
	#ifdef _MSC_VER
		#pragma comment(lib,"winmm.lib")
	#endif

#else

	// 1-16ms precision
	// in linux, osx: sometimes runs slower than real time
	#include <ctime>
	#define TIME          clock_t
	#define GETTIME(t)    t=clock()
	#define SUBTRACT(a,b) (int(a-b)/(float)CLOCKS_PER_SEC)

#endif

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Timer

#define time (*(TIME*)data)

RRTime::RRTime(bool _setNow)
{
	RR_STATIC_ASSERT(sizeof(TIME)<=sizeof(data),"insufficient space for timer");
	if (_setNow) setNow();
}

void RRTime::setNow()
{
	GETTIME(time);
}

void RRTime::addSeconds(float seconds)
{
	ADD(time,seconds);
}

float RRTime::secondsSince(const RRTime& a) const
{
	return SUBTRACT(time,(*(TIME*)a.data));
}

float RRTime::secondsSinceLastQuery()
{
	TIME now;
	GETTIME(now);
	float result = SUBTRACT(now,time);
	time = now;
	return result;
}

float RRTime::secondsPassed() const
{
	TIME now;
	GETTIME(now);
	return SUBTRACT(now,time);
}

} // namespace rr
