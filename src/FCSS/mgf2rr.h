//
// mgf to rrobject loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _MGF2RR_H
#define _MGF2RR_H

#include "RRVision.h"

// you may load object from mgf
rrVision::RRObjectImporter* new_mgf_importer(char* filename);

// and you may set direct exiting flux (not present in file) later
// (direct means that flux was caused by spotlight shining on face and face reflects part of power)
//void set_direct_exiting_flux(rrVision::RRObjectImporter* importer, unsigned triangle, const rrVision::RRColor* exitingFlux);

#endif
