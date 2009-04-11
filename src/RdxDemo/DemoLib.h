#ifndef DEMOLIB_H
#define DEMOLIB_H

#include "Lightsprint/RRScene.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfScene.h"

//! Dynamically generated vertex.
struct DynamicVertex
{
	float position[3];
	float normal[3];
};

//! Dynamically generated mesh.
struct DynamicMesh
{
	unsigned numVertices;
	DynamicVertex* vertices;
	unsigned numIndices;
	unsigned short* indices;
	rr::RRMaterial* material;
};

//! Scene with static objects, dynamic lights, dynamic skybox and realtime GI render.
//! It's legal to have multiple scenes and call their functions in any order as long as you do it serially from main thread.
//! Demo must already exist when creating Scene.
class Scene : public rr_gl::RRDynamicSolverGL
{
public:
	//! Loads scene from disk (Collada .dae, Gamebryo .gsa, Quake3 .bsp, .3ds, .mgf, .obj).
	//! Quality 5000 is usually ok, 1000 is only bit worse but much faster.
	//! Quality is baked into .fireball file created in temp,
	//! delete .fireball if you change quality later.
	Scene(const char* sceneFilename, unsigned quality, float indirectIllumMultiplier);
	~Scene();

	//! Sets skybox surrounding whole scene. Too expensive to be called each frame.
	void setEnvironment(rr::RRBuffer* skybox);

	//! Renders scene. When called for first time, acceleration structures are created (slow).
	void render(rr_gl::Camera& camera, unsigned numDynamicMeshes, DynamicMesh* dynamicMeshes);

	//! Runs interactive scene viewer/debugger.
	void debugger();

protected:
	//! Implements RRDynamicSolverGL interface.
	virtual void renderScene(rr_gl::UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight);

private:
	rr::RRScene* scene;
	rr_gl::RendererOfScene* rendererOfScene;
	rr::RRVec4 brightness;
	unsigned numDynamicMeshes;
	DynamicMesh* dynamicMeshes;
};

//! Initializes/shuts down global stuff, create one instance when your application starts.
//! Needs GL context.
class Demo
{
public:
	Demo(const char* pathToShaders);
	~Demo();
};

#endif // RRDEMO_H
