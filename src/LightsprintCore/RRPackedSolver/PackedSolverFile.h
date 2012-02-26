// --------------------------------------------------------------------------
// Fireball, lightning fast realtime GI solver.
// Copyright (c) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef PACKEDSOLVERFILE_H
#define PACKEDSOLVERFILE_H

//#define THREADED_IMPROVE 4 // num threads, na dualcore nulovy prinos, na 4core 3% zpomaleni

#define FACTOR_FORMAT 2 // 0: 32bit int/float overlap (lightsmark); 1: 32bit short+short; 2: 64bit int+float (SDK)

#define FIREBALL_STRUCTURE_VERSION (9+FACTOR_FORMAT) // change when file structere changes, old files will be overwritten
#define FIREBALL_FILENAME_VERSION  2 // change when file structere changes, old files will be preserved

#include <cstdio> // save/load
#include "RRPackedSolver.h" // THREADED_BEST, RRObject

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// ArrayWithArrays
//
// Array of C1 with each element owning variable length array of C2.
// In single memory block.
// Doesn't call constructors/destructors.
// unsigned C1::arrayOffset must exist.

template <class C1, class C2>
class ArrayWithArrays
{
public:
	//! Constructor used when complete array is acquired (e.g. loaded from disk).
	//! Block of data is adopted and delete[]d in destructor.
	ArrayWithArrays(char* _adopt_data, unsigned _sizeInBytes)
	{
		data = _adopt_data;
		sizeInBytes = _sizeInBytes; // full size
		numC1 = data ? getC1(0)->arrayOffset/sizeof(C1)-1 : 0;
	}
	//! Constructor used when empty array is created and then filled with newC1, newC2.
	//
	//! Size must be known in advance.
	//! Filling scheme:
	//! for (index1=0;index1<numC1;index1++)
	//! {
	//!   newC1(index1);
	//!   for (index2=0;index2<...;index2++) newC2();
	//! }
	//! newC1(numC1);
	ArrayWithArrays(unsigned _numC1, unsigned _numC2)
	{
		data = new char[(_numC1+1)*sizeof(C1)+_numC2*sizeof(C2)];
		sizeInBytes = (_numC1+1)*sizeof(C1); // point to first C2, will be increased by newC2
		numC1 = _numC1;
	}
	/*/! Loads instance from disk.
	static ArrayWithArrays* load(const char* filename)
	{
		FILE* f = fopen(filename,"rb");
		if (!f) return NULL;
		fseek(f,0,SEEK_END);
		unsigned sizeInBytes = ftell(f);
		char* data = new char[sizeInBytes];
		fseek(f,0,SEEK_SET);
		fread(data,1,sizeInBytes,f);
		fclose(f);
		return new ArrayWithArrays(data,sizeInBytes);
	}
	//! Saves instance to disk, returns success.
	bool save(const char* filename) const
	{
		if (!data) return false;
		FILE* f = fopen(filename,"wb");
		if (!f) return false;
		fwrite(data,sizeInBytes,1,f);
		fclose(f);
		return true;
	}*/
	unsigned getNumC1()
	{
		return numC1;
	}
	//! Returns n-th C1 element.
	C1* getC1(unsigned index1)
	{
		RR_ASSERT(index1<=numC1);
		return (C1*)data+index1;
	}
	//! Returns first C2 element of n-th C1 element. Other C2 elements follow until getC2(n+1) is reached.
	C2* getC2(unsigned index1)
	{
		RR_ASSERT(index1<=numC1);
		return (C2*)(data+getC1(index1)->arrayOffset);
	}
	//! Allocates C1 element. To be used only when array is dynamically created, see constructor for details.
	C1* newC1(unsigned index1)
	{
		RR_ASSERT(index1<=numC1);
		getC1(index1)->arrayOffset = sizeInBytes;
		return getC1(index1);
	}
	//! Allocates C2 element. To be used only when array is dynamically created, see constructor for details.
	C2* newC2()
	{
		sizeInBytes += sizeof(C2);
		return (C2*)(data+sizeInBytes-sizeof(C2));
	}
	//! Size of our single memory block. Don't use when array is not fully filled.
	unsigned getMemoryOccupied() const
	{
		return sizeInBytes;
	}
	~ArrayWithArrays()
	{
		delete[] data;
	}
protected:
	// format dat v jednom souvislem bloku:
	//   C1 c1[numC1+1];
	//   C2 c2[numC2];
	char* data;
	unsigned sizeInBytes;
	unsigned numC1;
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedFactor
//
// triangle-triangle form faktor

#if FACTOR_FORMAT==0
// 50% memory, 102% speed, 6bit precision, 128k triangles
class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>0);
		//RR_ASSERT(_visibility<=1); above 1 is ok in presence of specular reflectance/transmittance
		RR_ASSERT(_destinationTriangle<(1<<BITS_FOR_TRIANGLE_INDEX));
		visibility = _visibility;
		destinationTriangle &= ~((1<<BITS_FOR_TRIANGLE_INDEX)-1);
		destinationTriangle |= _destinationTriangle;
	}
	RRReal getVisibility() const
	{
		return visibility;
	}
	const RRReal* getVisibilityPtr() const // only for SSE version
	{
		return &visibility;
	}
	unsigned getDestinationTriangle() const
	{
		return destinationTriangle & ((1<<BITS_FOR_TRIANGLE_INDEX)-1);
	}
	enum
	{
		BITS_FOR_TRIANGLE_INDEX = 17,
		MAX_TRIANGLES = 1 << BITS_FOR_TRIANGLE_INDEX,
	};
