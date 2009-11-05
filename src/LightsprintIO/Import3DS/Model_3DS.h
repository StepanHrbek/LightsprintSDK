//////////////////////////////////////////////////////////////////////
//
// 3D Studio Model Class
// by: Matthew Fairfax
// modified by: Stepan Hrbek
//
// Model_3DS.h: interface for the Model_3DS class.
// This is a simple class for loading and viewing
// 3D Studio model files (.3ds). It supports models
// with multiple objects. It also supports multiple
// textures per object. It does not support the animation
// for 3D Studio models.
//
// Some models have problems loading and I don't know why. If you
// can import the 3D Studio file into Milkshape 3D 
// (http://www.swissquake.ch/chumbalum-soft) and then export it
// to a new 3D Studio file. This seems to fix many of the problems
// but there is a limit on the number of faces and vertices Milkshape 3D
// can read.
//
// Usage:
// Model_3DS m;
//
// m.Load("model.3ds"); // Load the model
// m.Draw();			// Renders the model to the screen
// 
// You can move and rotate the model like this:
// m.rot.x = 90.0f;
// m.rot.y = 30.0f;
// m.rot.z = 0.0f;
//
// m.pos.x = 10.0f;
// m.pos.y = 0.0f;
// m.pos.z = 0.0f;
//
// // If you want to move or rotate individual objects
// m.Objects[0].rot.x = 90.0f;
// m.Objects[0].rot.y = 30.0f;
// m.Objects[0].rot.z = 0.0f;
//
// m.Objects[0].pos.x = 10.0f;
// m.Objects[0].pos.y = 0.0f;
// m.Objects[0].pos.z = 0.0f;
//
//////////////////////////////////////////////////////////////////////

#ifndef MODEL_3DS_H
#define MODEL_3DS_H

#include <cstdio>
#include <cstring>
#include "Lightsprint/RRMaterial.h"

class Model_3DS
{
public:
	// Material is read directly to Lightsprint's RRMaterial
	struct Material : public rr::RRMaterial
	{
		enum // uv channels
		{
			CH_DIFFUSE_SPECULAR_EMISSIVE_OPACITY,
			CH_LIGHTMAP,
		};
		Material()
		{
			reset(false); // Model_3DS::MaterialChunkProcessor expects 1-sided is already set
			lightmapTexcoord = CH_LIGHTMAP;
		}
		~Material()
		{
			delete diffuseReflectance.texture;
			delete diffuseEmittance.texture;
			delete specularReflectance.texture;
			delete specularTransmittance.texture;
		}
	};

	// Every chunk in the 3ds file starts with this struct
	struct ChunkHeader
	{
		unsigned short id;	// The chunk's id
		unsigned int   len;	// The lenght of the chunk
	};

	// I sort the mesh by material so that I won't have to switch textures a great deal
	struct MaterialFaces
	{
		unsigned short *subFaces;	// Index to our vertex array of all the faces that use this material
		int numSubFaces;			// The number of faces
		int MatIndex;				// An index to our materials
		MaterialFaces()
		{
			memset(this,0,sizeof(*this));
		}
		~MaterialFaces()
		{
			delete[] subFaces;
		}
	};

	// The 3ds file can be made up of several objects
	struct Object
	{
		char name[80];				// The object name
		rr::RRVec3* Vertexes;		// The array of vertices
		rr::RRVec3* Normals;		// The array of the normals for the vertices
		rr::RRVec2* TexCoords;		// The array of texture coordinates for the vertices
		unsigned short *Faces;		// The array of face indices
		int numFaces;				// The number of faces
		int numMatFaces;			// The number of differnet material faces
		int numVerts;				// The number of vertices
		int numTexCoords;			// The number of vertices
		MaterialFaces *MatFaces;	// The faces are divided by materials
		rr::RRVec3 pos;				// The position to move the object to
		rr::RRVec3 rot;				// The angles to rotate the object
		Object()
		{
			memset(this,0,sizeof(*this));
		}
		~Object()
		{
			delete[] Vertexes;
			delete[] Normals;
			delete[] TexCoords;
			delete[] Faces;
			delete[] MatFaces;
		}
	};

	char *modelname;		// The name of the model
	char *path;				// The path of the model
	int numObjects;			// Total number of objects in the model
	int numMaterials;		// Total number of materials in the model
	int totalVerts;			// Total number of vertices in the model
	int totalFaces;			// Total number of faces in the model
	Material *Materials;	// The array of materials
	Object *Objects;		// The array of objects in the model
	rr::RRVec3 pos;			// The position to move the model to
	rr::RRVec3 rot;			// The angles to rotate the model
	float scale;			// Multiplier used in adapter, 10 makes model 10x bigger
	bool smoothAll;         // True: average normals of all vertices on the same position
	rr::RRVec3 localCenter;
	float localMinY;
	bool Load(const char *name, float scale); // Loads a model
	void Draw(
		void* model,
		bool lit, // scene is lit, feed normals
		bool texturedDiffuse,
		bool texturedEmissive,
		const float* (*acquireVertexColors)(void* model,unsigned object), // returns pointer to array of float rgb vertex colors
		void (*releaseVertexColors)(void* model,unsigned object)
		) const;  // Draws the model using provided indirect illum
	FILE *bin3ds;			// The binary 3ds file
	Model_3DS();			// Constructor
	~Model_3DS();           // Destructor

private:
	void UpdateCenter();
	void IntColorChunkProcessor(long length, long findex, rr::RRVec3& color);
	void FloatColorChunkProcessor(long length, long findex, rr::RRVec3& color);
	unsigned ReadPercentage(); // PercentageChunkProcessor
	// Processes the Main Chunk that all the other chunks exist is
	void MainChunkProcessor(long length, long findex);
		// Processes the model's info
		void EditChunkProcessor(long length, long findex);
			
			void UnitChunkProcessor(long length, long findex);

			// Processes the model's materials
			void MaterialChunkProcessor(long length, long findex, int matindex);
				// Processes the names of the materials
				void MaterialNameChunkProcessor(long length, long findex, int matindex);
				// Processes the material's diffuse color
				void ColorChunkProcessor(long length, long findex, rr::RRVec3& color);
				// Processes the material's texture maps
				char* TextureMapChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty);
					// Processes the names of the textures and load the textures
					char* MapNameChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty);
			
			// Processes the model's geometry
			void ObjectChunkProcessor(long length, long findex, int& objindex); // increases objindex if mesh is loaded. ignores lights, cameras
				// Processes the triangles of the model
				void TriangularMeshChunkProcessor(long length, long findex, int objindex);
					// Processes the vertices of the model and loads them
					void VertexListChunkProcessor(long length, long findex, int objindex);
					// Processes the texture cordiantes of the vertices and loads them
					void TexCoordsChunkProcessor(long length, long findex, int objindex);
					// Processes the faces of the model and loads the faces
					void FacesDescriptionChunkProcessor(long length, long findex, int objindex);
						// Processes the materials of the faces and splits them up by material
						void FacesMaterialsListChunkProcessor(long length, long findex, int objindex, int subfacesindex);
};

#endif // MODEL_3DS_H
