/*
prozatim odsunuto z headeru:

// get intersection
typedef       bool INTERSECT(rrCollider::RRRay*);
typedef       ObjectHandle ENUM_OBJECTS(rrCollider::RRRay*, INTERSECT);
void          setObjectEnumerator(ENUM_OBJECTS enumerator);
bool          intersect(rrCollider::RRRay* ray);

void          sceneSetColorFilter(const RRReal* colorFilter);
*/

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>

#include "LicGenOpt.h"
#include "rrcore.h"
#include "RRVision.h"

namespace rrVision
{

union StateValue
{
	unsigned u;
	real r;
};


//////////////////////////////////////////////////////////////////////////////
//
// statistics

RRSceneStatistics::RRSceneStatistics()
{
	Reset();
}

void RRSceneStatistics::Reset()
{
	memset(this,0,sizeof(*this));
}

RRSceneStatistics* RRScene::getSceneStatistics()
{
	static RRSceneStatistics s;
	return &s;
}


//////////////////////////////////////////////////////////////////////////////
//
// set/get state

static StateValue RRSSValue[RRSS_LAST];

void RRResetStates()
{
	memset(RRSSValue,0,sizeof(RRSSValue));
	RRSetState(RRSS_USE_CLUSTERS,0);
	RRSetStateF(RRSSF_SUBDIVISION_SPEED,1);
	RRSetState(RRSS_GET_SOURCE,1);
	RRSetState(RRSS_GET_REFLECTED,1);
	RRSetState(RRSS_GET_SMOOTH,1);
	RRSetState(RRSS_GET_FINAL_GATHER,0);
	RRSetStateF(RRSSF_MIN_FEATURE_SIZE,0);
	RRSetStateF(RRSSF_MAX_SMOOTH_ANGLE,M_PI/10+0.01f);
	RRSetStateF(RRSSF_IGNORE_SMALLER_AREA,SMALL_REAL);
	RRSetStateF(RRSSF_IGNORE_SMALLER_ANGLE,0.001f);
	RRSetState(RRSS_FIGHT_NEEDLES,0);
	RRSetStateF(RRSSF_FIGHT_SMALLER_AREA,0.01f);
	RRSetStateF(RRSSF_FIGHT_SMALLER_ANGLE,0.01f);

	//RRSetStateF(RRSSF_SUBDIVISION_SPEED,0);
	//RRSetStateF(RRSSF_MIN_FEATURE_SIZE,10.037f); //!!!
}

unsigned RRGetState(RRSceneState state)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValue[state].u;
}

unsigned RRSetState(RRSceneState state, unsigned value)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	unsigned tmp = RRSSValue[state].u;
	RRSSValue[state].u = value;
	return tmp;
}

real RRGetStateF(RRSceneState state)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValue[state].r;
}

real RRSetStateF(RRSceneState state, real value)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	real tmp = RRSSValue[state].r;
	RRSSValue[state].r = value;
	return tmp;
}

//////////////////////////////////////////////////////////////////////////////
//
// global init/done

class RREngine
{
public:
	RREngine();
	~RREngine();
};

RREngine::RREngine()
{
	core_Init();
	RRResetStates();
}

RREngine::~RREngine()
{
	core_Done();
}

static RREngine engine;


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


RRLicense::LicenseStatus RRLicense::registerLicense(char* licenseOwner, char* licenseNumber)
{
	licenseStatusValid = true;
	licenseStatus = VALID;
	return VALID;
}

} // namespace
