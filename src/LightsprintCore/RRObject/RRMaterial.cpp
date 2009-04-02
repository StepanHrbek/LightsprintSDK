// --------------------------------------------------------------------------
// Material.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <math.h>

#include "Lightsprint/RRObject.h"
#include "Lightsprint/RRBuffer.h"
#include "memory.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMaterial


void RRMaterial::reset(bool twoSided)
{
	RRSideBits sideBitsTmp[2][2]={
		{{1,1,1,1,1,1,1},{0,0,1,1,0,0,1}}, // definition of default 1-sided (front side, back side)
		{{1,1,1,1,1,1,1},{1,0,1,1,1,1,1}}, // definition of default 2-sided (front side, back side)
	};
	sideBits[0]                  = sideBitsTmp[twoSided?1:0][0];
	sideBits[1]                  = sideBitsTmp[twoSided?1:0][1];
	diffuseReflectance.color     = RRVec3(0.5f);
	specularTransmittanceInAlpha = false;
	specularTransmittanceKeyed   = false;
	refractionIndex              = 1;
	lightmapTexcoord             = 0;
	minimalQualityForPointMaterials = UINT_MAX; // Keep point materials disabled, adapter must explicitly want them.
	name                         = NULL;
}

// Extract mean and variance from buffer.
// When working with non-physical scale data, provide scaler for correct results.
// Without scaler, mean would be a bit darker,
//  triangle materials based on mean would produce darker lightmaps than point materials.
RRVec4 getVariance(const RRBuffer* buffer, const RRScaler* scaler, RRVec4& average)
{
	RR_ASSERT(buffer);
	unsigned numElements = buffer->getWidth()*buffer->getHeight();
	RR_ASSERT(numElements);
	RRVec4 sum = RRVec4(0);
	RRVec4 sumOfSquares = RRVec4(0);
	unsigned step = 1+numElements/2000; // test approximately 2000 samples
	unsigned numElementsTested = 0;
	for (unsigned i=0;i<numElements;i+=1+(rand()%step))
	{
		RRVec4 elem = buffer->getElement(i);
		if (scaler) scaler->getPhysicalFactor(elem);
		sum += elem;
		sumOfSquares += elem*elem;
		numElementsTested++;
	}
	average = sum/numElementsTested;
	RRVec4 variance = sumOfSquares/numElementsTested-average*average;
	for (unsigned i=0;i<4;i++)
	{
		if (variance[i]<0) variance[i] = 0;
	}
	if (scaler)
	{
		scaler->getCustomFactor(average);
		RRVec4 standardDeviation = RRVec4(
			(variance[0]<0)?0:sqrt(variance[0]),
			(variance[1]<0)?0:sqrt(variance[1]),
			(variance[2]<0)?0:sqrt(variance[2]),
			(variance[3]<0)?0:sqrt(variance[3]));
		scaler->getCustomFactor(standardDeviation);
		variance = standardDeviation*standardDeviation;
	}
	return variance;
}

// extract mean and variance from buffer
// convert mean to color, variance to scalar
RRReal getVariance(const RRBuffer* _buffer, const RRScaler* _scaler, RRVec3& _average, bool _isTransmittanceInAlpha)
{
	RRVec4 average;
	RRVec4 variance = getVariance(_buffer,_scaler,average);
	if (_isTransmittanceInAlpha)
	{
		_average = RRVec3(1-average[3]);
		return variance[3];
	}
	else
	{
		_average = average;
		return variance.RRVec3::avg();
	}
}

void RRMaterial::Property::multiplyAdd(RRVec4 multiplier, RRVec4 addend)
{
	if (multiplier!=RRVec4(1) || addend!=RRVec4(0))
	{
		color = color*multiplier+addend;
		if (texture)
		{
			texture->setFormatFloats();
			texture->multiplyAdd(multiplier,addend);
		}
	}
}

RRReal RRMaterial::Property::updateColorFromTexture(const RRScaler* scaler, bool isTransmittanceInAlpha, UniformTextureAction uniformTextureAction)
{
	if (texture)
	{
		RRReal variance = getVariance(texture,scaler,color,isTransmittanceInAlpha);
		if (!variance)
		{
			switch (uniformTextureAction)
			{
				case UTA_DELETE: delete texture;
				case UTA_NULL: texture = NULL;
				case UTA_KEEP:
				default:;
			}
		}
		return variance;
	}
	return 0;
}

