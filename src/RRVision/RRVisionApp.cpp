#define MULTIOBJECT // creates multiObject to accelerate calculation

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

RRVisionApp::RRVisionApp()
{
	multiObject = NULL;
	surfaces = NULL;
	numSurfaces = 0;
	scene = NULL;
	resultChannelIndex = 0;
	dirtyMaterials = true;
	dirtyGeometry = true;
	dirtyLights = true;
	calcTimeSinceReadingResults = 0;
}

RRVisionApp::~RRVisionApp()
{
	delete scene;
	delete surfaces;
}

void RRVisionApp::setObjects(Objects& aobjects)
{
	objects = aobjects;
	dirtyGeometry = true;
}

RRObjectImporter* RRVisionApp::getObject(unsigned i)
{
	if(i>=objects.size()) return NULL;
	return objects.at(i).first;
}

RRObjectIllumination* RRVisionApp::getIllumination(unsigned i)
{
	if(i>=objects.size()) return NULL;
	return objects.at(i).second;
}

void RRVisionApp::setResultChannel(unsigned channelIndex)
{
	resultChannelIndex = channelIndex;
}

void RRVisionApp::adjustScene()
{
	scene->setScaler(RRScaler::createRgbScaler());
}

void RRVisionApp::reportAction(const char* action) const
{
}

void RRVisionApp::reportMaterialChange()
{
	dirtyMaterials = true;
}

void RRVisionApp::reportGeometryChange()
{
	dirtyGeometry = true;
}

void RRVisionApp::reportLightChange()
{
	dirtyLights = true;
}

void RRVisionApp::reportInteraction()
{
	lastInteractionTime = GETTIME;
}

static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

void RRVisionApp::updateLookupTable()
// prepare lookup tables preImportVertex -> [postImportTriangle,vertex0..2] for all objects
{
	preVertex2PostTriangleVertex.resize(objects.size());
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		RRObjectIllumination* illumination = getIllumination(objectHandle);
		rrCollider::RRMeshImporter* mesh =
#ifdef MULTIOBJECT
			multiObject->getCollider()->getImporter();
#else
			object->getCollider()->getImporter();
#endif
		unsigned numPostImportVertices = mesh->getNumVertices();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		unsigned numPreImportVertices = illumination->getNumPreImportVertices();

		preVertex2PostTriangleVertex[objectHandle].resize(numPreImportVertices,std::pair<unsigned,unsigned>(rrCollider::RRMeshImporter::UNDEFINED,rrCollider::RRMeshImporter::UNDEFINED));

		for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
		{
			rrCollider::RRMeshImporter::Triangle postImportTriangleVertices;
			mesh->getTriangle(postImportTriangle,postImportTriangleVertices);
			for(unsigned v=0;v<3;v++)
			{
				unsigned postImportVertex = postImportTriangleVertices[v];
				if(postImportVertex<numPostImportVertices)
				{
					unsigned preVertex = mesh->getPreImportVertex(postImportVertex,postImportTriangle);
#ifdef MULTIOBJECT
					rrCollider::RRMeshImporter::MultiMeshPreImportNumber preVertexMulti = preVertex;
					if(preVertexMulti.object==objectHandle)
						preVertex = preVertexMulti.index;
					else
						continue; // skip asserts
#endif
					if(preVertex<numPreImportVertices)
						preVertex2PostTriangleVertex[objectHandle][preVertex] = std::pair<unsigned,unsigned>(postImportTriangle,v);
					else
						assert(0);
				}
				else
					assert(0);
			}
		}
	}
}

