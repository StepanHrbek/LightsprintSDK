// --------------------------------------------------------------------------
// LightField, precomputed GI for dynamic objects + static lights
// Copyright 2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <cstdio> // save/load
#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

#define LIGHTFIELD_STRUCTURE_VERSION 1 // change when file structure changes, old files will be overwritten

#define MAX(a,b)         (((a)>(b))?(a):(b))

//////////////////////////////////////////////////////////////////////////////
//
// LightFieldParameters

struct LightFieldParameters
{
	unsigned version;
	unsigned gridSize[3];
	unsigned diffuseSize;
	unsigned specularSize;
	RRVec3 aabbMin;
	RRVec3 aabbSize;

	LightFieldParameters()
	{
		version = LIGHTFIELD_STRUCTURE_VERSION;
		gridSize[0] = 1;
		gridSize[1] = 1;
		gridSize[2] = 1;
		diffuseSize = 4;
		specularSize = 16;
		aabbMin = RRVec3(0);
		aabbSize = RRVec3(1);
	}
	bool isOk() const
	{
		return version==LIGHTFIELD_STRUCTURE_VERSION && fieldSize() && aabbSize[0] && aabbSize[1] && aabbSize[2];
	}
	unsigned cellSize() const
	{
		return (diffuseSize*diffuseSize*6+specularSize*specularSize*6)*3;
	}
	unsigned fieldSize() const
	{
		return gridSize[0]*gridSize[1]*gridSize[2]*cellSize();
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
	}
	virtual ~LightField()
	{
		delete[] rawField;
		delete[] rawCell;
	}

	virtual unsigned updateEnvironmentMap(RRObjectIllumination* object) const
	{
		if(!header.isOk() || !rawField || !rawCell || !object) return 0;

		// find cell in field (out of 8 cells that blend together, this one has minimal coords)
		RRVec3 cellCoordFloat = (object->envMapWorldCenter-header.aabbMin)/header.aabbSize*RRVec3(RRReal(header.gridSize[0]),RRReal(header.gridSize[1]),RRReal(header.gridSize[2]))-RRVec3(0.5f);
		int cellCoordInt[3] = {int(cellCoordFloat[0]),int(cellCoordFloat[1]),int(cellCoordFloat[2])};
		RRVec3 cellCoordFraction = cellCoordFloat-RRVec3(RRReal(cellCoordInt[0]),RRReal(cellCoordInt[1]),RRReal(cellCoordInt[2]));
		for(unsigned i=0;i<3;i++)
		{
			if(cellCoordInt[i]<0 || cellCoordFraction[i]<0) {cellCoordInt[i]=0;cellCoordFraction[i]=0;}
			if(cellCoordInt[i]>=(int)header.gridSize[i]-1) {cellCoordInt[i]=header.gridSize[i]-1;cellCoordFraction[i]=0;}
		}

		// blend it with neighbour cells
		unsigned cellSize = header.cellSize();
		unsigned numFields = header.gridSize[0]*header.gridSize[1]*header.gridSize[2];
		unsigned cellIndex = cellCoordInt[0]+cellCoordInt[1]*header.gridSize[0]+cellCoordInt[2]*header.gridSize[0]*header.gridSize[1];
		unsigned cellOffset[8];
		unsigned cellWeight[8];
		for(unsigned i=0;i<8;i++)
		{
			cellOffset[i] = cellSize*(( cellIndex+(i&1)+((i>>1)&1)*header.gridSize[0]+((i>>2)&1)*header.gridSize[0]*header.gridSize[1] )%numFields);
			cellWeight[i] = unsigned( 65535 * ((i&1)?cellCoordFraction[0]:1-cellCoordFraction[0]) * ((i&2)?cellCoordFraction[1]:1-cellCoordFraction[1]) * ((i&4)?cellCoordFraction[2]:1-cellCoordFraction[2]) );
		}
		for(unsigned i=0;i<cellSize;i++)
		{
			rawCell[i] = (
				cellWeight[0]*rawField[cellOffset[0]+i] +
				cellWeight[1]*rawField[cellOffset[1]+i] +
				cellWeight[2]*rawField[cellOffset[2]+i] +
				cellWeight[3]*rawField[cellOffset[3]+i] +
				cellWeight[4]*rawField[cellOffset[4]+i] +
				cellWeight[5]*rawField[cellOffset[5]+i] +
				cellWeight[6]*rawField[cellOffset[6]+i] +
				cellWeight[7]*rawField[cellOffset[7]+i] ) >> 16;
		}

		// copy into buffers
		unsigned numUpdates = 0;
		if(object->diffuseEnvMap && header.diffuseSize)
		{
			object->diffuseEnvMap->reset(BT_CUBE_TEXTURE,header.diffuseSize,header.diffuseSize,6,BF_RGB,true,rawCell);
			numUpdates++;
		}
		if(object->specularEnvMap && header.specularSize)
		{
			object->specularEnvMap->reset(BT_CUBE_TEXTURE,header.specularSize,header.specularSize,6,BF_RGB,true,rawCell+header.diffuseSize*header.diffuseSize*6*3);
			numUpdates++;
		}
		return numUpdates;
	}