void RRMaterial::updateColorsFromTextures(const RRScaler* scaler, UniformTextureAction uniformTextureAction)
{
	float variance = 0.000001f;
	variance += 5 * specularTransmittance.updateColorFromTexture(scaler,specularTransmittanceInAlpha,uniformTextureAction);
	variance += 3 * diffuseEmittance.updateColorFromTexture(scaler,0,uniformTextureAction);
	variance += 2 * diffuseReflectance.updateColorFromTexture(scaler,0,uniformTextureAction);
	variance += 1 * specularReflectance.updateColorFromTexture(scaler,0,uniformTextureAction);
	minimalQualityForPointMaterials = unsigned(40/(variance*variance));
	//RRReporter::report(INF2,"%d\n",minimalQualityForPointMaterials);
}

// opacityInAlpha -> avg distance from 0 or 1 (what's closer)
// !opacityInAlpha -> avg distance from 0,0,0 or 1,1,1 (what's closer)
// result: 0 .. 0.06 keying probably better (3DRender trees that need keying have 0.03)
// result: 0.06 .. 0.5 blending probably better
static RRReal getBlendImportance(RRBuffer* buffer, bool opacityInAlpha)
{
	if (!buffer) return 0;
	enum {size = 16};
	RRReal blendImportance = 0;
	for (unsigned i=0;i<size;i++)
		for (unsigned j=0;j<size;j++)
		{
			RRVec4 color = buffer->getElement(RRVec3(i/(float)size,j/(float)size,0));
			RRReal distFrom0 = opacityInAlpha ? fabs(color[3]) : color.RRVec3::abs().avg();
			RRReal distFrom1 = opacityInAlpha ? fabs(color[3]-1) : (color-RRVec4(1)).RRVec3::abs().avg();
			blendImportance += MIN(distFrom0,distFrom1);
		}
	return blendImportance/(size*size); 
}

void RRMaterial::updateKeyingFromTransmittance()
{
	specularTransmittanceKeyed = getBlendImportance(specularTransmittance.texture,specularTransmittanceInAlpha)<0.06f;
}

void RRMaterial::updateSideBitsFromColors()
{
	sideBits[0].reflect = sideBits[0].catchFrom && specularReflectance.color!=RRVec3(0);
	sideBits[1].reflect = sideBits[1].catchFrom && specularReflectance.color!=RRVec3(0);
	sideBits[0].transmitFrom = sideBits[0].catchFrom && specularTransmittance.color!=RRVec3(0);
	sideBits[1].transmitFrom = sideBits[1].catchFrom && specularTransmittance.color!=RRVec3(0);
}

bool clamp1(RRReal& a, RRReal min, RRReal max)
{
	if (a<min) a=min; else
		if (a>max) a=max; else
			return false;
	return true;
}

bool clamp3(RRVec3& vec, RRReal min, RRReal max)
{
	unsigned clamped = 3;
	for (unsigned i=0;i<3;i++)
	{
		if (vec[i]<min) vec[i]=min; else
			if (vec[i]>max) vec[i]=max; else
				clamped--;
	}
	return clamped>0;
}

bool RRMaterial::validate(RRReal redistributedPhotonsLimit)
{
	bool changed = false;

	if (clamp3(diffuseReflectance.color,0,10)) changed = true;
	if (clamp3(specularReflectance.color,0,10)) changed = true;
	if (clamp3(specularTransmittance.color,0,10)) changed = true;
	if (clamp3(diffuseEmittance.color,0,1e6f)) changed = true;

	RRVec3 sum = diffuseReflectance.color+specularTransmittance.color+specularReflectance.color;
	RRReal max = MAX(sum[0],MAX(sum[1],sum[2]));
	if (max>redistributedPhotonsLimit)
	{
		diffuseReflectance.color *= redistributedPhotonsLimit/max;
		specularReflectance.color *= redistributedPhotonsLimit/max;
		specularTransmittance.color *= redistributedPhotonsLimit/max;
		changed = true;
	}

	// it is common error to default refraction to 0, we must anticipate and handle it
	if (refractionIndex==0) {refractionIndex = 1; changed = true;}

	return changed;
}

void RRMaterial::convertToCustomScale(const RRScaler* scaler)
{
	if (scaler)
	{
		scaler->getCustomFactor(diffuseReflectance.color);
		scaler->getCustomScale(diffuseEmittance.color);
		scaler->getCustomFactor(specularReflectance.color);
		scaler->getCustomFactor(specularTransmittance.color);
		validate();
	}
}

void RRMaterial::convertToPhysicalScale(const RRScaler* scaler)
{
	if (scaler)
	{
		scaler->getPhysicalFactor(diffuseReflectance.color);
		scaler->getPhysicalScale(diffuseEmittance.color);
		scaler->getPhysicalFactor(specularReflectance.color);
		scaler->getPhysicalFactor(specularTransmittance.color);
		validate();
	}
}

} // namespace