void RRVisionApp::readResults()
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
	for(Objects::iterator obji=objects.begin();obji!=objects.end();obji++)
	{
		RRObjectImporter* object = (*obji).first;
		RRObjectIllumination* illumination = (*obji).second;
		rrCollider::RRMeshImporter* mesh =
#ifdef MULTIOBJECT
			multiObject->getCollider()->getImporter();
#else
			object->getCollider()->getImporter();
#endif
		unsigned numPostImportVertices = mesh->getNumVertices();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		unsigned numPreImportVertices = illumination->getNumPreImportVertices();
		RRObjectIllumination::Channel* channel = illumination->getChannel(resultChannelIndex);
		RRObjectIllumination::VertexBuffer* vertexBuffer = &channel->vertexBuffer;

		assert(vertexBuffer->vertices);
		assert(vertexBuffer->numPreImportVertices==numPreImportVertices);
		assert(vertexBuffer->format==RRObjectIllumination::RGB32F);

		//vertexBuffer->setToZero();
/*
		// prepare lookup table preImportVertex -> [postImportTriangle,vertex0..2]
		std::pair<unsigned,unsigned>* preVertex2PostTriangleVertex = new std::pair<unsigned,unsigned>[numPreImportVertices];
		for(unsigned preImportVertex=0;preImportVertex<numPreImportVertices;preImportVertex++)
			preVertex2PostTriangleVertex[preImportVertex] = std::pair<unsigned,unsigned>(rrCollider::RRMeshImporter::UNDEFINED,rrCollider::RRMeshImporter::UNDEFINED);
		for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
		{
			rrCollider::RRMeshImporter::Triangle postImportTriangleVertices;
			mesh->getTriangle(postImportTriangle,postImportTriangleVertices);
			for(unsigned v=0;v<3;v++)
			{
				unsigned postImportVertex = postImportTriangleVertices[v];
				if(postImportVertex<numPostImportVertices)
				{
					unsigned preVertex = mesh->getPreImportVertex(postImportVertex,postImportTriangle);
#ifdef MULTIOBJECT
					rrCollider::RRMeshImporter::MultiMeshPreImportNumber preVertexMulti = preVertex;
					if(preVertexMulti.object==objectHandle)
						preVertex = preVertexMulti.index;
					else
						continue; // skip asserts
#endif
					if(preVertex<numPreImportVertices)
						preVertex2PostTriangleVertex[preVertex] = std::pair<unsigned,unsigned>(postImportTriangle,v);
					else
						assert(0);
				}
				else
					assert(0);
			}
		}*/
		// load measure into each preImportVertex
		for(unsigned preImportVertex=0;preImportVertex<numPreImportVertices;preImportVertex++)
		{
			unsigned t = preVertex2PostTriangleVertex[objectHandle][preImportVertex].first;
			unsigned v = preVertex2PostTriangleVertex[objectHandle][preImportVertex].second;
			RRColor indirect = RRColor(0);
#ifdef MULTIOBJECT
			scene->getTriangleMeasure(0,t,v,RM_IRRADIANCE,indirect);
#else
			scene->getTriangleMeasure(objectHandle,t,v,RM_IRRADIANCE,indirect);
#endif
			for(unsigned i=0;i<3;i++)
			{
				assert(_finite(indirect[i]));
				assert(indirect[i]>=0);
				assert(indirect[i]<1500000);
			}
			((RRColor*)vertexBuffer->vertices)[preImportVertex] = indirect;
		}
		// delete lookup table
		//delete[] preVertex2PostTriangleVertex;

/*
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
				assert(preVertexIdx<vertexBuffer->numPreImportVertices);
				((RRColor*)vertexBuffer)[preVertexIdx] = indirect;
				/*
				rrCollider::RRMeshImporter::MultiMeshPreImportNumber pre = mesh->getPreImportVertex(triangle[v],t);
				unsigned preVertexIdx = firstVertexIdx[pre.object]+pre.index;
				assert(preVertexIdx<numPostImportVertices);
				if(size2(indirect)>size2(indirectColors[preVertexIdx])) // use maximum, so degenerated black triangles are ignored
					indirectColors[preVertexIdx] = indirect;
					* /
			}
		}*/
		objectHandle++;
	}
}

RRScene::Improvement RRVisionApp::calculate()
{
	TIME now = GETTIME;
	if((now-lastInteractionTime)/(float)PER_SEC<PAUSE_AFTER_INTERACTION) return RRScene::NOT_IMPROVED;

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
		if(scene)
		{
			reportAction("Closing old radiosity solver.");
			delete scene;
		}
		reportAction("Opening new radiosity solver.");
		scene = new RRScene;
#ifdef MULTIOBJECT
		rrVision::RRObjectImporter** importers = new rrVision::RRObjectImporter*[objects.size()];
		for(unsigned i=0;i<(unsigned)objects.size();i++)
		{
			importers[i] = objects.at(i).first;
		}
		RRObjectImporter* object = RRObjectImporter::createMultiObject(importers,(unsigned)objects.size(),rrCollider::RRCollider::IT_BSP_FASTEST,0.01f,false,NULL);
		multiObject = object ? object->createAdditionalIllumination() : NULL;
		//!!! delete[] importers;
		scene->objectCreate(multiObject);
#else
		for(Objects::iterator i=objects.begin();i!=objects.end();i++)
			scene->objectCreate((*i).first);
#endif
		updateLookupTable();
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
	RRScene::Improvement improvement = scene->illuminationImprove(endByTime,(void*)&end);
	if(improvement==RRScene::FINISHED || improvement==RRScene::INTERNAL_ERROR)
		return improvement;

	if(calcTimeSinceReadingResults>=readingResultsPeriod)
	{
		reportAction("Reading results.");
		calcTimeSinceReadingResults = 0;
		if(readingResultsPeriod<1.5f) readingResultsPeriod*=1.1f;
		readResults();
		return RRScene::IMPROVED;
	}
	return RRScene::NOT_IMPROVED;
}

} // namespace