private:
	union
	{
		RRReal visibility;
		unsigned destinationTriangle;
	};
};
#endif

#if FACTOR_FORMAT==1
// 50% memory, 100% speed, ~10bit precision, 128k triangles
class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>0);
		//RR_ASSERT(_visibility<=1); above 1 is ok in presence of specular reflectance/transmittance
		RR_ASSERT(_destinationTriangle<(1<<BITS_FOR_TRIANGLE_INDEX));
		visibility = (unsigned)(RR_CLAMPED(_visibility,0,1)*((1<<BITS_FOR_VISIBILITY)-1));
		RR_ASSERT(visibility);
		destinationTriangle = _destinationTriangle;
	}
	RRReal getVisibility() const
	{
		return visibility * (1.f/((1<<BITS_FOR_VISIBILITY)-1));
	}
	unsigned getDestinationTriangle() const
	{
		return destinationTriangle;
	}
	enum
	{
		BITS_FOR_VISIBILITY = 15,
		BITS_FOR_TRIANGLE_INDEX = sizeof(unsigned)*8-BITS_FOR_VISIBILITY,
		MAX_TRIANGLES = 1 << BITS_FOR_TRIANGLE_INDEX,
	};
private:
	unsigned visibility          : BITS_FOR_VISIBILITY;
	unsigned destinationTriangle : BITS_FOR_TRIANGLE_INDEX;
};
#endif

#if FACTOR_FORMAT==2
// 100% memory, 100% speed, 23bit precision, 4G triangles
class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>0);
		//RR_ASSERT(_visibility<=1); above 1 is ok in presence of specular reflectance/transmittance
		visibility = _visibility;
		destinationTriangle = _destinationTriangle;
	}
	RRReal getVisibility() const
	{
		return visibility;
	}
	const RRReal* getVisibilityPtr() const // only for SSE version
	{
		return &visibility;
	}
	unsigned getDestinationTriangle() const
	{
		return destinationTriangle;
	}
	enum
	{
		MAX_TRIANGLES = UINT_MAX,
	};
private:
	float visibility;
	unsigned destinationTriangle;
};
#endif


//////////////////////////////////////////////////////////////////////////////
//
// PackedSkyTriangleFactor
//
// sky-triangle form factor

//! SkyTriangleFactor consists of SkyTriangleFactorColor and NUM_PATCHES PatchTriangleFactors
class PackedSkyTriangleFactor
{
public:
	//! Sky is divided into patches that emit light separately
	enum
	{
		NUM_PATCHES=13,
		NUM_EQUAL_SIZE_PATCHES=24
	};

	struct UnpackedFactor
	{
		RRVec3 patches[NUM_PATCHES];

		UnpackedFactor()
		{
			for (unsigned p=0;p<NUM_PATCHES;p++)
				patches[p] = RRVec3(0);
		}
	};

	//! Sets sky-triangle configuration factor.
	//! Returns relative and absolute compression error, how much compressed+decompressed factors differs from original ones.
	RRVec2 setSkyTriangleFactor(const UnpackedFactor& factor, const float* intensityTable)
	{
		RRVec3 color(0);
		for (unsigned p=0;p<NUM_PATCHES;p++)
		{
			color += factor.patches[p];
		}
		if (color.sum()==0)
		{
			// triangle is completely black, not affected by skylight
			r = 0; // r and g are irrelevant at intensity 0, but let's zero them anyway
			g = 0;
			for (unsigned p=0;p<NUM_PATCHES;p++)
			{
				compressedPatchIntensities[p] = 0;
			}
			return RRVec2(0,0);
		}
		// triangle is affected by skylight
		// - compress color
		color *= 255/color.sum();
		r = (unsigned char)color[0];
		g = (unsigned char)color[1];
		// - compress intensities
		color = getSkyTriangleFactorColor();
		float relativeError = 0;
		float absoluteError = 0;
		for (unsigned p=0;p<NUM_PATCHES;p++)
		{
			// compress and store intensity
			float intensity = factor.patches[p].sum()/color.sum();
			compressedPatchIntensities[p] = getCompressedIntensity(intensity,intensityTable);
			// decompress factor and measure error
			RRVec3 decompressedPatchFactor = getSkyTriangleFactorColor() * intensityTable[compressedPatchIntensities[p]];
			relativeError += getRelativeError(factor.patches[p][0],decompressedPatchFactor[0])+getRelativeError(factor.patches[p][1],decompressedPatchFactor[1])+getRelativeError(factor.patches[p][2],decompressedPatchFactor[2]);
			absoluteError += getAbsoluteError(factor.patches[p][0],decompressedPatchFactor[0])+getAbsoluteError(factor.patches[p][1],decompressedPatchFactor[1])+getAbsoluteError(factor.patches[p][2],decompressedPatchFactor[2]);
		}
		relativeError /= 3*NUM_PATCHES;
		absoluteError /= 3*NUM_PATCHES;
		return RRVec2(relativeError,absoluteError);
	}

