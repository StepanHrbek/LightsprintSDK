#include <math.h>

#include "Lightsprint/RRObject.h"
#include "memory.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMaterial
;
// K cemu jsou sidebits?
// 
// gather-hemi: sondovat z povrchu do okoli zda tam neni svetlo
//
// gather-lights: zmerit viditelnost z povrchu do RRLight
//   - povrch je prostupny podle specularTransmittance
//     - dulezite pro normalni matrose
//     - catchFrom, transmitFrom
//   - povrch je zcela prostupny (napr zadni strana facu s 0.5 propustnosti)
//     - dulezite pro polopruhlednou krabicku, zadni strany 1sided sten
//       renderuje se jen 1 strana, tak at i propustnost omezuje jen jedna
//     - !catchFrom
//   - paprsek zasahl neco co nemel, vyradit ho z vypoctu jako neverohodny
//     - dulezite proti vytekani tmy zpod zidky v koupelne
//     - !legal
//   - povrch je zcela neprostupny (napr zadni strana facu s 0.5 propustnosti)
//     - asi neni nutne
//     - catchFrom, !transmitFrom
//
// propag: strilet fotony z povrchu do okoli
//

void RRMaterial::reset(bool twoSided)
{
	memset(this,0,sizeof(*this));
	RRSideBits sideBitsTmp[2][2]={
		{{1,1,1,1,1,1,1,0},{0,0,1,0,0,0,0,0}}, // definition of default 1-sided (front side, back side)
		{{1,1,1,1,1,1,1,0},{1,0,1,1,1,1,1,0}}, // definition of default 2-sided (front side, back side)
	};
	for(unsigned i=0;i<2;i++) sideBits[i] = sideBitsTmp[twoSided?1:0][i];
	diffuseReflectance             = RRColor(0.5);
	diffuseEmittance               = RRColor(0);
	specularReflectance            = 0;
	specularTransmittance          = 0;
	refractionIndex                = 1;
}

bool clamp(RRReal& vec, RRReal min, RRReal max)
{
	if(vec<min) {vec=min; return true;}
	if(vec>max) {vec=max; return true;}
	return false;
}

bool clamp(RRVec3& vec, RRReal min, RRReal max)
{
	unsigned clamped = 3;
	for(unsigned i=0;i<3;i++)
	{
		if(vec[i]<min) vec[i]=min; else
			if(vec[i]>max) vec[i]=max; else
				clamped--;
	}
	return clamped>0;
}

bool RRMaterial::validate()
{
	bool changed = false;

	const float MAX_LEFT = 0.98f; // photons that left surface, sum of dif/spec reflected, transmitted
	const float MAX_REFL = MAX_LEFT;
	const float MAX_TRANS = MAX_LEFT;
	const float MAX_EMIT = 1e6;//1000;

	if(clamp(diffuseEmittance,0,MAX_EMIT)) changed = true;
	if(clamp(specularTransmittance,0,MAX_TRANS)) changed = true;
	if(clamp(diffuseReflectance,0,MAX_REFL)) changed = true;
	if(clamp(specularReflectance,0,MAX_REFL)) changed = true;
	// clamp so that no new photons appear
	for(unsigned i=0;i<3;i++)
	{
		RRReal all = diffuseReflectance[i]+specularReflectance+specularTransmittance;
		if(clamp(all,0,MAX_LEFT)) changed = true;
		if(clamp(diffuseReflectance[i],0,all-specularTransmittance)) changed = true;
		if(clamp(specularReflectance,0,all-specularTransmittance-diffuseReflectance[i])) changed = true;
	}

	// FCollada returns 0 when information is not available
	if(refractionIndex==0) {refractionIndex = 1; changed = true;}

	return changed;
}


} // namespace
