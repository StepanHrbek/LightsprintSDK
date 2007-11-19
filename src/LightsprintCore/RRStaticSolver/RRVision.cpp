#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>

#include "LicGenOpt.h"
#include "rrcore.h"
#include "../RRStaticSolver/RRStaticSolver.h"

namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// statistics


//////////////////////////////////////////////////////////////////////////////
//
// global init/done



//////////////////////////////////////////////////////////////////////////////
//
// Obfuscation

unsigned char mask[16]={138,41,237,122,49,191,254,91,34,169,59,207,152,76,187,106};

#define MASK1(str,i) (str[i]^mask[(i)%16])
#define MASK2(str,i) MASK1(str,i),MASK1(str,i+1)
#define MASK3(str,i) MASK2(str,i),MASK1(str,i+2)
#define MASK4(str,i) MASK2(str,i),MASK2(str,i+2)
#define MASK5(str,i) MASK4(str,i),MASK1(str,i+4)
#define MASK6(str,i) MASK4(str,i),MASK2(str,i+4)
#define MASK7(str,i) MASK4(str,i),MASK3(str,i+4)
#define MASK8(str,i) MASK4(str,i),MASK4(str,i+4)
#define MASK10(str,i) MASK8(str,i),MASK2(str,i+8)
#define MASK12(str,i) MASK8(str,i),MASK4(str,i+8)
#define MASK14(str,i) MASK8(str,i),MASK6(str,i+8)
#define MASK16(str,i) MASK8(str,i),MASK8(str,i+8)
#define MASK18(str,i) MASK16(str,i),MASK2(str,i+16)
#define MASK32(str,i) MASK16(str,i),MASK16(str,i+16)
#define MASK44(str,i) MASK32(str,i),MASK12(str,i+32)
#define MASK64(str,i) MASK32(str,i),MASK32(str,i+32)
#define MASK108(str,i) MASK64(str,i),MASK44(str,i+64)

const char* unmask(const char* str,char* buf)
{
	for(unsigned i=0;;i++)
	{
		char c = str[i] ^ mask[i%16];
		buf[i] = c;
		if(!c) break;
	}
	return buf;
}


//////////////////////////////////////////////////////////////////////////////
//
// License


RRLicense::LicenseStatus registerLicense(char* licenseOwner, char* licenseNumber)
{
	licenseStatusValid = true;
	licenseStatus = RRLicense::VALID;
	return RRLicense::VALID;
}

RRLicense::LicenseStatus RRLicense::loadLicense(const char* filename)
{
	RRLicenseCollider::loadLicense(filename);
	char* owner = NULL;
	char* number = NULL;
	RRLicense::LicenseStatus status = registerLicense(owner,number);
	delete[] owner;
	return status;
}

} // namespace