	//! Returns triangle's GI from sky computed as a dot product of patch exitances and patch factors.
	RRVec3 getTriangleIrradiancePhysical(const UnpackedFactor& patchExitancesPhysical, const float* intensityTable) const
	{
		RRVec3 color(0);
		for (unsigned p=0;p<NUM_PATCHES;p++)
		{
			color += patchExitancesPhysical.patches[p] * intensityTable[compressedPatchIntensities[p]];
		}
		color *= getSkyTriangleFactorColor();
		return color;
	}
	//! Returns sky patch index for given direction.
	static unsigned getPatchIndex(const RRVec3& direction)
	{
		// find major axis
		RRVec3 d = direction.abs();
		unsigned axis = (d[0]>=d[1] && d[0]>=d[2]) ? 0 : ( (d[1]>=d[0] && d[1]>=d[2]) ? 1 : 2 ); // 0..2
		// generate index 0..23
		unsigned index = axis +
			((direction.x>0)?3:0) +
			((direction.z>0)?6:0) +
			((direction.y<0)?12:0);
		// make everything below ground patch 12
		if (index>12) index = 12;
		RR_ASSERT(index<NUM_PATCHES);
		return index;
	}

private:
	RRVec3 getSkyTriangleFactorColor() const
	{
		return RRVec3((RRReal)r,(RRReal)g,(RRReal)(255-r-g));
	}
	//! Returns relative error caused by using approximated number. E.g. using 1.8 instead of 2 = 0.1 error.
	static float getRelativeError(float precise, float approximated)
	{
		if (!precise || !approximated || precise*approximated<0)
			return (precise==approximated)?0.0f:1.0f;
		precise = fabs(precise);
		approximated = fabs(approximated);
		float error = 1-RR_MIN(precise,approximated)/RR_MAX(precise,approximated);
		return error;
	}
	//! Returns absolute error caused by using approximated number. E.g. using 1.8 instead of 2 = 0.2 error.
	static float getAbsoluteError(float precise, float approximated)
	{
		float error = fabs(precise-approximated);
		return error;
	}
	//! Returns intensity compressed to index into intensityTable.
	static unsigned char getCompressedIntensity(float intensity, const float* intensityTable)
	{
		for (unsigned i=256;i--;)
		{
			if (intensity>intensityTable[i])
			{
				if (i==255 || fabs(intensity-intensityTable[i])<fabs(intensity-intensityTable[i+1]))
					return i;
				return i+1;
			}
		}
		return 0;
	}

	//! Color of triangle's GI AO, b=255-r-g
	unsigned char r;
	unsigned char g;
	//! Intensity indices of intensity of GI from individual sky patches.
	unsigned char compressedPatchIntensities[NUM_PATCHES];
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedFactorsThread
//
// form factors for one static scene, one header per triangle

class PackedFactorHeader
{
public:
	unsigned arrayOffset;

