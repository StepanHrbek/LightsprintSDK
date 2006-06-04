//
// 3ds to rrobject loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "RRIllumCalculator.h"
#include "Model_3DS.h"

// you may load object from mgf
void new_3ds_importer(Model_3DS* model,rr::RRVisionApp* app);

// and you may set direct exiting flux (not present in file) later
// (direct means that flux was caused by spotlight shining on face and face reflects part of power)
//void set_direct_exiting_flux_3ds(rr::RRObject* importer, unsigned triangle, const rr::RRColor* exitingFlux);

#endif
