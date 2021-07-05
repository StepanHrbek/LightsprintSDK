// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Material.
// --------------------------------------------------------------------------

#include <math.h>

#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRMaterial.h"
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRMesh.h"
#include "memory.h"
#include <climits> // UINT_MAX

#define DEFAULT_SHININESS 1000
#define MAX_SHININESS 500000
					// prevents errors from limited float precision:
					// - test\cuberefl-car.rr3   - car reflection has banding visible if shininess>1'000'000
					// - 2015-03!bake\boris2.rr3 - floor lightmap contains bright dots (hooks reflect some photons with reflectance>>1) if shininess>500'000
#define MIN_ROUGHNESS (1.f/MAX_SHININESS)
					// prevents errors from limited float precision:
					// - BLINN_TORRANCE_SPARROW  - generates #IND if roughness<1/100'000'000
					// - test\cuberefl-car.rr3   - car reflection is too bright if roughness<1/2'000'000
#define MAX_EMITTANCE 1e6f
//#define BACKSIDE_ILLEGAL // sets 'illegal' flag on invisible backsides. no longer used, but LDM generator in Lightsmark still needs it

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRRadiometricMeasure

bool RRRadiometricMeasure::operator ==(const RRRadiometricMeasure& a) const
{
	return 1
		&& a.exiting==exiting
		&& a.scaled==scaled
		&& a.flux==flux
		&& a.direct==direct
		&& a.indirect==indirect
		&& a.smoothed==smoothed
		;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRSideBits

bool RRSideBits::operator ==(const RRSideBits& a) const
{
	return a.renderFrom==renderFrom
		&& a.emitTo==emitTo
		&& a.catchFrom==catchFrom
		&& a.legal==legal
		&& a.receiveFrom==receiveFrom
		&& a.reflect==reflect
		&& a.transmitFrom==transmitFrom
		;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMaterial::Property

bool RRMaterial::Property::operator ==(const RRMaterial::Property& a) const
{
	return a.color==color
		&& a.colorLinear==colorLinear
		&& a.texcoord==texcoord
		&& a.texture==texture
		;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMaterial

RRMaterial::RRMaterial()
{
	preview = nullptr;
}


static void copyProperty(RRMaterial::Property& to, const RRMaterial::Property& from)
{
	to.color = from.color;
	to.colorLinear = from.colorLinear;
	to.texcoord = from.texcoord;
	// copy texture only if it differs, copying is bit expensive and thread unsafe
	if (to.texture!=from.texture)
	{
		// texture is owned by material, so old one must be deleted, new one created (reference only)
		delete to.texture;
		to.texture = from.texture ? from.texture->createReference() : nullptr;
	}
}

void RRMaterial::copyFrom(const RRMaterial& a)
{
	sideBits[0] = a.sideBits[0];
	sideBits[1] = a.sideBits[1];
	copyProperty(diffuseReflectance,a.diffuseReflectance);
	copyProperty(diffuseEmittance,a.diffuseEmittance);
	copyProperty(specularReflectance,a.specularReflectance);
	copyProperty(specularTransmittance,a.specularTransmittance);
	copyProperty(bumpMap,a.bumpMap);
	copyProperty(lightmap,a.lightmap);
	specularModel = a.specularModel;
	specularShininess = a.specularShininess;
	specularTransmittanceInAlpha = a.specularTransmittanceInAlpha;
	specularTransmittanceKeyed = a.specularTransmittanceKeyed;
	specularTransmittanceThreshold = a.specularTransmittanceThreshold;
	specularTransmittanceMapInverted = a.specularTransmittanceMapInverted;
	specularTransmittanceBackground = a.specularTransmittanceBackground;
	refractionIndex = a.refractionIndex;
	bumpMapTypeHeight = a.bumpMapTypeHeight;
	lightmap.texcoord = a.lightmap.texcoord;
	minimalQualityForPointMaterials = a.minimalQualityForPointMaterials;
	if (name!=a.name) name = a.name;
	RR_SAFE_DELETE(preview);
}

bool RRMaterial::operator ==(const RRMaterial& a) const
{
	return a.sideBits[0]==sideBits[0]
		&& a.sideBits[1]==sideBits[1]
		&& a.diffuseReflectance==diffuseReflectance
		&& a.diffuseEmittance==diffuseEmittance
		&& a.specularReflectance==specularReflectance
		&& a.specularTransmittance==specularTransmittance
		&& a.bumpMap==bumpMap
		&& a.lightmap==lightmap
		&& a.specularModel==specularModel
		&& a.specularShininess==specularShininess
		&& a.specularTransmittanceInAlpha==specularTransmittanceInAlpha
		&& a.specularTransmittanceKeyed==specularTransmittanceKeyed
		&& a.specularTransmittanceThreshold==specularTransmittanceThreshold
		&& a.specularTransmittanceMapInverted==specularTransmittanceMapInverted
		&& a.specularTransmittanceBackground==specularTransmittanceBackground
		&& a.refractionIndex==refractionIndex
		&& a.bumpMapTypeHeight==bumpMapTypeHeight
		&& a.lightmap.texcoord==lightmap.texcoord
		&& a.minimalQualityForPointMaterials==minimalQualityForPointMaterials
		&& a.name==name
		;
}


void RRMaterial::reset(bool twoSided)
{
	RRSideBits sideBitsTmp[2][2]={
#ifdef BACKSIDE_ILLEGAL
		{{1,1,1,1,1,1,1},{0,0,1,0,0,0,1}}, // definition of default 1-sided (front side, illegal invisible back side)
#else
		{{1,1,1,1,1,1,1},{0,0,1,1,0,0,1}}, // definition of default 1-sided (front side, legal invisible back side)
#endif
		{{1,1,1,1,1,1,1},{1,0,1,1,1,1,1}}, // definition of default 2-sided (front side, visible back side)
	};
	sideBits[0]                  = sideBitsTmp[twoSided?1:0][0];
	sideBits[1]                  = sideBitsTmp[twoSided?1:0][1];
	diffuseReflectance.color     = RRVec3(0.5f);
	diffuseReflectance.colorLinear = RRVec3(0.5f);//!!!
	specularModel                = PHONG;
	specularShininess            = DEFAULT_SHININESS;
	specularTransmittanceInAlpha = false;
	specularTransmittanceKeyed   = false;
	specularTransmittanceThreshold = 0.5f;
	specularTransmittanceMapInverted = false;
	specularTransmittanceBackground = false;
	bumpMapTypeHeight            = true;
	refractionIndex              = 1;
	bumpMap.color                = RRVec3(1);
	bumpMap.colorLinear          = RRVec3(1);
	lightmap.color               = RRVec3(0.5f);
	lightmap.colorLinear         = RRVec3(0.5f);
	lightmap.texcoord            = UINT_MAX; // no unwrap by default
	minimalQualityForPointMaterials = UINT_MAX; // Keep point materials disabled, adapter must explicitly want them.
	name.clear();
	RR_SAFE_DELETE(preview);
}

// Extract mean and variance from buffer.
// When working with custom color space data, provide colorSpace for correct results.
// Without colorSpace, mean of sRGB data would be a bit darker,
//  triangle materials based on mean would produce darker lightmaps than point materials.
RRVec4 getVariance(const RRBuffer* buffer, const RRColorSpace* colorSpace, RRVec4& average)
{
	RR_ASSERT(buffer);
	if (!buffer)
	{
		average = RRVec4(0);
		return RRVec4(0);
	}
	if (buffer->getDuration())
	{
		// video changes in time, ignore current frame and return average gray
		RRBufferFormat videoFormat = buffer->getFormat();
		bool videoAlpha = videoFormat==BF_RGBA || videoFormat==BF_RGBAF;
		average = RRVec4(0.5,0.5,0.5,videoAlpha?0.5f:1);
		return RRVec4(0.5,0.5,0.5,videoAlpha?0.5f:0);
	}
	unsigned numElements = buffer->getWidth()*buffer->getHeight();
	RR_ASSERT(numElements);
	RRVec4 sum(0);
	RRVec4 sumOfSquares(0);
	unsigned step = 1+numElements/2000; // test approximately 2000 samples
	unsigned numElementsTested = 0;
	unsigned r = 1649317406;
	for (unsigned i=0;i<numElements;)
	{
		RRVec4 elem = buffer->getElement(i,colorSpace);
		sum += elem;
		sumOfSquares += elem*elem;
		numElementsTested++;
		// texel selection must stay deterministic so that colors distilled from textures are always the same,
		// hash is always the same and fireball does not ask for rebuild
		// (still, accumulating floats results in different colors on different CPUs, so hash s preserved only on one CPU)
		r = (r * 1103515245) + 12345;
		i+=1+((r>>8)%step);
	}
	average = sum/numElementsTested;
	RRVec4 variance = sumOfSquares/numElementsTested-average*average;
	for (unsigned i=0;i<4;i++)
	{
		if (variance[i]<0) variance[i] = 0;
	}
	RRVec4 standardDeviation(sqrt(variance[0]),sqrt(variance[1]),sqrt(variance[2]),sqrt(variance[3]));
	if (colorSpace)
	{
		colorSpace->fromLinear(average);
		colorSpace->fromLinear(standardDeviation);
		variance = standardDeviation*standardDeviation;
	}
	// -1               ... texture with random 0 1 or 0 20 returns the same as texture with random 0 255, which is max possible difference
	// -0.9             ... texture with random 0 20 returns nearly as much as 0 255. 0 1 returns much less
	// return variance; ... texture with random 0 1 is equal to 254 255, which is minimal possible difference
	return standardDeviation*pow(average.avg()+0.00001f,-0.9f);
}

// extract mean and variance from buffer
// convert mean to color, variance to scalar
RRReal getVariance(const RRBuffer* _buffer, const RRColorSpace* _colorSpace, RRVec3& _average, bool _isTransmittanceInAlpha)
{
	RRVec4 average;
	RRVec4 variance = getVariance(_buffer,_colorSpace,average);
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

RRReal RRMaterial::Property::updateColorFromTexture(const RRColorSpace* colorSpace, bool isTransmittanceInAlpha, UniformTextureAction uniformTextureAction, bool updateEvenFromStub)
{
	if (texture && (updateEvenFromStub || !texture->isStub()))
	{
		RRReal variance = getVariance(texture,colorSpace,color,isTransmittanceInAlpha);
		if (!variance && !texture->isStub()) // don't remove stubs
		{
			switch (uniformTextureAction)
			{
				case UTA_DELETE: delete texture;
				case UTA_NULL: texture = nullptr;
				case UTA_KEEP:
				default:;
			}
		}
		return variance;
	}
	return 0;
}

void RRMaterial::updateColorsFromTextures(const RRColorSpace* colorSpace, UniformTextureAction uniformTextureAction, bool updateEvenFromStubs)
{
	float variance = 0;
	variance = RR_MAX(variance, 3 * specularTransmittance.updateColorFromTexture(colorSpace,specularTransmittanceInAlpha,uniformTextureAction,updateEvenFromStubs));
	variance = RR_MAX(variance, 2 * diffuseEmittance.updateColorFromTexture(colorSpace,0,uniformTextureAction,updateEvenFromStubs));
	variance = RR_MAX(variance, 1 * diffuseReflectance.updateColorFromTexture(colorSpace,0,uniformTextureAction,updateEvenFromStubs));
	variance = RR_MAX(variance, 1 * specularReflectance.updateColorFromTexture(colorSpace,0,uniformTextureAction,updateEvenFromStubs));
	RRVec2 bumpMultipliers = bumpMap.color; // backup bumpMap.color.xy, those are multipliers
	bumpMap.updateColorFromTexture(nullptr,0,uniformTextureAction,updateEvenFromStubs);
	bumpMap.color.x = bumpMultipliers.x;
	bumpMap.color.y = bumpMultipliers.y;
	//lightmap.updateColorFromTexture(nullptr,0,UTA_KEEP,updateEvenFromStubs); lightmap.color not used, so no need to update it
	minimalQualityForPointMaterials = variance ? unsigned(100/(variance*variance)) : UINT_MAX;
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
	texture = RRBuffer::create(BT_2D_TEXTURE,1,1,1,isTransmittance?BF_RGBA:BF_RGB,true,nullptr);
	texture->setElement(0,RRVec4(color,1-color.avg()),nullptr);
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
	if (!transmittanceTexture)
		return 0;
	if (transmittanceTexture->getDuration())
	{
		// video changes in time, ignore current frame and blend
		return 1;
	}
	enum {size = 16};
	unsigned width = transmittanceTexture->getWidth();
	unsigned height = transmittanceTexture->getHeight();
	for (unsigned i=0;i<size;i++)
	{
		for (unsigned j=0;j<size;j++)
		{
			unsigned x = (unsigned)((i+0.5f)*width/size);
			unsigned y = (unsigned)((j+0.5f)*height/size);
			RRVec4 color = transmittanceTexture->getElement(x+y*width,nullptr);
			RRReal blendImportance = getBlendImportance(color,opacityInAlpha);
			if (blendImportance)
			{
				RRVec4 colorMin = color;
				RRVec4 colorMax = color;
				for (unsigned n=0;n<4;n++)
				{
					RRVec4 neighbour = color;
					if (n==0 && x>0) neighbour = transmittanceTexture->getElement(x-1+y*width,nullptr);
					if (n==1 && x+1<width) neighbour = transmittanceTexture->getElement(x+1+y*width,nullptr);
					if (n==2 && y>0) neighbour = transmittanceTexture->getElement(x+(y-1)*width,nullptr);
					if (n==3 && y+1<height) neighbour = transmittanceTexture->getElement(x+(y+1)*width,nullptr);
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

void RRMaterial::updateBumpMapType()
{
	if (!bumpMap.texture)
	{
		// no action needed
	}
	else
	if (bumpMap.texture==diffuseReflectance.texture)
	{
		bumpMapTypeHeight = true;
	}
	else
	if (bumpMap.texture->getFormat()==BF_LUMINANCE || bumpMap.texture->getFormat()==BF_LUMINANCEF || bumpMap.texture->getFormat()==BF_DEPTH)
	{
		bumpMapTypeHeight = true;
	}
	else
	if (bumpMap.texture->filename=="c@pture")
	{
		bumpMapTypeHeight = true;
	}
	else
	{
		unsigned numElements = bumpMap.texture->getNumElements();
		enum {LOOKUPS=23};
		RRVec3 sum(0);
		for (unsigned i=0;i<LOOKUPS;i++)
		{
			RRVec3 color = bumpMap.texture->getElement(numElements*i/LOOKUPS,nullptr);
			sum += color;
		}
		float blueness = sum.z-RR_MAX(sum.x,sum.y); // normal map should have it highly positive, gray height map zero, rgb used as height map can be anything (with average in zero)
		bumpMapTypeHeight = blueness<0.1f*LOOKUPS;
	}
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

	if (clamp3(diffuseReflectance.colorLinear,0,10)) changed = true;
	if (clamp3(specularReflectance.colorLinear,0,10)) changed = true;
	if (clamp3(specularTransmittance.colorLinear,0,10)) changed = true;
	if (clamp3(diffuseEmittance.colorLinear,0,MAX_EMITTANCE)) changed = true;

	if (specularModel==PHONG || specularModel==BLINN_PHONG)
		if (clamp1(specularShininess,0,MAX_SHININESS)) changed = true;
	if (specularModel==TORRANCE_SPARROW || specularModel==BLINN_TORRANCE_SPARROW)
		if (clamp1(specularShininess,MIN_ROUGHNESS,1)) changed = true;

	RRVec3 sum = diffuseReflectance.colorLinear+specularTransmittance.colorLinear+specularReflectance.colorLinear;
	RRReal max = sum.maxi();
	if (max>redistributedPhotonsLimit)
	{
		diffuseReflectance.colorLinear *= redistributedPhotonsLimit/max;
		specularReflectance.colorLinear *= redistributedPhotonsLimit/max;
		specularTransmittance.colorLinear *= redistributedPhotonsLimit/max;
		changed = true;
	}

	// it is common error to default refraction to 0, we must anticipate and handle it
	if (refractionIndex==0) {refractionIndex = 1; changed = true;}

	return changed;
}

void RRMaterial::convertFromLinear(const RRColorSpace* colorSpace)
{
	if (colorSpace)
	{
		colorSpace->fromLinear(diffuseReflectance.color = diffuseReflectance.colorLinear);
		colorSpace->fromLinear(diffuseEmittance.color = diffuseEmittance.colorLinear);
		colorSpace->fromLinear(specularReflectance.color = specularReflectance.colorLinear);
		colorSpace->fromLinear(specularTransmittance.color = specularTransmittance.colorLinear);
	}
	validate();
}

void RRMaterial::convertToLinear(const RRColorSpace* colorSpace)
{
	if (colorSpace)
	{
		colorSpace->toLinear(diffuseReflectance.colorLinear = diffuseReflectance.color);
		colorSpace->toLinear(diffuseEmittance.colorLinear = diffuseEmittance.color);
		colorSpace->toLinear(specularReflectance.colorLinear = specularReflectance.color);
		colorSpace->toLinear(specularTransmittance.colorLinear = specularTransmittance.color);
	}
	validate();
}

bool RRMaterial::needsBlending() const
{
	return specularTransmittance.color!=RRVec3(0) && !specularTransmittanceKeyed;
}

RRMaterial::~RRMaterial()
{
	delete diffuseReflectance.texture;
	delete diffuseEmittance.texture;
	delete specularReflectance.texture;
	delete specularTransmittance.texture;
	delete bumpMap.texture;
	delete lightmap.texture;
	delete preview;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMaterial pathtracing

// Copy of GLSL reflect(), used for consistency with shaders.
// I ... Specifies the incident vector.
// N ... Specifies the normal vector.
// N normalized = returns reflection direction
// N and I normalized = returns normalized reflection direction
static RRVec3 reflect(const RRVec3& I, const RRVec3& N)
{
	return I - N * (2 * N.dot(I));
}

// Similar to GLSL refract(), used for consistency with shaders.
// I ... Specifies the incident vector.
// N ... Specifies the normal vector.
// eta ... Specifies the ratio of indices of refraction.
// Differs by
// - returns total internal reflection direction where GLSL returns 0.
RRVec3 refract(const RRVec3& I, const RRVec3& N, float eta)
{
	RRReal ndoti = I.dot(N);
	RRReal D2 = 1-eta*eta*(1-ndoti*ndoti);
	if (D2>=0)
		return I*eta-N*((ndoti>=0)?eta*ndoti-sqrt(D2):eta*ndoti+sqrt(D2));
	// total internal reflection
	return I-N*(2*ndoti);
}

// Customized refract(). Differs by
// - detects side hit and inverts refractionIndex if necessary
// - 2sided face behaves as thin layer
RRVec3 refract(const RRVec3& I, const RRVec3& N, const RRMaterial* m)
{
	RR_ASSERT(m);
	if (!m || (m->sideBits[0].receiveFrom && m->sideBits[1].receiveFrom)) // testing .reflect instead of .receiveFrom broke caustic under glass sphere in scene5.mgf because updateSideBitsFromColors() sets .reflect even for back of 1sided face, sphere was treated as bubble
	{
		// 2sided faces simulate thin layer, don't change light direction
		return I;
	}
	RRReal ndoti = I.dot(N);
	RRReal eta = (ndoti<=0) ? m->refractionIndex : 1/m->refractionIndex; //!!! asi spatne, pokud jsem hitnul backside tak uz sem prisla obracena normala 
	return refract(I,N,eta);
}

// dirIn,dirNormal,dirOut -> result
static RRReal specularResponse(RRMaterial::SpecularModel specularModel, RRReal specularShininess, RRMaterial::Response& response, const RRVec3& dirInMajor)
{
	RRReal spec = 0;
	RRReal NH = response.dirNormal.dot((response.dirOut-response.dirIn).normalized());
	NH = RR_MAX(0,NH);
	switch (specularModel)
	{
		case RRMaterial::PHONG:
			// specularShininess in (0..inf)
			if (specularShininess>0)
			{
				//specularShininess = RR_MIN(specularShininess,MAX_SHININESS);
				spec = response.dirIn.dot(dirInMajor.normalized());
				spec = pow(RR_CLAMPED(spec,0,1),specularShininess) * (specularShininess+1);
			}
			else
			{
				// formula above does not converge to diffuse for shininess->0, so we force it at least for 0
				spec = 1; // still not identical to diffuse, but much closer
			}
			break;
		case RRMaterial::BLINN_PHONG:
			// specularShininess in (0..inf)
			spec = pow(NH,specularShininess) * (specularShininess+1);
			break;
		case RRMaterial::TORRANCE_SPARROW:
			// specularShininess=m^2=0..1
			spec = exp((1-1/(NH*NH))/specularShininess) / (4*specularShininess*((NH*NH)*(NH*NH))+0.0000000000001f);
			break;
		case RRMaterial::BLINN_TORRANCE_SPARROW:
			// specularShininess=c3^2=0..1
			spec = specularShininess/(NH*NH*(specularShininess-1)+1);
			spec = spec*spec;
			break;
		default:
			RR_ASSERT(0);
	}
	RR_ASSERT(_finite(spec));
	return spec;
}

// 0..1, fraction of specularTransmittance that is turned into specularReflectance
float getFresnelReflectance(float cos_theta1, bool twosided, bool hitFrontSide, float materialRefractionIndex)
{
	float index;
	if (!twosided)
		index = hitFrontSide?materialRefractionIndex:1.0f/materialRefractionIndex;
	else
		// treat 2sided face as thin layer: always start by hitting front side
		index = materialRefractionIndex;

	float cos_theta2 = sqrt( 1 - (1-cos_theta1*cos_theta1)/(index*index) );
	if (!_finite(cos_theta2)) // total internal reflection?
		return 1;
	float fresnel_rs = (cos_theta1-index*cos_theta2) / (cos_theta1+index*cos_theta2);
	float fresnel_rp = (index*cos_theta1-cos_theta2) / (index*cos_theta1+cos_theta2);
	float r = (fresnel_rs*fresnel_rs + fresnel_rp*fresnel_rp)*0.5f;
	if (!twosided)
		return r;
		// for cos_theta1=1, reflectance is ((1.0-materialRefractionIndex)/(1.0+materialRefractionIndex))^2
		// for materialRefractionIndex=1, reflectance is 0
	else
		// treat 2sided face as thin layer: take multiple interreflections between front and back into account
		return 2*r/(1+r);
}

// dirIn, dirNormal, dirOut -> colorOut
void RRMaterial::getResponse(Response& response, BrdfType type) const
{
	switch(type)
	{
		case BRDF_DIFFUSE:
			{
				RRReal dif = -response.dirIn.dot(response.dirNormal);
				RR_CLAMP(dif,0,1);
				response.colorOut = diffuseReflectance.colorLinear * dif; /* embree: /RR_PI */
				RR_ASSERT(response.colorOut.finite());
			}
			break;
		case BRDF_TRANSMIT:
			if (specularTransmittance.texture && specularTransmittance.colorLinear==RRVec3(1))
			{
				// hole in transparency texture, pass through without shininess and refraction
				response.colorOut = RRVec3((response.dirIn==response.dirOut)?1.f:0.f);
				break;
			}
			// intentionally no break
		case BRDF_SPECULAR:
			{
				RRVec3 specularReflectance_colorLinear = specularReflectance.colorLinear;
				RRVec3 specularTransmittance_colorLinear = specularTransmittance.colorLinear;

				// fresnel effect
				if (refractionIndex!=1)
				{
					RRReal dif = response.dirOut.dot(response.dirNormal);
					bool twoSided = sideBits[0].renderFrom && sideBits[1].renderFrom;
					bool hitFrontSide = dif>=0; //!!! asi spatne, pokud jsem hitnul backside tak uz sem prisla obracena normala
					float fresnelReflectance = getFresnelReflectance(dif,twoSided,hitFrontSide,refractionIndex);
					RR_CLAMP(fresnelReflectance,0,0.999f); // clamping to 1 in shader produces strange artifact
					specularReflectance_colorLinear += specularTransmittance.colorLinear*fresnelReflectance;
					specularTransmittance_colorLinear *= 1-fresnelReflectance;
				}

				RRVec3 dirInMajor = (type==BRDF_SPECULAR) ? reflect(response.dirOut,response.dirNormal) : refract(response.dirOut,response.dirNormal,this);
				// we have importance sampling implemented only for PHONG, so other models are temporarily converted to PHONG
				RRReal phongShininess = (specularModel==PHONG || specularModel==BLINN_PHONG) ? RR_MIN(specularShininess,MAX_SHININESS) : (1/RR_MAX(specularShininess,MIN_ROUGHNESS)-1);
				RRReal spec = specularResponse(PHONG,phongShininess,response,dirInMajor);
				//RRReal spec = specularResponse(specularModel,specularShininess,response,dirInMajor);
				response.colorOut = ((type==BRDF_SPECULAR) ? specularReflectance_colorLinear : specularTransmittance_colorLinear) * spec;
				RR_ASSERT(response.colorOut.finite());
			}
			break;
		case BRDF_NONE:
			response.colorOut = RRVec3(0);
			break;
		default:
			{	
				RR_ASSERT((unsigned)type<=BRDF_ALL);
				RRVec3 color(0);
				for (unsigned i=1;i<=BRDF_ALL;i*=2)
					if (type&i)
					{
						getResponse(response, (BrdfType)i);
						color += response.colorOut;
					}
				response.colorOut = color;
				RR_ASSERT(response.colorOut.finite());
			}
			break;
	}
	RR_ASSERT(response.colorOut.finite());
}

// w is probability distribution function
// (similar function is getRandomEnterDirNormalized, can be merged)
RRVec4 sampleHemisphereDiffuse(const RRVec2& uv, const RRVec3& normal)
{
	RRMesh::TangentBasis basisOrthonormal;
	basisOrthonormal.normal = normal;
	basisOrthonormal.buildBasisFromNormal();
	RRReal phi = 2*RR_PI*uv[0]; // phi = rotation angle around normal
	RRReal cosTheta = sqrt(uv[1]); // theta = rotation angle from normal to side
	RRReal sinTheta = sqrt(1 - uv[1]);
	return RRVec4(basisOrthonormal.normal*cosTheta + basisOrthonormal.tangent*(cos(phi)*sinTheta) + basisOrthonormal.bitangent*(sin(phi)*sinTheta), cosTheta /* embree: /RR_PI */);
}

RRVec4 sampleHemisphereSpecular(const RRVec2& uv, const RRVec3& dirInMajor, float exp)
{
	RRMesh::TangentBasis basisOrthonormal;
	basisOrthonormal.normal = dirInMajor;
	basisOrthonormal.buildBasisFromNormal();
	const float phi = 2*RR_PI*uv[0]; // phi = rotation angle around dirInMajor
	const float cosTheta = pow(uv[1],1/(exp+1)); // theta = rotation angle from dirInMajor to side
	const float sinTheta = sqrt(RR_MAX(0,1-cosTheta*cosTheta));
	return RRVec4(basisOrthonormal.normal*cosTheta + basisOrthonormal.tangent*(cos(phi)*sinTheta) + basisOrthonormal.bitangent*(sin(phi)*sinTheta), (exp+1)*pow(cosTheta,exp) /* embree: /(2*RR_PI) */);
}

// dirNormal, dirOut -> dirIn, colorOut, pdf, brdfType
void RRMaterial::sampleResponse(Response& response, const RRVec3& randomness, BrdfType type) const
{
	switch (type)
	{
		case BRDF_DIFFUSE:
			{
				RRVec4 sample = sampleHemisphereDiffuse(randomness,response.dirNormal);
				response.dirIn = -sample;
				response.pdf = sample.w;
				response.brdfType = type;
				getResponse(response,type);
			}
			break;
		case BRDF_TRANSMIT:
				if (specularTransmittance.texture && specularTransmittance.colorLinear==RRVec3(1))
				{
					// hole in transparency texture, pass through without shininess and refraction
					response.dirIn = response.dirOut;
					response.colorOut = RRVec3(1);
					response.pdf = 1;
					response.brdfType = type;
					break;
				}
				// intentionally no break
		case BRDF_SPECULAR:
			{
				RRVec3 dirInMajor = (type==BRDF_SPECULAR) ? reflect(response.dirOut,response.dirNormal) : refract(response.dirOut,response.dirNormal,this);
				// we have importance sampling implemented only for PHONG, so other models are temporarily converted to PHONG
				RRReal phongShininess = (specularModel==PHONG || specularModel==BLINN_PHONG) ? RR_MIN(specularShininess,MAX_SHININESS) : (1/RR_MAX(specularShininess,MIN_ROUGHNESS)-1);
				RRVec4 sample = sampleHemisphereSpecular(randomness,dirInMajor,phongShininess);
				response.dirIn = sample;
				response.pdf = sample.w;
				response.brdfType = type;
				//optimize: the same reflect()/refract() is called again in getResponse()
				getResponse(response,type);
				if (response.colorOut==RRVec3(0))
				{
					response.dirIn = dirInMajor;
					response.pdf = phongShininess+1;
					getResponse(response,type);
				}
			}
			break;
		case BRDF_NONE:
			none:
			response.dirIn = RRVec3(0);
			response.colorOut = RRVec3(0);
			response.pdf = 0;
			response.brdfType = type;
			break;
		default:
			{	
				if ((unsigned)type>BRDF_ALL)
				{
					RR_ASSERT(0);
					type = BRDF_ALL;
				}
				// sample selected brdfs
				unsigned numSamples = 0;
				Response sample[NUM_BRDFS];
				float weightSum = 0;
				for (unsigned i=1;i<=BRDF_ALL;i*=2)
					if (type&i)
					{
						sampleResponse(response, randomness, (BrdfType)i);
						if (response.colorOut!=RRVec3(0) && response.pdf>0)
						{
							sample[numSamples++] = response;
							weightSum += response.colorOut.maxi()/response.pdf;
						}
					}
				// early exit if none available
				if (!numSamples)
					goto none;
				// select one sample
				float weightAccumulator = 0;
				for (unsigned i=0;i<numSamples;i++)
				{
					float weight = sample[i].colorOut.maxi()/(sample[i].pdf*weightSum);
					weightAccumulator += weight;
					if (weightAccumulator>randomness.z || i+1==numSamples)
					{
						response = sample[i];
						response.pdf *= weight;
						break;
					}
				}
			}
			break;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RRPointMaterial

RRPointMaterial& RRPointMaterial::operator =(const RRMaterial& a)
{
	// memcpy creates shallow copy, destructor guarantees we won't delete things we don't own
	memcpy(this,&a,sizeof(a));
	return *this;
}

RRPointMaterial& RRPointMaterial::operator =(const RRPointMaterial& a)
{
	// memcpy creates shallow copy, destructor guarantees we won't delete things we don't own
	memcpy(this,&a,sizeof(a));
	return *this;
}

RRPointMaterial::~RRPointMaterial()
{
	// nullptr all pointers, so that ~RRMaterial has no work
	memset(&name,0,sizeof(name));
	diffuseReflectance.texture = nullptr;
	specularReflectance.texture = nullptr;
	diffuseEmittance.texture = nullptr;
	specularTransmittance.texture = nullptr;
	bumpMap.texture = nullptr;
	lightmap.texture = nullptr;
	preview = nullptr;
}

} // namespace
