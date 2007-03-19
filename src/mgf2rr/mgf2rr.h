//
// mgf to rrobject loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _MGF2RR_H
#define _MGF2RR_H

#include "Lightsprint/RRVision.h"

// you may load object from mgf
rr::RRObject* new_mgf_importer(char* filename);

// and you may set direct exiting flux (not present in file) later
// (direct means that flux was caused by spotlight shining on face and face reflects part of power)
//void set_direct_exiting_flux(rr::RRObject* importer, unsigned triangle, const rr::RRColor* exitingFlux);

#endif
