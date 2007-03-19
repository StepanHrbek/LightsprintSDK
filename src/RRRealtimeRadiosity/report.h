#ifndef REPORT_H
#define REPORT_H

#include "Lightsprint/DemoEngine/Timer.h"

#define REPORT(a)        //a
#define REPORT_INIT      REPORT( Timer timer; )
#define REPORT_BEGIN(a)  REPORT( timer.Start(); reportAction(a ".."); )
#define REPORT_END       REPORT( {char buf[20]; sprintf(buf," %d ms.\n",(int)(timer.Watch()*1000));reportAction(buf);} )
#define reportAction     printf

#endif
