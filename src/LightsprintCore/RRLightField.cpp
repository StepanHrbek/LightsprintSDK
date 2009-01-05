// --------------------------------------------------------------------------
// LightField, precomputed GI for dynamic objects + static lights
// Copyright (c) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <cstdio> // save/load
#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

#define LIGHTFIELD_STRUCTURE_VERSION 2 // change when file structure changes, old files will be overwritten

#define MAX(a,b)         (((a)>(b))?(a):(b))

//////////////////////////////////////////////////////////////////////////////
//
// LightFieldParameters

struct LightFieldParameters
{
	unsigned version;
	unsigned gridSize[4];
	unsigned diffuseSize;
	unsigned specularSize;
	RRVec4 aabbMin;
	RRVec4 aabbSize; // all must be >=0, at least one must be >0

	LightFieldParameters()
	{
		version = LIGHTFIELD_STRUCTURE_VERSION;
		gridSize[0] = 1;
		gridSize[1] = 1;
		gridSize[2] = 1;
		gridSize[3] = 1;
		diffuseSize = 4;
		specularSize = 16;
		aabbMin = RRVec4(0);
		aabbSize = RRVec4(1);
	}
	bool isOk() const
	{
		return version==LIGHTFIELD_STRUCTURE_VERSION && fieldSize() && aabbSize[0]>=0 && aabbSize[1]>=0 && aabbSize[2]>=0 && aabbSize[3]>=0 && (aabbSize[0]>0 || aabbSize[1]>0 || aabbSize[2]>0 || aabbSize[3]>0);
	}
	unsigned cellSize() const
	{
		return (diffuseSize*diffuseSize*6+specularSize*specularSize*6)*3;
	}
	unsigned fieldSize() const
	{
		return gridSize[0]*gridSize[1]*gridSize[2]*gridSize[3]*cellSize();
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// LightField

class LightField : public RRLightField
{
public:
	LightField()
	{
		rawField = NULL;
		rawCell = NULL;
		for (unsigned i=0;i<256;i++)
			customToPhysical[i] = (unsigned)pow(float(i),2.2222f);
	}
	virtual ~LightField()
	{
		delete[] rawField;
		delete[] rawCell;
	}

	virtual void captureLighting(class RRDynamicSolver* solver, unsigned timeSlot)
	{
		if (!solver) return;
		RRReportInterval report(INF2,"Filling lightfield %d*%d*%d dif=%d spec=%d size=%d.%dM...\n",header.gridSize[0],header.gridSize[1],header.gridSize[2],header.diffuseSize,header.specularSize,header.fieldSize()/1024/1024,(header.fieldSize()*10/1024/1024)%10);
		if (timeSlot>=header.gridSize[3])
		{
			RRReporter::report(WARN,"timeSlot %d is out of valid range 0..%d\n",timeSlot,header.gridSize[3]);
			return;
		}
		RRObjectIllumination objectIllum(0);
		//objectIllum.gatherEnvMapSize = 32; // default 16 is usually good enough
		if (header.diffuseSize) objectIllum.diffuseEnvMap = RRBuffer::create(BT_CUBE_TEXTURE,header.diffuseSize,header.diffuseSize,6,rr::BF_RGB,true,NULL);
		if (header.specularSize) objectIllum.specularEnvMap = RRBuffer::create(BT_CUBE_TEXTURE,header.specularSize,header.specularSize,6,rr::BF_RGB,true,NULL);
		for (unsigned k=0;k<header.gridSize[2];k++)
		for (unsigned j=0;j<header.gridSize[1];j++)
		for (unsigned i=0;i<header.gridSize[0];i++)
		{
			// update single cell in objectIllum
			objectIllum.envMapWorldCenter = RRVec3(header.aabbMin) + RRVec3(header.aabbSize) *
				RRVec3((i+0.5f)/header.gridSize[0],(j+0.5f)/header.gridSize[1],(k+0.5f)/header.gridSize[2]);
			solver->updateEnvironmentMap(&objectIllum);
			// copy single cell to grid
			unsigned cellIndex = i+header.gridSize[0]*(j+header.gridSize[1]*(k+timeSlot*header.gridSize[2]));
			if (objectIllum.diffuseEnvMap)
			{
				memcpy(rawField+cellIndex*header.diffuseSize*header.diffuseSize*6*3+cellIndex*header.specularSize*header.specularSize*6*3,
					objectIllum.diffuseEnvMap->lock(BL_READ),header.diffuseSize*header.diffuseSize*6*3);
				objectIllum.diffuseEnvMap->unlock();
			}
			if (objectIllum.specularEnvMap)
			{
				memcpy(rawField+(cellIndex+1)*header.diffuseSize*header.diffuseSize*6*3+cellIndex*header.specularSize*header.specularSize*6*3,
					objectIllum.specularEnvMap->lock(BL_READ),header.specularSize*header.specularSize*6*3);
				objectIllum.specularEnvMap->unlock();
			}
			// report progress
			enum {STEP=10000};
			if ((cellIndex%STEP)==STEP-1) RRReporter::report(INF3,"%d/%d\n",cellIndex/STEP+1,(header.gridSize[0]*header.gridSize[1]*header.gridSize[2])/STEP);
		}
	}

	virtual unsigned updateEnvironmentMap(RRObjectIllumination* object, RRReal time) const
	{
		if (!header.isOk() || !rawField || !rawCell || !object) return 0;

		// find cell in field (out of 16 cells that blend together, this one has minimal coords)
		//                              min                     max
		//  for xyz, capture points are  |.....|.....|.....|.....|
		//                                  0     1     2     3       <- capture points and cellCoordFloat
		//  for time, capture points are |...|.......|.......|...|
		//                               0       1       2       3    <- capture points and cellCoordFloat
		RRVec4 cellCoordFloat = RRVec4(
			(object->envMapWorldCenter-header.aabbMin  )/header.aabbSize  *RRVec3(RRReal(header.gridSize[0]),RRReal(header.gridSize[1]),RRReal(header.gridSize[2]))-RRVec3(0.5f),
			(time                     -header.aabbMin.w)/header.aabbSize.w*(header.gridSize[3]-1) );
		int cellCoordInt[4] = {int(cellCoordFloat[0]),int(cellCoordFloat[1]),int(cellCoordFloat[2]),int(cellCoordFloat[3])};
		RRVec4 cellCoordFraction = cellCoordFloat-RRVec4(RRReal(cellCoordInt[0]),RRReal(cellCoordInt[1]),RRReal(cellCoordInt[2]),RRReal(cellCoordInt[3]));
		for (unsigned i=0;i<4;i++)
		{
			if (cellCoordInt[i]<0 || cellCoordFraction[i]<0) {cellCoordInt[i]=0;cellCoordFraction[i]=0;}
			if (cellCoordInt[i]>=(int)header.gridSize[i]-1) {cellCoordInt[i]=header.gridSize[i]-1;cellCoordFraction[i]=0;}
		}

		// blend it with neighbour cells
		if (header.gridSize[3]==1)
		{
			// faster 3D blend
			unsigned cellSize = header.cellSize();
			unsigned numFields = header.gridSize[0]*header.gridSize[1]*header.gridSize[2];
			unsigned cellIndex = cellCoordInt[0]+cellCoordInt[1]*header.gridSize[0]+cellCoordInt[2]*header.gridSize[0]*header.gridSize[1];
			unsigned cellOffset[8];
			unsigned cellWeight[8];
			for (unsigned i=0;i<8;i++)
			{
				cellOffset[i] = cellSize*(( cellIndex+(i&1)+((i>>1)&1)*header.gridSize[0]+((i>>2)&1)*header.gridSize[0]*header.gridSize[1] )%numFields);
				cellWeight[i] = unsigned( 8192 * ((i&1)?cellCoordFraction[0]:1-cellCoordFraction[0]) * ((i&2)?cellCoordFraction[1]:1-cellCoordFraction[1]) * ((i&4)?cellCoordFraction[2]:1-cellCoordFraction[2]) );
			}
			for (unsigned i=0;i<cellSize;i++)
			{
				rawCell[i] = (unsigned)pow(float((
					cellWeight[0]*customToPhysical[rawField[cellOffset[0]+i]] +
					cellWeight[1]*customToPhysical[rawField[cellOffset[1]+i]] +
					cellWeight[2]*customToPhysical[rawField[cellOffset[2]+i]] +
					cellWeight[3]*customToPhysical[rawField[cellOffset[3]+i]] +
					cellWeight[4]*customToPhysical[rawField[cellOffset[4]+i]] +
					cellWeight[5]*customToPhysical[rawField[cellOffset[5]+i]] +
					cellWeight[6]*customToPhysical[rawField[cellOffset[6]+i]] +
					cellWeight[7]*customToPhysical[rawField[cellOffset[7]+i]] ) /8192 ), 0.45f);
			}
		}
		else
		{
			// slower 4D blend
			unsigned cellSize = header.cellSize();
			unsigned numFields = header.gridSize[0]*header.gridSize[1]*header.gridSize[2]*header.gridSize[3];
			unsigned cellIndex = cellCoordInt[0]+header.gridSize[0]*(cellCoordInt[1]+header.gridSize[1]*(cellCoordInt[2]+header.gridSize[2]*cellCoordInt[3]));
			unsigned cellOffset[16];
			unsigned cellWeight[16];
			for (unsigned i=0;i<16;i++)
			{
				cellOffset[i] =
					cellSize*((
						cellIndex
						+(i&1)
						+header.gridSize[0]*(
							((i>>1)&1)
							+ header.gridSize[1]*(
								((i>>2)&1)
								+ header.gridSize[2]*((i>>3)&1) 
							)
						)
					)%numFields);
				cellWeight[i] = unsigned( 8192 * ((i&1)?cellCoordFraction[0]:1-cellCoordFraction[0]) * ((i&2)?cellCoordFraction[1]:1-cellCoordFraction[1]) * ((i&4)?cellCoordFraction[2]:1-cellCoordFraction[2]) * ((i&8)?cellCoordFraction[3]:1-cellCoordFraction[3]) );
			}
			for (unsigned i=0;i<cellSize;i++)
			{
				rawCell[i] = (unsigned)pow(float((
					cellWeight[0]*customToPhysical[rawField[cellOffset[0]+i]] +
					cellWeight[1]*customToPhysical[rawField[cellOffset[1]+i]] +
					cellWeight[2]*customToPhysical[rawField[cellOffset[2]+i]] +
					cellWeight[3]*customToPhysical[rawField[cellOffset[3]+i]] +
					cellWeight[4]*customToPhysical[rawField[cellOffset[4]+i]] +
					cellWeight[5]*customToPhysical[rawField[cellOffset[5]+i]] +
					cellWeight[6]*customToPhysical[rawField[cellOffset[6]+i]] +
					cellWeight[7]*customToPhysical[rawField[cellOffset[7]+i]] +
					cellWeight[8]*customToPhysical[rawField[cellOffset[8]+i]] +
					cellWeight[9]*customToPhysical[rawField[cellOffset[9]+i]] +
					cellWeight[10]*customToPhysical[rawField[cellOffset[10]+i]] +
					cellWeight[11]*customToPhysical[rawField[cellOffset[11]+i]] +
					cellWeight[12]*customToPhysical[rawField[cellOffset[12]+i]] +
					cellWeight[13]*customToPhysical[rawField[cellOffset[13]+i]] +
					cellWeight[14]*customToPhysical[rawField[cellOffset[14]+i]] +
					cellWeight[15]*customToPhysical[rawField[cellOffset[15]+i]] ) /8192 ), 0.45f);
			}
		}

		// copy into buffers
		unsigned numUpdates = 0;
		if (object->diffuseEnvMap && header.diffuseSize)
		{
			object->diffuseEnvMap->reset(BT_CUBE_TEXTURE,header.diffuseSize,header.diffuseSize,6,BF_RGB,true,rawCell);
			numUpdates++;
		}
		if (object->specularEnvMap && header.specularSize)
		{
			object->specularEnvMap->reset(BT_CUBE_TEXTURE,header.specularSize,header.specularSize,6,BF_RGB,true,rawCell+header.diffuseSize*header.diffuseSize*6*3);
			numUpdates++;
		}
		return numUpdates;
	}

	// realloc arrays according to (new) header
	bool reallocData()
	{
		RR_SAFE_DELETE_ARRAY(rawField);
		RR_SAFE_DELETE_ARRAY(rawCell);
		try
		{
			rawField = new unsigned char[header.fieldSize()];
			rawCell = new unsigned char[header.cellSize()];
		}
		catch(...)
		{
			return false;
		}
		return true;
	}

	bool reload(const char* filename)
	{
		bool success = false;
		if (filename)
		{
			FILE* f = fopen(filename,"rb");
			if (f)
			{
				success = fread(&header,sizeof(header),1,f) && header.isOk() && reallocData() && fread(rawField,header.fieldSize(),1,f);
				fclose(f);
			}
		}
		return success;
	}

	virtual bool save(const char* filename) const
	{
		bool success = false;
		if (filename && header.isOk() && rawField)
		{
			FILE* f = fopen(filename,"wb");
			if (f)
			{
				success = fwrite(&header,sizeof(header),1,f) && fwrite(rawField,header.fieldSize(),1,f);
				fclose(f);
			}
		}
		return success;
	}

	LightFieldParameters header;
	unsigned char* rawField; // static array of precomputed cells
	unsigned char* rawCell; // temporary cell used by updateEnvironmentMap (for blending precomputed cells)
	unsigned customToPhysical[256]; // custom scale 8bit to phys scale ~18bit
};


//////////////////////////////////////////////////////////////////////////////
//
// RRLightField

RRLightField* RRLightField::create(RRVec4 aabbMin, RRVec4 aabbSize, RRReal spacing, unsigned diffuseSize, unsigned specularSize, unsigned numTimeSlots)
{
	LightField* lightField = new LightField();
	lightField->header.aabbMin = aabbMin;
	lightField->header.aabbSize = aabbSize;
	lightField->header.gridSize[0] = unsigned(MAX(1,(aabbSize[0]+spacing*0.5f)/spacing));
	lightField->header.gridSize[1] = unsigned(MAX(1,(aabbSize[1]+spacing*0.5f)/spacing));
	lightField->header.gridSize[2] = unsigned(MAX(1,(aabbSize[2]+spacing*0.5f)/spacing));
	lightField->header.gridSize[3] = numTimeSlots;
	lightField->header.diffuseSize = diffuseSize;
	lightField->header.specularSize = specularSize;
	if (!lightField->reallocData())
	{
		delete lightField;
		return NULL;
	}
	return lightField;
}

RRLightField* RRLightField::load(const char* filename)
{
	LightField* lightField = new LightField();
	if (!lightField->reload(filename))
	{
		RR_SAFE_DELETE(lightField);
	}
	return lightField;
}

} // namespace

