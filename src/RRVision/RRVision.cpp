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
// RRSurface

void RRSurface::reset(bool twoSided)
{
	static RRSideBits sideBitsTmp[2][2]={
		{{1,1,1,1,1,1},{0,0,1,0,0,0}}, // definition of default 1-sided (front side, back side)
		{{1,1,1,1,1,1},{1,0,1,1,1,1}}, // definition of default 2-sided (front side, back side)
	};
	for(unsigned i=0;i<2;i++) sideBits[i] = sideBitsTmp[twoSided?1:0][i];
	diffuseReflectance        = RRColor(0.5);
	diffuseEmittance               = RRColor(0);
	specularReflectance            = 0;
	specularTransmittance          = 0;
	refractionReal                 = 1;
}

//////////////////////////////////////////////////////////////////////////////
//
// set/get state

static unsigned RRSSValueU[RRScene::SSU_LAST];
static RRReal   RRSSValueF[RRScene::SSF_LAST];

void RRScene::ResetStates()
{
	memset(RRSSValueU,0,sizeof(RRSSValueU));
	memset(RRSSValueF,0,sizeof(RRSSValueF));
	SetState(RRScene::GET_SOURCE,1);
	SetState(RRScene::GET_REFLECTED,1);
	SetState(RRScene::GET_SMOOTH,1);
	// development
	SetState(RRScene::USE_CLUSTERS,0);
	SetState(RRScene::GET_FINAL_GATHER,0);
	SetState(RRScene::FIGHT_NEEDLES,0);

	SetStateF(RRScene::MIN_FEATURE_SIZE,0);
	SetStateF(RRScene::MAX_SMOOTH_ANGLE,M_PI/10+0.01f);
	// development
	SetStateF(RRScene::SUBDIVISION_SPEED,0);
	SetStateF(RRScene::IGNORE_SMALLER_AREA,SMALL_REAL);
	SetStateF(RRScene::IGNORE_SMALLER_ANGLE,0.001f);
	SetStateF(RRScene::FIGHT_SMALLER_AREA,0.01f);
	SetStateF(RRScene::FIGHT_SMALLER_ANGLE,0.01f);
	//SetStateF(RRScene::SUBDIVISION_SPEED,0);
	//SetStateF(RRScene::MIN_FEATURE_SIZE,10.037f); //!!!
}

unsigned RRScene::GetState(SceneStateU state)
{
	if(state<0 || state>=SSU_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValueU[state];
}

unsigned RRScene::SetState(SceneStateU state, unsigned value)
{
	if(state<0 || state>=SSU_LAST) 
	{
		assert(0);
		return 0;
	}
	unsigned tmp = RRSSValueU[state];
	RRSSValueU[state] = value;
	return tmp;
}

real RRScene::GetStateF(SceneStateF state)
{
	if(state<0 || state>=SSF_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValueF[state];
}

real RRScene::SetStateF(SceneStateF state, real value)
{
	if(state<0 || state>=SSF_LAST) 
	{
		assert(0);
		return 0;
	}
	real tmp = RRSSValueF[state];
	RRSSValueF[state] = value;
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
	RRScene::ResetStates();
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
