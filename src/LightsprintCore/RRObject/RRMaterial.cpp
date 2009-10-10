// --------------------------------------------------------------------------
// Material.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <math.h>

#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRMaterial.h"
#include "Lightsprint/RRBuffer.h"
#include "memory.h"
#include <climits> // UINT_MAX

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
	unsigned r = 1649317406;
	for (unsigned i=0;i<numElements;)
	{
		RRVec4 elem = buffer->getElement(i);
		if (scaler) scaler->getPhysicalFactor(elem);
		sum += elem;
		sumOfSquares += elem*elem;
		numElementsTested++;
		// texel selection must stay deterministic so that colors distilled from textures are always the same,
		// hash is always the same and fireball does not ask for rebuild
		r = (r * 1103515245) + 12345;
		i+=1+((r>>8)%step);
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

unsigned RRMaterial::Property::createTextureFromColor(bool isTransmittance)
{
	if (texture)
		return 0;
	// isTransmittance:
	//   specularTransmittanceInAlpha was irrelevant without texture
	//   it will be relevant when we create texture, should we clear it?
	//   no, instead we are adding alpha to satisfy both specularTransmittanceInAlpha=false and true
	texture = RRBuffer::create(BT_2D_TEXTURE,1,1,1,isTransmittance?BF_RGBA:BF_RGB,true,NULL);
	texture->setElement(0,RRVec4(color,1-color.avg()));
	return 1;
}

unsigned RRMaterial::createTexturesFromColors()
{
	return diffuseReflectance.createTextureFromColor(false)
		+ specularReflectance.createTextureFromColor(false)
		+ diffuseEmittance.createTextureFromColor(false)
		+ specularTransmittance.createTextureFromColor(true);
}

// opacityInAlpha -> distance of color[3] from 0 or 1 (what's closer)
// !opacityInAlpha -> distance of color[0,1,2] from 0,0,0 or 1,1,1 (what's closer)
static RRReal getBlendImportance(RRVec4 color, bool opacityInAlpha)
{
	RRReal distFrom0 = opacityInAlpha ? fabs(color[3]) : color.RRVec3::abs().avg();
	RRReal distFrom1 = opacityInAlpha ? fabs(color[3]-1) : (color-RRVec4(1)).RRVec3::abs().avg();
	return RR_MIN(distFrom0,distFrom1);
}

static RRReal getBlendImportance(RRBuffer* transmittanceTexture, bool opacityInAlpha)
{
	RRReal blendImportanceSum = 0;
	RR_ASSERT(transmittanceTexture);
	enum {size = 16};
	unsigned width = transmittanceTexture->getWidth();
	unsigned height = transmittanceTexture->getHeight();
	for (unsigned i=0;i<size;i++)
	{
		for (unsigned j=0;j<size;j++)
		{
			unsigned x = (unsigned)((i+0.5f)*width/size);
			unsigned y = (unsigned)((j+0.5f)*height/size);
			RRVec4 color = transmittanceTexture->getElement(x+y*width);
			RRReal blendImportance = getBlendImportance(color,opacityInAlpha);
			if (blendImportance)
			{
				RRVec4 colorMin = color;
				RRVec4 colorMax = color;
				for (unsigned n=0;n<4;n++)
				{
					RRVec4 neighbour = color;
					if (n==0 && x>0) neighbour = transmittanceTexture->getElement(x-1+y*width);
					if (n==1 && x+1<width) neighbour = transmittanceTexture->getElement(x+1+y*width);
					if (n==2 && y>0) neighbour = transmittanceTexture->getElement(x+(y-1)*width);
					if (n==3 && y+1<height) neighbour = transmittanceTexture->getElement(x+(y+1)*width);
					for (unsigned a=0;a<4;a++)
					{
						colorMin[a] = RR_MIN(colorMin[a],neighbour[a]);
						colorMax[a] = RR_MAX(colorMax[a],neighbour[a]);
					}
				}
				RRReal areaDelta = opacityInAlpha ? colorMax[3]-colorMin[3] : (colorMax-colorMin).RRVec3::avg();
				// if max-min opacity in area is more than 50%, blend importance is decreased
				// if max-min opacity in area is less than 50%, blend importance is increased up to 10x for completely flat area
				RRReal areaFlatness = 1 / (areaDelta*2+0.1f);
				blendImportanceSum += blendImportance * areaFlatness;
			}
			else
			{
				blendImportanceSum += blendImportance;
			}
		}
	}
	blendImportanceSum /= (size*size);
	return blendImportanceSum;
}

void RRMaterial::updateKeyingFromTransmittance()
{
	if (specularTransmittance.texture)
	{
		RRReal blendImportance = getBlendImportance(specularTransmittance.texture,specularTransmittanceInAlpha);
		// car windows that need blending have at least 0.13
		// trees that need 1-bit keying have less than 0.016
		specularTransmittanceKeyed = blendImportance<0.05f;
	}
	else
	{
		// 0% transparent    -> render opaque, 1-bit keyed
		// 1-99% transparent -> render blended
		// 100% transparent  -> render blended! this is for realtime render where we use custom material properties,
		//                      so 100% transparency still allows >0% specular, diffuse and emissive components.
		//                      1-bit keying would drop whole object.
		specularTransmittanceKeyed = specularTransmittance.color.avg()<0.01f;
	}
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
	RRReal max = RR_MAX3(sum[0],sum[1],sum[2]);
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
