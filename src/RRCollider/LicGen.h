#ifndef COLLIDER_LICGEN_H
#define COLLIDER_LICGEN_H

#include <stdio.h>
#include <string.h>
#include <time.h>

extern "C" {
	#include "rsa.h"
	#include "http.h"
	#pragma comment(lib,"rsa.lib")
	#pragma comment(lib,"ws2_32.lib")
};

namespace rrLicense
{

class License
{
public:

	License(char* alicenseOwner, char* alicenseNumber)
	{
		memset(licenseOwner,0,ownerChars);
		strncpy(licenseOwner,alicenseOwner,ownerChars-1);
		memset(licenseNumber,0,numberChars);
		strncpy(licenseNumber,alicenseNumber,numberChars-1);
	}

	void report(uint8* key, char* name);
	void keygen();
	void sign();

	unsigned verify()
	{
		const unsigned rsaKeyBits = 1024;
		uint8 pub[rsaKeyBits/8] = 
		{
			69,86,243,3,69,199,233,66,64,77,84,130,251,175,70,242,159,6,56,244,
			73,177,50,137,180,85,255,196,208,97,44,131,165,160,182,220,46,46,238,181,
			209,121,153,7,255,42,44,8,153,243,76,52,77,77,69,88,220,92,252,202,
			221,210,57,108,53,209,45,218,240,219,89,163,113,233,179,219,248,84,204,236,
			78,238,168,146,13,195,250,7,202,138,85,243,212,170,133,202,92,176,91,162,
			70,56,49,221,138,82,216,142,39,24,28,214,155,50,140,4,143,254,106,72,
			212,91,175,30,182,243,83,233
		};

		// fill ctext
		uint8 ctext[rsaKeyBits/8+2];
		memset(ctext,0,rsaKeyBits/8+2);
		for(unsigned i=0;i<rsaKeyBits/8+2;i++)
		{
			if(licenseNumber[2*i]==0 || licenseNumber[2*i+1]==0) break;
			ctext[i] = (licenseNumber[2*i]-'A')+16*(licenseNumber[2*i+1]-'A');
		}
		unsigned expDay = 256*ctext[0]+ctext[1];

		// fill ptext
		uint8 ptext[ownerChars+2];
		memset(ptext,0,ownerChars+2);
		ptext[0] = expDay/256;
		ptext[1] = expDay;
		strncpy((char*)ptext+2,licenseOwner,ownerChars-1);

		// verify
		uint8 tmp[rsaKeyBits*8];
		uint8 ptext2[rsaKeyBits/8];
		unsigned ok = rsa_verify(ctext+2,ptext2,pub,rsaKeyBits,tmp);
		assert(ok);
		if(!ok) return 0;
		if(strcmp((char*)ptext,(char*)ptext2)) return 1;
		return expDay;

	}

	const char* getOwner() {return licenseOwner;}
	const char* getNumber() {return licenseNumber;}
	const char* getExp() {return licenseExp;}

private:
	static const unsigned daysValid = 31;
	static const unsigned rsaKeyBits = 1024;
	static const unsigned ownerChars = 70;
	static const unsigned numberChars = 500;

	char licenseOwner[ownerChars];
	char licenseNumber[numberChars];
	char licenseExp[20];
};

} // namespace

#endif
