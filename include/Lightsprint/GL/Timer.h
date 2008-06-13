//----------------------------------------------------------------------------
//! \file Timer.h
//! \brief LightsprintGL | measures real time, processor time in userspace and in kernel
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef TIMER_H
#define TIMER_H

#include <cassert>

#ifdef _OPENMPxxx // delete xxx to enable

	// GETTIME: 1us precision, slow,  Timer: low precision
	// on linux: sometimes runs slower than real time
	#include <omp.h>
	#include <cstring> // NULL
	#define TIME    double
	#define GETTIME omp_get_wtime()
	#define PER_SEC 1

#elif defined(_WIN32xxx) // delete xxx to enable

	// GETTIME: 1ms precision,  Timer: high precision
	#define WINDOWS_TIME
	#include <windows.h>
	#define TIME    DWORD
	#define GETTIME timeGetTime()
	#define PER_SEC 1000
	#ifdef _MSC_VER
		#pragma comment(lib,"winmm.lib")
	#endif

#elif defined(LINUX) || defined(linux)

	// GETTIME: 1 ns precision, artificially reduced to 1 ms
	// on linux: correct
	#include <time.h>
	#define TIME unsigned long long
	#define GETTIME getTime()
	#define PER_SEC 1000

	inline unsigned long long getTime()
	{
		timespec t;
		clock_gettime(CLOCK_MONOTONIC, &t);
		return t.tv_sec * 1000 + (t.tv_nsec + 500000) / 1000000;
	}

#else

	// GETTIME: 16ms precision on windows, fast,  Timer: low precision
	// on linux: sometimes runs slower than real time
	#include <ctime>
	#define TIME    clock_t
	#define GETTIME clock()
	#define PER_SEC CLOCKS_PER_SEC

#endif

#define GETSEC ((double)(GETTIME/(double)PER_SEC))

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Timer

//! Timer for measuring intervals.
class Timer
{
public:

	//! Creates timer, but doesn't start counting.
	Timer()
	{
#ifdef WINDOWS_TIME
		QueryPerformanceFrequency((LARGE_INTEGER *)&perffreq);
		proc = GetCurrentProcess();
#endif
	}

	//! Starts measuring.
	void Start()
	{
#ifdef WINDOWS_TIME
		MyFILETIME creationtime,exittime;
		if(!GetProcessTimes(proc,&creationtime.ft,&exittime.ft,&kernelstart.ft,&userstart.ft))
		{
			assert(0);
		}
		if(perffreq)
		{
			QueryPerformanceCounter((LARGE_INTEGER *)&perfstart);
		}
		else
#endif
		{
			timestart = GETTIME;
		}
	};

	//! Returns real time spent from Start.
	//! Optionally fills also processor times in user/kernel space.
	//! Note that user+kernel time may be higher than real time on multicore/processor machines.
	double Watch(double* usertime=NULL, double* kerneltime=NULL)
	{
#ifdef WINDOWS_TIME
		if(usertime || kerneltime)
		{
			MyFILETIME creationtime,exittime,kerneltime2,usertime2;
			if(!GetProcessTimes(proc,&creationtime.ft,&exittime.ft,&kerneltime2.ft,&usertime2.ft))
			{
				assert(0);
			}
			if(kerneltime) *kerneltime = (kerneltime2.u-kernelstart.u)*1e-7;
			if(usertime) *usertime = (usertime2.u-userstart.u)*1e-7;
		}
		if(perffreq)
		{
			__int64 perftime;
			QueryPerformanceCounter((LARGE_INTEGER *)&perftime);
			return (perftime-perfstart)/(double)perffreq;
		}
#endif
		return (GETTIME-timestart)/(double)PER_SEC;
	};

private:
#ifdef WINDOWS_TIME
	typedef unsigned __int64 uint64_t;
	union MyFILETIME {FILETIME ft; uint64_t u;};
	HANDLE proc;
	MyFILETIME kernelstart,userstart;
	__int64 perffreq, perfstart;
#endif
	TIME timestart;
};

}; // namespace

#endif
