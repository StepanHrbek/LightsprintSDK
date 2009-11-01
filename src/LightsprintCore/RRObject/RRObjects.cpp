// --------------------------------------------------------------------------
// Collection of objects.
// Copyright (c) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include "Lightsprint/RRObject.h"

namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRObjects

static char *bp(const char *fmt, ...)
{
	static char msg[1000];
	va_list argptr;
	va_start (argptr,fmt);
	_vsnprintf (msg,999,fmt,argptr);
	msg[999] = 0;
	va_end (argptr);
	return msg;
}

static bool exists(const char* filename)
{
	FILE* f = fopen(filename,"rb");
	if (!f) return false;
	fclose(f);
	return true;
}

// formats filename from prefix(path), object number and postfix(ext)
const char* formatFilename(const char* path, unsigned objectIndex, const char* ext, bool isVertexBuffer)
{
	char* tmp = NULL;
	const char* finalExt;
	if (isVertexBuffer)
	{
		if (!ext)
		{
			finalExt = "vbu";
		}
		else
		{
			// transforms ext "bent_normals.png" to finalExt "bent_normals.vbu"
			tmp = new char[strlen(ext)+5];
			strcpy(tmp,ext);
			unsigned i = (unsigned)strlen(ext);
			while (i && tmp[i-1]!='.') i--;
			strcpy(tmp+i,"vbu");
			finalExt = tmp;
		}
	}
	else
	{
		if (!ext)
		{
			finalExt = "png";
		}
		else
		{
			finalExt = ext;
		}
	}
	const char* result = bp("%s%05d.%s",path?path:"",objectIndex,finalExt);
	delete[] tmp;
	return result;
}

void RRObjects::recommendLayerParameters(RRObjects::LayerParameters& layerParameters) const
{
	RR_ASSERT((unsigned)layerParameters.objectIndex<size());
	layerParameters.actualWidth = layerParameters.suggestedMapSize ? layerParameters.suggestedMapSize : (*this)[layerParameters.objectIndex]->getCollider()->getMesh()->getNumPreImportVertices();
	layerParameters.actualHeight = layerParameters.suggestedMapSize ? layerParameters.suggestedMapSize : 1;
	layerParameters.actualType = layerParameters.suggestedMapSize ? BT_2D_TEXTURE : BT_VERTEX_BUFFER;
	layerParameters.actualFormat = layerParameters.suggestedMapSize ? BF_RGB : BF_RGBF;
	layerParameters.actualScaled = layerParameters.suggestedMapSize ? true : false;
	free(layerParameters.actualFilename);
	layerParameters.actualFilename = _strdup(formatFilename(layerParameters.suggestedPath,layerParameters.objectIndex,layerParameters.suggestedExt,layerParameters.actualType==BT_VERTEX_BUFFER));
}


unsigned RRObjects::loadLayer(int layerNumber, const char* path, const char* ext) const
{
	unsigned result = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRObjectIllumination* illumination = (*this)[objectIndex]->illumination;
			if (illumination)
			{
				// first try to load per-pixel format
				RRBuffer* buffer = NULL;
				RRObjects::LayerParameters layerParameters;
				layerParameters.objectIndex = objectIndex;
				layerParameters.suggestedPath = path;
				layerParameters.suggestedExt = ext;
				layerParameters.suggestedMapSize = 256;
				recommendLayerParameters(layerParameters);
				if ( !exists(layerParameters.actualFilename) || !(buffer=RRBuffer::load(layerParameters.actualFilename,NULL)) )
				{
					// if it fails, try to load per-vertex format
					layerParameters.suggestedMapSize = 0;
					recommendLayerParameters(layerParameters);
					if (exists(layerParameters.actualFilename))
						buffer = RRBuffer::load(layerParameters.actualFilename);
				}
				if (buffer && buffer->getType()==BT_VERTEX_BUFFER && buffer->getWidth()!=illumination->getNumPreImportVertices())
				{
					RR_LIMITED_TIMES(5,RRReporter::report(ERRO,"%s has wrong size.\n",layerParameters.actualFilename));
					RR_SAFE_DELETE(buffer);
				}
				if (buffer)
				{
					delete illumination->getLayer(layerNumber);
					illumination->getLayer(layerNumber) = buffer;
					result++;
					RRReporter::report(INF3,"Loaded %s.\n",layerParameters.actualFilename);
				}
				else
				{
					RRReporter::report(INF3,"Not loaded %s.\n",layerParameters.actualFilename);
				}
			}
		}
		RRReporter::report(INF2,"Loaded layer %d, %d/%d buffers into %s.\n",layerNumber,result,size(),path);
	}
	return result;
}

unsigned RRObjects::saveLayer(int layerNumber, const char* path, const char* ext) const
{
	unsigned result = 0;
	if (layerNumber>=0)
	{
		for (unsigned objectIndex=0;objectIndex<size();objectIndex++)
		{
			RRBuffer* buffer = (*this)[objectIndex]->illumination ? (*this)[objectIndex]->illumination->getLayer(layerNumber) : NULL;
			if (buffer)
			{
				RRObjects::LayerParameters layerParameters;
				layerParameters.objectIndex = objectIndex;
				layerParameters.suggestedPath = path;
				layerParameters.suggestedExt = ext;
				layerParameters.suggestedMapSize = (buffer->getType()==BT_VERTEX_BUFFER) ? 0 : 256;
				recommendLayerParameters(layerParameters);
				if (buffer->save(layerParameters.actualFilename))
				{
					result++;
					RRReporter::report(INF3,"Saved %s.\n",layerParameters.actualFilename);
				}
				else
					RRReporter::report(WARN,"Not saved %s.\n",layerParameters.actualFilename);
			}
		}
		RRReporter::report(INF2,"Saved layer %d, %d/%d buffers into %s.\n",layerNumber,result,size(),path);
	}
	return result;
}


} // namespace
