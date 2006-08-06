#ifndef RRTIMER_H
#define RRTIMER_H

//////////////////////////////////////////////////////////////////////////////
// Simple timer for time interval measurement.
//
// Measures real time, processor time spent in userspace and processor time spent in kernel.
// Note that processor time may be higher than real time on multicore/processor machines.
//
// Lightsprint, 2006
//////////////////////////////////////////////////////////////////////////////

#include <cassert>

#ifdef _WIN32
	#define WINDOWS_TIME
	#include <windows.h>
	#define TIME    DWORD
	#define GETTIME GetTickCount()
	#define PER_SEC 1000
#else
	#define TIME    clock_t
	#define GETTIME clock()
	#define PER_SEC CLOCKS_PER_SEC
#endif

namespace rr
{

	class RRTimer
	{
	public:

		RRTimer()
		{
#ifdef WINDOWS_TIME
			QueryPerformanceFrequency((LARGE_INTEGER *)&perffreq);
			proc = GetCurrentProcess();
#endif
		}

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

		double Watch(double* usertime=NULL, double* kerneltime=NULL) // user time from Start in 100ns units, approx 1ms precision in windows xp
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

} // namespace

#endif
