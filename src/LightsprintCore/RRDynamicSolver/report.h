#ifndef REPORT_H
#define REPORT_H

#include "Lightsprint/DemoEngine/Timer.h"

#ifdef _DEBUG
	#define REPORT(a)    a
#else
	#define REPORT(a)
#endif
#define REPORT_INIT      REPORT( rr_gl::Timer timer; )
#define REPORT_BEGIN(a)  REPORT( timer.Start(); rr::RRReporter::report(rr::RRReporter::INFO,a ".."); )
#define REPORT_END       REPORT( rr::RRReporter::report(rr::RRReporter::CONT," %d ms.\n",(int)(timer.Watch()*1000)) )

#endif