	// realloc arrays according to (new) header
	bool reallocData()
	{
		SAFE_DELETE_ARRAY(rawField);
		SAFE_DELETE_ARRAY(rawCell);
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
		if(filename)
		{
			FILE* f = fopen(filename,"rb");
			if(f)
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
		if(filename && header.isOk() && rawField)
		{
			FILE* f = fopen(filename,"wb");
			if(f)
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
};


//////////////////////////////////////////////////////////////////////////////
//
// RRLightField

const RRLightField* RRLightField::load(const char* filename)
{
	LightField* lightField = new LightField();
	if(!lightField->reload(filename))
	{
		SAFE_DELETE(lightField);
	}
	return lightField;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolver

const RRLightField* RRDynamicSolver::buildLightField(RRVec3 aabbMin, RRVec3 aabbSize, RRReal spacing, unsigned diffuseSize, unsigned specularSize)
{
	LightField* lightField = new LightField();
	lightField->header.aabbMin = aabbMin;
	lightField->header.aabbSize = aabbSize;
	lightField->header.gridSize[0] = unsigned(MAX(1,(aabbSize[0]+spacing*0.5f)/spacing));
	lightField->header.gridSize[1] = unsigned(MAX(1,(aabbSize[1]+spacing*0.5f)/spacing));
	lightField->header.gridSize[2] = unsigned(MAX(1,(aabbSize[2]+spacing*0.5f)/spacing));
	RRReportInterval report(INF2,"Building lightfield %d*%d*%d dif=%d spec=%d size=%d.%dM...\n",lightField->header.gridSize[0],lightField->header.gridSize[1],lightField->header.gridSize[2],diffuseSize,specularSize,lightField->header.fieldSize()/1024/1024,(lightField->header.fieldSize()*10/1024/1024)%10);
	lightField->header.diffuseSize = diffuseSize;
	lightField->header.specularSize = specularSize;
	if(!lightField->reallocData())
	{
		delete lightField;
		return NULL;
	}
	RRObjectIllumination objectIllum(0);
	//objectIllum.gatherEnvMapSize = 32; // default 16 is usually good enough
	if(diffuseSize) objectIllum.diffuseEnvMap = RRBuffer::create(BT_CUBE_TEXTURE,diffuseSize,diffuseSize,6,rr::BF_RGB,true,NULL);
	if(specularSize) objectIllum.specularEnvMap = RRBuffer::create(BT_CUBE_TEXTURE,specularSize,specularSize,6,rr::BF_RGB,true,NULL);
	for(unsigned k=0;k<lightField->header.gridSize[2];k++)
	for(unsigned j=0;j<lightField->header.gridSize[1];j++)
	for(unsigned i=0;i<lightField->header.gridSize[0];i++)
	{
		// update single cell in objectIllum
		objectIllum.envMapWorldCenter = aabbMin + aabbSize *
			RRVec3((i+0.5f)/lightField->header.gridSize[0],(j+0.5f)/lightField->header.gridSize[1],(k+0.5f)/lightField->header.gridSize[2]);
		updateEnvironmentMap(&objectIllum);
		// copy single cell to grid
		unsigned cellIndex = i+j*lightField->header.gridSize[0]+k*lightField->header.gridSize[0]*lightField->header.gridSize[1];
		if(objectIllum.diffuseEnvMap)
		{
			memcpy(lightField->rawField+cellIndex*diffuseSize*diffuseSize*6*3+cellIndex*specularSize*specularSize*6*3,
				objectIllum.diffuseEnvMap->lock(BL_READ),diffuseSize*diffuseSize*6*3);
			objectIllum.diffuseEnvMap->unlock();
		}
		if(objectIllum.specularEnvMap)
		{
			memcpy(lightField->rawField+(cellIndex+1)*diffuseSize*diffuseSize*6*3+cellIndex*specularSize*specularSize*6*3,
				objectIllum.specularEnvMap->lock(BL_READ),specularSize*specularSize*6*3);
			objectIllum.specularEnvMap->unlock();
		}
	}
	return lightField;
}

} // namespace

