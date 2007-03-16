// --------------------------------------------------------------------------
// Imports Collada model into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------
#if 0

#ifndef RROBJECTCOLLADA_H
#define RROBJECTCOLLADA_H

#include <map>
#include "RRRealtimeRadiosity.h"


//////////////////////////////////////////////////////////////////////////////
//
// ColladaToRealtimeRadiosity

class ColladaToRealtimeRadiosity
{
public:

	//! Imports FCollada scene contents to RRRealtimeRadiosity solver.
	ColladaToRealtimeRadiosity(class FCDocument* document,const char* pathToTextures,rr::RRRealtimeRadiosity* solver,const rr::RRScene::SmoothingParameters* smoothing);

	//! Removes FCollada scene contents from RRRealtimeRadiosity solver.
	~ColladaToRealtimeRadiosity();

private:
	rr::RRCollider*                  newColliderCached(const class FCDGeometryMesh* mesh);
	class RRObjectCollada*           newObject(const class FCDSceneNode* node, const class FCDGeometryInstance* geometryInstance, const char* pathToTextures);
	void                             addNode(const class FCDSceneNode* node, const char* pathToTextures);
	const class FCDocument*          document;
	rr::RRRealtimeRadiosity*         solver;
	typedef std::map<const class FCDGeometryMesh*,rr::RRCollider*> Cache;
	Cache                            cache;
	const char*                      pathToTextures;
	rr::RRRealtimeRadiosity::Objects objects;
};

#endif
#endif
