// StopWatch for time interval measurement.
// Windows only implementation.
//
// Lightsprint, 2006

#include <assert.h>
#include <windows.h>

class StopWatch
{
public:
	StopWatch()
	{
		proc = GetCurrentProcess();
	}
	void Start()
	{
		MyFILETIME creationtime,exittime;
		if(!GetProcessTimes(proc,&creationtime.ft,&exittime.ft,&kerneltime1.ft,&usertime1.ft))
		{
			assert(0);
		}
		realtime1 = GetTickCount();
	};
	double Watch() // user time from Start in 100ns units, approx 1ms precision in windows xp
	{
		MyFILETIME creationtime,exittime,kerneltime2,usertime2;
		if(!GetProcessTimes(proc,&creationtime.ft,&exittime.ft,&kerneltime2.ft,&usertime2.ft))
		{
			assert(0);
		}
		realtime = (GetTickCount()-realtime1)*1e-3;
		kerneltime = (kerneltime2.u-kerneltime1.u)*1e-7;
		usertime = (usertime2.u-usertime1.u)*1e-7;
		return usertime;
	};
	double kerneltime; // sec
	double usertime; // sec
	double realtime; // sec
private:
	typedef unsigned __int64 uint64_t;
	union MyFILETIME {FILETIME ft; uint64_t u;};
	HANDLE proc;
	MyFILETIME kerneltime1,usertime1;
	DWORD realtime1;
};