	PackedSkyTriangleFactor packedSkyTriangleFactor;
};

typedef ArrayWithArrays<PackedFactorHeader,PackedFactor> PackedFactorsThread;


//////////////////////////////////////////////////////////////////////////////
//
// PackedFactorsProcess
//



//////////////////////////////////////////////////////////////////////////////
//
// PackedIvertices, PackedSmoothTriangle[]

class PackedSmoothIvertex
{
public:
	unsigned arrayOffset;
};

class PackedSmoothTriangleWeight
{
public:
	unsigned triangleIndex;
	RRReal weight;
};

//! Precomputed data stored for each ivertex.
typedef ArrayWithArrays<PackedSmoothIvertex,PackedSmoothTriangleWeight> PackedIvertices;

//! Precomputed data stored for each triangle.
class PackedSmoothTriangle
{
public:
	unsigned ivertexIndex[3];
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedSolverFile

class PackedSolverFile
{
public:
	PackedSolverFile()
	{
		packedFactors = NULL;
		packedIvertices = NULL;
		packedSmoothTriangles = NULL;
		packedSmoothTrianglesBytes = 0;

		// build table so that intensity goes up to 0.023,
		// intensity*color(white=85,85,85) goes up to 2
		// usually 1 is enough, more is possible only with mirrors concentrating skylight
		for (unsigned i=1;i<256;i++)
			//intensityTable[i] = (float)(i*0.0033);
			intensityTable[i] = (float)(1.7e-8*pow(2.0,i*0.08));
		intensityTable[0] = 0;
	}
	unsigned getMemoryOccupied() const
	{
		return packedFactors->getMemoryOccupied() + packedIvertices->getMemoryOccupied() + packedSmoothTrianglesBytes;
	}
	bool save(const char* filename, const RRHash& hash) const
	{
		// create file
		FILE* f = fopen(filename,"wb");
		if (!f) return false;
		// save invalid header (because file contents is incomplete ATM)
		Header header;
		header.version = 0xffffffff;
		header.packedFactorsBytes = packedFactors->getMemoryOccupied();
		header.packedIverticesBytes = packedIvertices->getMemoryOccupied();
		header.packedSmoothTrianglesBytes = packedSmoothTrianglesBytes;
		header.hash = hash;
		size_t ok = fwrite(&header,sizeof(header),1,f);
		// save intensityTable
		ok += fwrite(intensityTable,sizeof(intensityTable),1,f);
		// save packedFactors
		ok += fwrite(packedFactors->getC1(0),packedFactors->getMemoryOccupied(),1,f);
		// save packedIvertices
		ok += fwrite(packedIvertices->getC1(0),packedIvertices->getMemoryOccupied(),1,f);
		// save packedSmoothTriangles
		ok += fwrite(packedSmoothTriangles,packedSmoothTrianglesBytes,1,f);
		// save valid header
		if (ok==5)
		{
			header.version = FIREBALL_STRUCTURE_VERSION;
			fseek(f,0,SEEK_SET);
			fwrite(&header.version,sizeof(header.version),1,f);
		}
		fclose(f);
		return true;
	}
	static PackedSolverFile* load(const char* filename, const RRHash* hashThatMustMatch)
	{
		// open file
		FILE* f = fopen(filename,"rb");
		if (!f) return NULL;
		// load header
		Header header;
		if (fread(&header,sizeof(Header),1,f)!=1 || header.version!=FIREBALL_STRUCTURE_VERSION || (hashThatMustMatch && header.hash!=*hashThatMustMatch))
		{
			fclose(f);
			return NULL;
		}
		PackedSolverFile* psf = new PackedSolverFile;
		// load intensityTable
		size_t ok = 0;
		ok += fread(psf->intensityTable,sizeof(psf->intensityTable),1,f);
		// load packedFactors
		char* tmp;
		tmp = new char[header.packedFactorsBytes];
		ok += fread(tmp,header.packedFactorsBytes,1,f);
		psf->packedFactors = new PackedFactorsThread(tmp,header.packedFactorsBytes);
		// load packedIvertices
		tmp = new char[header.packedIverticesBytes];
		ok += fread(tmp,header.packedIverticesBytes,1,f);
		psf->packedIvertices = new PackedIvertices(tmp,header.packedIverticesBytes);
		// load packedSmoothTriangles
		PackedSmoothTriangle* tmp2 = new PackedSmoothTriangle[(header.packedSmoothTrianglesBytes+sizeof(PackedSmoothTriangle)-1)/sizeof(PackedSmoothTriangle)];
		ok += fread(tmp2,header.packedSmoothTrianglesBytes,1,f);
		psf->packedSmoothTriangles = tmp2;
		psf->packedSmoothTrianglesBytes = header.packedSmoothTrianglesBytes;
		// done
		fclose(f);
		if (ok!=4) RR_SAFE_DELETE(psf);
		return psf;
	}
	bool isCompatible(const RRObject* object) const
	{
		if (!object) return false;
		if (packedSmoothTrianglesBytes/sizeof(PackedSmoothTriangle)!=object->getCollider()->getMesh()->getNumTriangles()) return false;
		return true;
	}
	~PackedSolverFile()
	{
		delete packedFactors;
		delete packedIvertices;
		delete packedSmoothTriangles;
	}
	PackedFactorsThread* packedFactors;
	PackedIvertices* packedIvertices;
	PackedSmoothTriangle* packedSmoothTriangles;
	unsigned packedSmoothTrianglesBytes;
	float intensityTable[256]; // fixed table used to compress/decompress sky-tri factors
protected:
	struct Header
	{
		unsigned version;
		unsigned packedFactorsBytes;
		unsigned packedIverticesBytes;
		unsigned packedSmoothTrianglesBytes;
		RRHash hash; // we want to save only hash value, luckily RRHash contains only value
	};
};

} // namespace

#endif
