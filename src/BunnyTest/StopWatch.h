#include <assert.h>
#include <windows.h>

class StopWatch
{
public:
	typedef unsigned __int64 uint64_t;
	union MyFILETIME {FILETIME ft; uint64_t u;};
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
	};
	uint64_t Watch() // user time from Start in 100ns units, approx 1ms precision in windows xp
	{
		MyFILETIME creationtime,exittime,kerneltime2,usertime2;
		if(!GetProcessTimes(proc,&creationtime.ft,&exittime.ft,&kerneltime2.ft,&usertime2.ft))
		{
			assert(0);
		}
		kerneltime = kerneltime2.u-kerneltime1.u;
		usertime = usertime2.u-usertime1.u;
		return usertime;
	};
	uint64_t kerneltime; // in 100ns units
	uint64_t usertime; // in 100ns units
private:
	HANDLE proc;
	MyFILETIME kerneltime1,usertime1;
};
