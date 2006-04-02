#include <assert.h>
#include <float.h> // _finite
#include <time.h>
#include "RRVisionApp.h"

namespace rrVision
{

#define TIME    clock_t
#define GETTIME clock()
#define PER_SEC CLOCKS_PER_SEC

#define PAUSE_AFTER_INTERACTION 0.3f
#define CALC_STEP 0.1f

RRAppFramework::RRAppFramework()
{
	surfaces = NULL;
	scene = NULL;
	resultChannelIndex = 0;
	dirtyMaterials = true;
	dirtyGeometry = true;
	dirtyLights = true;
	calcTimeSinceReadingResults = 0;
}

RRAppFramework::~RRAppFramework()
{
	delete scene;
	delete surfaces;
}

void RRAppFramework::setObjects(Objects& aobjects)
{
	objects = aobjects;
	dirtyGeometry = true;
}

void RRAppFramework::setResultChannel(unsigned channelIndex)
{
	resultChannelIndex = channelIndex;
}

void RRAppFramework::adjustScene()
{
	scene->setScaler(RRScaler::createRgbScaler());
}

void RRAppFramework::reportAction(const char* action) const
{
}

void RRAppFramework::reportMaterialChange()
{
	dirtyMaterials = true;
}

void RRAppFramework::reportGeometryChange()
{
	dirtyGeometry = true;
}

void RRAppFramework::reportLightChange()
{
	dirtyLights = true;
}

void RRAppFramework::reportInteraction()
{
	lastInteractionTime = GETTIME;
}

static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

void RRAppFramework::readResults()
{
	//!!! multiobjekt
	/*unsigned firstTriangleIdx[1000];//!!!
	unsigned firstVertexIdx[1000];
	unsigned triIdx = 0;
	unsigned vertIdx = 0;
	for(unsigned obj=0;obj<(unsigned)m3ds.numObjects;obj++)
	{
		firstTriangleIdx[obj] = triIdx;
		firstVertexIdx[obj] = vertIdx;
		for (int j = 0; j < m3ds.Objects[obj].numMatFaces; j ++)
		{
			unsigned numTriangles = m3ds.Objects[obj].MatFaces[j].numSubFaces/3;
			triIdx += numTriangles;
		}
		vertIdx += m3ds.Objects[obj].numVerts;
	}*/

	unsigned objectHandle=0;//!!!
	for(Objects::iterator i=objects.begin();i!=objects.end();i++)
	{
		RRObjectIllumination* objectIllumination = *i;
		RRObjectImporter* object = objectIllumination->getObjectImporter();
		rrCollider::RRMeshImporter* mesh = object->getCollider()->getImporter();
		unsigned numVertices = mesh->getNumVertices();
		unsigned numTriangles = mesh->getNumTriangles();
		RRObjectIllumination::Channel* channel = objectIllumination->getChannel(resultChannelIndex);
		RRObjectIllumination::VertexBuffer* vertexBuffer = &channel->vertexBuffer;

		assert(vertexBuffer->vertices);
		assert(vertexBuffer->format==RRObjectIllumination::RGB32F);

		//vertexBuffer->setToZero();

		for(unsigned t=0;t<numTriangles;t++) // triangle
		{
			rrCollider::RRMeshImporter::Triangle triangle;
			mesh->getTriangle(t,triangle);
			for(unsigned v=0;v<3;v++) // vertex
			{
				// get 1*irradiance
				rrVision::RRColor indirect;
				scene->getTriangleMeasure(objectHandle,t,v,rrVision::RM_IRRADIANCE,indirect);
				// write 1*irradiance
				for(unsigned i=0;i<3;i++)
				{
					assert(_finite(indirect[i]));
					assert(indirect[i]>=0);
					assert(indirect[i]<1500000);
				}
				unsigned preVertexIdx = mesh->getPreImportVertex(triangle[v],t);
				assert(preVertexIdx<vertexBuffer->numVertices);
				((RRColor*)vertexBuffer)[preVertexIdx] = indirect;
				/*
				rrCollider::RRMeshImporter::MultiMeshPreImportNumber pre = mesh->getPreImportVertex(triangle[v],t);
				unsigned preVertexIdx = firstVertexIdx[pre.object]+pre.index;
				assert(preVertexIdx<numVertices);
				if(size2(indirect)>size2(indirectColors[preVertexIdx])) // use maximum, so degenerated black triangles are ignored
					indirectColors[preVertexIdx] = indirect;
					*/
			}
		}
		objectHandle++;
	}
}

void RRAppFramework::calculate()
{
	TIME now = GETTIME;
	if((now-lastInteractionTime)/(float)PER_SEC<PAUSE_AFTER_INTERACTION) return;

	bool dirtyFactors = false;
	bool dirtyEnergies = false;
	if(dirtyMaterials)
	{
		dirtyMaterials = false;
		dirtyFactors = true;
		reportAction("Detecting material properties.");
		detectMaterials();
	}
	if(dirtyGeometry)
	{
		dirtyGeometry = false;
		dirtyLights = true;
		dirtyFactors = true;
		reportAction("Closing old radiosity solver.");
		delete scene;
		reportAction("Opening new radiosity solver.");
		scene = new RRScene;
		for(Objects::iterator i=objects.begin();i!=objects.end();i++)
			scene->objectCreate((*i)->getObjectImporter());
		//!!! slepit multiobject
	}
	if(dirtyLights)
	{
		dirtyLights = false;
		dirtyEnergies = true;
		readingResultsPeriod = 0.1f;
		reportAction("Detecting direct illumination.");
		detectDirectIllumination();
	}
	if(dirtyFactors)
	{
		dirtyFactors = false;
		dirtyEnergies = false;
		reportAction("Resetting solver energies and factors.");
		scene->illuminationReset(true);
	}
	if(dirtyEnergies)
	{
		dirtyEnergies = false;
		reportAction("Resetting solver energies.");
		scene->illuminationReset(false);
	}

	reportAction("Calculating.");
	calcTimeSinceReadingResults += CALC_STEP;
	TIME end = (TIME)(now+CALC_STEP*PER_SEC);
	scene->illuminationImprove(endByTime,(void*)&end);

	if(calcTimeSinceReadingResults>=readingResultsPeriod)
	{
		reportAction("Reading results.");
		calcTimeSinceReadingResults = 0;
		if(readingResultsPeriod<1.5f) readingResultsPeriod*=1.1f;
		readResults();
	}
}

} // namespace
