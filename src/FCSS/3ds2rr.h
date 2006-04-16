//
// 3ds to rrobject loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "RRVisionApp.h"
#include "Model_3DS.h"

// you may load object from mgf
void new_3ds_importer(Model_3DS* model,rrVision::RRVisionApp* app);

// and you may set direct exiting flux (not present in file) later
// (direct means that flux was caused by spotlight shining on face and face reflects part of power)
//void set_direct_exiting_flux_3ds(rrVision::RRObjectImporter* importer, unsigned triangle, const rrVision::RRColor* exitingFlux);

#endif
