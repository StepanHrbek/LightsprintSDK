#ifndef REPORT_H
#define REPORT_H

#include "Lightsprint/DemoEngine/Timer.h"

#ifdef _DEBUG
	#define REPORT(a)    a
#else
	#define REPORT(a)
#endif
#define REPORT_INIT      REPORT( de::Timer timer; )
#define REPORT_BEGIN(a)  REPORT( timer.Start(); RRReporter::report(RRReporter::INFO,a ".."); )
#define REPORT_END       REPORT( RRReporter::report(RRReporter::CONT," %d ms.\n",(int)(timer.Watch()*1000)) )

#endif
