#ifndef DYNAMICMESHDEMO_H
#define DYNAMICMESHDEMO_H

#include "Lightsprint/RRScene.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfScene.h"

namespace rr_gl
{

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
	unsigned numIndices; // numIndices = 3*numTriangles
	unsigned short* indices; // triangle-list, 3 indices per triangle
	rr::RRMaterial* material;
};

//! Scene with static objects, dynamic lights, dynamic skybox and realtime GI render.
//! It's legal to have multiple scenes and call their functions in any order as long as you do it serially from main thread.
//! Demo must already exist when creating Scene.
class RR_GL_API Scene : public RRDynamicSolverGL
{
public:
	//! Loads scene from disk (Collada .dae, Gamebryo .gsa, Quake3 .bsp, .3ds, .mgf, .obj).
	//! Quality 5000 is usually ok, 1000 is bit worse but much faster.
	//! Quality is baked into .fireball file created in temp,
	//! delete .fireball if you want to change quality later.
	Scene(const char* sceneFilename, unsigned quality, float indirectIllumMultiplier);
	~Scene();

	//! Sets skybox surrounding whole scene. May be called each frame if you modify skybox dynamically.
	void setEnvironment(rr::RRBuffer* skybox);

	//! Renders scene. When called for first time, acceleration structures are created (slow).
	void render(Camera& camera, unsigned numDynamicMeshes, DynamicMesh* dynamicMeshes);

	//! Runs interactive scene viewer/debugger.
	void debugger();

protected:
	//! Implements RRDynamicSolverGL interface.
	virtual void renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight);

private:
	rr::RRScene* scene;
	RendererOfScene* rendererOfScene;
	rr::RRVec4 brightness;
	unsigned numDynamicMeshes;
	DynamicMesh* dynamicMeshes;
};

//! Initializes/shuts down global stuff, create one instance when your application starts.
//! Needs GL context.
class RR_GL_API Demo
{
public:
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	Demo(const char* pathToShaders);
	~Demo();
};

}; // namespace

#endif // RRDEMO_H
