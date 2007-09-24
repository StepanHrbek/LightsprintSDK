#ifndef PACKEDSOLVERFILE_H
#define PACKEDSOLVERFILE_H

//#define THREADED_IMPROVE 2 // num threads, na dualcore nulovy prinos
//#define USE_SSE // aligned: memory=125%, speed=154/94
//#define USE_SSEU // unaligned: memory=105% speed=154/100 (koup/sponza)
//no-SSE: memory=100% speed=145/103

#define FIREBALL_STRUCTURE_VERSION 1 // change when file structere changes, old files will be overwritten
#define FIREBALL_FILENAME_VERSION  1 // change when file structere changes, old files will be preserved

#include <cstdio> // save/load
#include "RRPackedSolver.h" // THREADED_BEST, RRObject

#define SAFE_DELETE(a)   {delete a;a=NULL;}

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
	//! for(index1=0;index1<numC1;index1++)
	//! {
	//!   newC1(index1);
	//!   for(index2=0;index2<...;index2++) newC2();
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
		if(!f) return NULL;
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
		if(!data) return false;
		FILE* f = fopen(filename,"wb");
		if(!f) return false;
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
	//! Returns first C2 element in of n-th C1 element. Other C2 elements follow until getC2(n+1) is reached.
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
// form faktor

// 50% memory, 102% speed, 6bit precision, 128k triangles
class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>0);
		RR_ASSERT(_visibility<=1);
		RR_ASSERT(_destinationTriangle<(1<<BITS_FOR_TRIANGLE_INDEX));
		visibility = _visibility;
		destinationTriangle &= ~((1<<BITS_FOR_TRIANGLE_INDEX)-1);
		destinationTriangle |= _destinationTriangle;
	}
	RRReal getVisibility() const
	{
		return visibility;
	}
	const RRReal* getVisibilityPtr() const
	{
		return &visibility;
	}
	unsigned getDestinationTriangle() const
	{
		return destinationTriangle & ((1<<BITS_FOR_TRIANGLE_INDEX)-1);
	}
private:
	enum
	{
		BITS_FOR_TRIANGLE_INDEX = 17
	};
	union
	{
		RRReal visibility;
		unsigned destinationTriangle;
	};
};

/*
// 50% memory, 100% speed, ~10bit precision, 128k triangles
class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>0);
		RR_ASSERT(_visibility<=1);
		RR_ASSERT(_destinationTriangle<(1<<BITS_FOR_TRIANGLE_INDEX));
		visibility = (unsigned)(_visibility*((1<<BITS_FOR_VISIBILITY)-1));
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
private:
	enum
	{
		BITS_FOR_VISIBILITY = 15,
		BITS_FOR_TRIANGLE_INDEX = sizeof(unsigned)*8-BITS_FOR_VISIBILITY,
	};
	unsigned visibility          : BITS_FOR_VISIBILITY;
	unsigned destinationTriangle : BITS_FOR_TRIANGLE_INDEX;
};
/*
// 100% memory, 100% speed, 23bit precision, 4G triangles
class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>0);
		RR_ASSERT(_visibility<=1);
		visibility = _visibility;
		destinationTriangle = _destinationTriangle;
	}
	RRReal getVisibility() const
	{
		return visibility;
	}
	const RRReal* getVisibilityPtr() const
	{
		return &visibility;
	}
	unsigned getDestinationTriangle() const
	{
		return destinationTriangle;
	}
private:
	float visibility;
	unsigned destinationTriangle;
};
*/

//////////////////////////////////////////////////////////////////////////////
//
// PackedFactorsThread
//
// form faktory pro celou jednu statickou scenu

class PackedFactorHeader
{
public:
	unsigned arrayOffset;
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
	}
	unsigned getMemoryOccupied() const
	{
		return packedFactors->getMemoryOccupied() + packedIvertices->getMemoryOccupied() + packedSmoothTrianglesBytes;
	}
	bool save(const char* filename) const
	{
		// create file
		FILE* f = fopen(filename,"wb");
		if(!f) return false;
		// write data with invalid header
		Header header;
		header.version = 0xffffffff;
		header.packedFactorsBytes = packedFactors->getMemoryOccupied();
		header.packedIverticesBytes = packedIvertices->getMemoryOccupied();
		header.packedSmoothTrianglesBytes = packedSmoothTrianglesBytes;
		size_t ok = fwrite(&header,sizeof(header),1,f);
		ok += fwrite(packedFactors->getC1(0),packedFactors->getMemoryOccupied(),1,f);
		ok += fwrite(packedIvertices->getC1(0),packedIvertices->getMemoryOccupied(),1,f);
		ok += fwrite(packedSmoothTriangles,packedSmoothTrianglesBytes,1,f);
		// fix header
		if(ok==4)
		{
			header.version = FIREBALL_STRUCTURE_VERSION;
			fseek(f,0,SEEK_SET);
			fwrite(&header.version,sizeof(header.version),1,f);
		}
		fclose(f);
		return true;
	}
	static PackedSolverFile* load(const char* filename)
	{
		FILE* f = fopen(filename,"rb");
		if(!f) return NULL;
		Header header;
		if(fread(&header,sizeof(Header),1,f)!=1 || header.version!=FIREBALL_STRUCTURE_VERSION)
		{
			fclose(f);
			return NULL;
		}
		PackedSolverFile* psf = new PackedSolverFile;
		// load packedFactors
		char* tmp;
		size_t ok;
		tmp = new char[header.packedFactorsBytes];
		ok = fread(tmp,header.packedFactorsBytes,1,f);
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
		if(ok!=3) SAFE_DELETE(psf);
		return psf;
	}
	bool isCompatible(const RRObject* object) const
	{
		if(!object) return false;
		if(packedSmoothTrianglesBytes/sizeof(PackedSmoothTriangle)!=object->getCollider()->getMesh()->getNumTriangles()) return false;
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
protected:
	struct Header
	{
		unsigned version;
		unsigned packedFactorsBytes;
		unsigned packedIverticesBytes;
		unsigned packedSmoothTrianglesBytes;
	};
};

} // namespace

#endif
