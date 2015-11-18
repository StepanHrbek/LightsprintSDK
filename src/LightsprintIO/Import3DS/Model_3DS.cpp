//////////////////////////////////////////////////////////////////////
//
// 3D Studio Model Class
// by: Matthew Fairfax
// modified by: Stepan Hrbek
//
// http://www.jalix.org/ressources/graphics/3DS/_unofficials/3ds-info.txt
//
// Model_3DS.cpp: implementation of the Model_3DS class.
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

#include <cmath>
#include <cstring>
#include "Model_3DS.h"
#ifndef RR_IO_BUILD
#include "Lightsprint/GL/UberProgramSetup.h"
#endif
#include "Lightsprint/RRDebug.h"

//////////////////////////////////////////////////////////////////////
// utility functions for handling little endian on a big endian system
//////////////////////////////////////////////////////////////////////

inline void swap16(void* p)
{
#ifdef RR_BIG_ENDIAN
	char b0 = ((char*) p)[0];
	char b1 = ((char*) p)[1];
	((char*) p)[0] = b1;
	((char*) p)[1] = b0;
#endif
}

inline void swap32(void* p)
{
#ifdef RR_BIG_ENDIAN
	char b0 = ((char*) p)[0];
	char b1 = ((char*) p)[1];
	char b2 = ((char*) p)[2];
	char b3 = ((char*) p)[3];
	((char*) p)[0] = b3;
	((char*) p)[1] = b2;
	((char*) p)[2] = b1;
	((char*) p)[3] = b0;
#endif
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Model_3DS::Model_3DS()
{
	memset(this,0,sizeof(*this));
}

Model_3DS::~Model_3DS()
{
	delete[] Materials;
	delete[] Objects;
}

bool Model_3DS::Load(const rr::RRString& _filename, const rr::RRFileLocator* _textureLocator, float _scale)
{
	rr::RRFileLocator* localLocator = _textureLocator ? NULL : rr::RRFileLocator::create();
	if (localLocator)
		localLocator->setParent(true,_filename);
	textureLocator = _textureLocator ? _textureLocator : localLocator;

	scale = _scale;

	// holds the main chunk header
	ChunkHeader main;

	// Load the file
#ifdef _WIN32
	bin3ds = _wfopen(_filename.w_str(),L"rb");
#else
	bin3ds = fopen(RR_RR2CHAR(_filename),"rb"); // 3ds outside windows does not support unicode filename
#endif

	if (!bin3ds)
	{
		textureLocator = NULL;
		RR_SAFE_DELETE(localLocator);
		printf("file not found: %ls\n",_filename.w_str());
		return false;
	}

	// Make sure we are at the beginning
	fseek(bin3ds, 0, SEEK_SET);

	// Load the Main Chunk's header
	fread(&main.id,sizeof(main.id),1,bin3ds);
	fread(&main.len,sizeof(main.len),1,bin3ds);
	swap16(&main.id);
	swap32(&main.len);

	// Start Processing
	MainChunkProcessor(main.len, ftell(bin3ds));

	// Don't need the file anymore so close it
	fclose(bin3ds);

	// Normalize the vertex normals
	for (int i = 0; i < numObjects; i++)
	{
		for (int g = 0; g < Objects[i].numVerts; g++)
		{
			// normalizeSafe is necessary, because vertex shared by 2 triangles with opposite planes has sum of normals 0
			Objects[i].Normals[g].normalizeSafe();
		}
	}

	// Find the total number of faces and vertices
	totalFaces = 0;
	totalVerts = 0;

	for (int i = 0; i < numObjects; i ++)
	{
		totalFaces += Objects[i].numFaces/3;
		totalVerts += Objects[i].numVerts;
	}

	// If the object doesn't have any texcoords generate some
	for (int k = 0; k < numObjects; k++)
	{
		if (Objects[k].numTexCoords == 0)
		{
			// Set the number of texture coords
			Objects[k].numTexCoords = Objects[k].numVerts;

			// Allocate an array to hold the texture coordinates
			Objects[k].TexCoords = new rr::RRVec2[Objects[k].numTexCoords];

			// Make some texture coords
			for (int m = 0; m < Objects[k].numTexCoords; m++)
			{
				Objects[k].TexCoords[m] = Objects[k].Vertexes[m];
			}
		}
	}

	UpdateCenter();

	// check that transformations are identity
	bool identity = true;
	/*if (pos.x||pos.y||pos.z||rot.x||rot.y||rot.z||scale!=1)
	{
		identity=false;
		RR_ASSERT(0);
	}*/
	for (int i = 0; i < numObjects; i++)
	{
		if (Objects[i].pos.x||Objects[i].pos.y||Objects[i].pos.z||Objects[i].rot.x||Objects[i].rot.y||Objects[i].rot.z)
		{
			identity=false;
			RR_ASSERT(0);
		}
	}
	if (!identity)
		rr::RRReporter::report(rr::WARN,"Object transformations in .3ds are not supported.\n");

	textureLocator = NULL;
	RR_SAFE_DELETE(localLocator);
	return true;
}

void Model_3DS::UpdateCenter()
{
	localCenter = rr::RRVec3(0);
	localMinY = 1e10;
	for (int o=0;o<numObjects;o++)
	{
		for (int v=0;v<Objects[o].numVerts;v++)
		{
			localCenter += Objects[o].Vertexes[v];
			localMinY = RR_MIN(localMinY,Objects[o].Vertexes[v].y);
		}
	}
	localCenter /= (float)totalVerts;
}

void Model_3DS::Draw(
	void* model,
	bool lit,
	bool texturedDiffuse,
	bool texturedEmissive,
	const float* (*acquireVertexColors)(void* model,unsigned object),
	void (*releaseVertexColors)(void* model,unsigned object)) const
{

#ifndef RR_IO_BUILD

	glShadeModel(GL_SMOOTH);
	/*glPushMatrix();

	// Move the model
	glTranslatef(pos.x, pos.y, pos.z);

	// Rotate the model
	glRotatef(rot.x, 1.0f, 0.0f, 0.0f);
	glRotatef(rot.y, 0.0f, 1.0f, 0.0f);
	glRotatef(rot.z, 0.0f, 0.0f, 1.0f);

	//glScalef(scale, scale, scale);*/

	// Loop through the objects
	for (int i = 0; i < numObjects; i++)
	{
		// additional exitance
		if (acquireVertexColors)
		{
			const float* vertexColors = acquireVertexColors(model,i);
			if (vertexColors)
			{
				glEnableVertexAttribArray(rr_gl::VAA_COLOR);
				glVertexAttribPointer(rr_gl::VAA_COLOR, 3, GL_FLOAT, GL_FALSE, 0, vertexColors);
				if (releaseVertexColors) releaseVertexColors(model,i);
			}
		}
		else
		{
			glColor3f(0,0,0);
		}

		// Enable texture coordiantes, normals, and vertices arrays
		// Point them to the objects arrays
		if (texturedDiffuse)
		{
			glEnableVertexAttribArray(rr_gl::VAA_UV_MATERIAL_DIFFUSE);
			glVertexAttribPointer(rr_gl::VAA_UV_MATERIAL_DIFFUSE, 2, GL_FLOAT, GL_FALSE, 0, Objects[i].TexCoords);
		}
		if (texturedEmissive)
		{
			glEnableVertexAttribArray(rr_gl::VAA_UV_MATERIAL_EMISSIVE);
			glVertexAttribPointer(rr_gl::VAA_UV_MATERIAL_EMISSIVE, 2, GL_FLOAT, GL_FALSE, 0, Objects[i].TexCoords);
		}
		if (lit)
		{
			glEnableVertexAttribArray(rr_gl::VAA_NORMAL);
			glVertexAttribPointer(rr_gl::VAA_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, Objects[i].Normals);
		}
		glEnableVertexAttribArray(rr_gl::VAA_POSITION);
		glVertexAttribPointer(rr_gl::VAA_POSITION, 3, GL_FLOAT, GL_FALSE, 0, Objects[i].Vertexes);

		// Loop through the faces as sorted by material and draw them
		for (int j = 0; j < Objects[i].numMatFaces; j ++)
		{
			// Use the material's texture
			if (texturedDiffuse || texturedEmissive)
				if (Materials[Objects[i].MatFaces[j].MatIndex].diffuseReflectance.texture)
					rr_gl::getTexture(Materials[Objects[i].MatFaces[j].MatIndex].diffuseReflectance.texture)->bindTexture();

			/*glpushmatrix();

				// move the model
				gltranslatef(Objects[i].pos.x, Objects[i].pos.y, Objects[i].pos.z);

				// Rotate the model
				//glRotatef(Objects[i].rot.x, 1.0f, 0.0f, 0.0f);
				//glRotatef(Objects[i].rot.y, 0.0f, 1.0f, 0.0f);
				//glRotatef(Objects[i].rot.z, 0.0f, 0.0f, 1.0f);

				glRotatef(Objects[i].rot.z, 0.0f, 0.0f, 1.0f);
				glRotatef(Objects[i].rot.y, 0.0f, 1.0f, 0.0f);
				glRotatef(Objects[i].rot.x, 1.0f, 0.0f, 0.0f);*/

				// Draw the faces using an index to the vertex array
				glDrawElements(GL_TRIANGLES, Objects[i].MatFaces[j].numSubFaces, GL_UNSIGNED_SHORT, Objects[i].MatFaces[j].subFaces);

			//glPopMatrix();
		}
	}

	//glPopMatrix();

	glDisableVertexAttribArray(rr_gl::VAA_UV_MATERIAL_DIFFUSE);
	glDisableVertexAttribArray(rr_gl::VAA_UV_MATERIAL_EMISSIVE);
	glDisableVertexAttribArray(rr_gl::VAA_NORMAL);
	glDisableVertexAttribArray(rr_gl::VAA_POSITION);
	glDisableVertexAttribArray(rr_gl::VAA_COLOR);

#endif // RR_IO_BUILD
}

void Model_3DS::MainChunkProcessor(long length, long findex)
{
	ChunkHeader h;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			// This is the mesh information like vertices, faces, and materials
			case 0x3D3D:
				EditChunkProcessor(h.len, ftell(bin3ds));
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::EditChunkProcessor(long length, long findex)
{
	ChunkHeader h;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	int numObjectsLightsCameras = 0; // meshes+lights+cameras
	numObjects = 0;

	// First count the number of meshes+lights+cameras and Materials
	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0x4000:
				numObjectsLightsCameras++;
				break;
			case 0xAFFF:
				numMaterials++;
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// Now load the materials and units
	if (numMaterials > 0)
	{
		Materials = new Material[numMaterials];

		fseek(bin3ds, findex, SEEK_SET);

		int i = 0;

		while (ftell(bin3ds) < (findex + length - 6))
		{
			fread(&h.id,sizeof(h.id),1,bin3ds);
			fread(&h.len,sizeof(h.len),1,bin3ds);
			swap16(&h.id);
			swap32(&h.len);

			switch (h.id)
			{
				case 0xAFFF:
					MaterialChunkProcessor(h.len, ftell(bin3ds), i);
					i++;
					break;
				case 0x0100:
					UnitChunkProcessor(h.len, ftell(bin3ds));
					break;
			}

			fseek(bin3ds, (h.len - 6), SEEK_CUR);
		}
	}

	// Load the Objects (individual meshes in the whole model)
	if (numObjectsLightsCameras > 0)
	{
		Objects = new Object[numObjectsLightsCameras]; // possibly wastes small amount of memory, we don't need space for lights+cameras

		fseek(bin3ds, findex, SEEK_SET);

		int j = 0;

		while (ftell(bin3ds) < (findex + length - 6))
		{
			fread(&h.id,sizeof(h.id),1,bin3ds);
			fread(&h.len,sizeof(h.len),1,bin3ds);
			swap16(&h.id);
			swap32(&h.len);

			switch (h.id)
			{
				case 0x4000:
					ObjectChunkProcessor(h.len, ftell(bin3ds), numObjects);
					j++;
					break;
			}

			fseek(bin3ds, (h.len - 6), SEEK_CUR);
		}
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::UnitChunkProcessor(long length, long findex)
{
	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	float unit;
	fread(&unit,sizeof(unit),1,bin3ds);
	swap32(&unit);
	// there's no documentation on what unit means
	// 'unit' values in existing scenes are quite random, without visible relation to actual units
	//scale /= unit;

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::MaterialChunkProcessor(long length, long findex, int matindex)
{
	ChunkHeader h;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	char* diffuseName = NULL;
	char* opacityName = NULL;

	rr::RRVec3 specularColor;
	bool specularColorSet = false;
	float shininessStrength;
	bool shininessStrengthSet = false;

	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0xA081:
				// change default 1-sided to 2-sided
				//  depends on setting 1-sided in Material constructor
				//  we must not call reset() here, it would destroy lightmapTexcoord
				Materials[matindex].sideBits[1].renderFrom = 1;
				Materials[matindex].sideBits[1].receiveFrom = 1;
				Materials[matindex].sideBits[1].reflect = 1;
				break;
			case 0xA000:
				// Material name
				MaterialNameChunkProcessor(h.len, ftell(bin3ds), matindex);
				break;
			case 0xA020:
				// Diffuse color
				ColorChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].diffuseReflectance.color);
				break;
			case 0xA030:
				// Specular color
				ColorChunkProcessor(h.len, ftell(bin3ds), specularColor);
				specularColorSet = true;
				break;
			case 0xa050:
				// Transparency percent
				Materials[matindex].specularTransmittance.color = rr::RRVec3(ReadPercentage()*0.01f);
				break;
			case 0xa041:
				// Shininess strength percent
				shininessStrength = ReadPercentage()*0.01f;
				shininessStrengthSet = true;
				break;

			case 0xA200:
				// Texture map 1
				diffuseName = TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].diffuseReflectance);
				break;
			case 0xA210:
				// Opacity map
				opacityName = TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].specularTransmittance);
				break;
			case 0xA204:
				// Specular map
				free(TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].specularReflectance));
				break;
			case 0xA33D:
				// Self illum map
				free(TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].diffuseEmittance));
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);

	// due to lack of 3ds documentation, we only guess what to do
	if (specularColorSet && !shininessStrengthSet)
		Materials[matindex].specularReflectance.color = specularColor;
	if (!specularColorSet && shininessStrengthSet)
		Materials[matindex].specularReflectance.color = rr::RRVec3(shininessStrength);
	if (specularColorSet && shininessStrengthSet)
		Materials[matindex].specularReflectance.color = specularColor*shininessStrength;

	// due to lack of 3ds documentation, we interpret opacity map as follows
	// 1. if it is the same map as diffuse map, alpha=1 is opaque
	// 2. if it is different map, rgb=1 is opaque
	Materials[matindex].specularTransmittanceInAlpha = diffuseName && opacityName && strcmp(diffuseName,opacityName)==0;
	if (Materials[matindex].specularTransmittance.texture && !Materials[matindex].specularTransmittanceInAlpha)
	{
		Materials[matindex].specularTransmittanceMapInverted = true;
	}
	free(diffuseName);
	free(opacityName);

	// get average colors from textures
	rr::RRColorSpace* colorSpace = rr::RRColorSpace::create_sRGB();
	Materials[matindex].updateColorsFromTextures(colorSpace,rr::RRMaterial::UTA_DELETE,true);
	delete colorSpace;

	// autodetect keying
	Materials[matindex].updateKeyingFromTransmittance();

	// optimize material flags
	Materials[matindex].updateSideBitsFromColors();
}

// returns value from percentage chunk
unsigned Model_3DS::ReadPercentage()
{
	unsigned char arr[8];
	fread(arr,8,1,bin3ds);
	fseek(bin3ds, -8, SEEK_CUR);
	RR_ASSERT(arr[0]==0x30 && arr[1]==0 && arr[2]==8 && arr[3]==0 && arr[4]==0 && arr[5]==0 && arr[7]==0);
	return arr[6];
}

void Model_3DS::MaterialNameChunkProcessor(long length, long findex, int matindex)
{
	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	// Read the material's name
	char buf[580];
	for (int i = 0; i < 580; i++)
	{
		buf[i] = fgetc(bin3ds);
		if (buf[i] == 0)
			break;
	}
	Materials[matindex].name = buf;

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::ColorChunkProcessor(long length, long findex, rr::RRVec3& color)
{
	ChunkHeader h;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		// Determine the format of the color and load it
		switch (h.id)
		{
			case 0x0010:
				// A rgb float color chunk
				FloatColorChunkProcessor(h.len, ftell(bin3ds), color);
				break;
			case 0x0011:
				// A rgb int color chunk
				IntColorChunkProcessor(h.len, ftell(bin3ds), color);
				break;
			case 0x0013:
				// A rgb gamma corrected float color chunk
				FloatColorChunkProcessor(h.len, ftell(bin3ds), color);
				break;
			case 0x0012:
				// A rgb gamma corrected int color chunk
				IntColorChunkProcessor(h.len, ftell(bin3ds), color);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::FloatColorChunkProcessor(long length, long findex, rr::RRVec3& color)
{
	float r;
	float g;
	float b;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	fread(&r,sizeof(r),1,bin3ds);
	fread(&g,sizeof(g),1,bin3ds);
	fread(&b,sizeof(b),1,bin3ds);
	swap32(&r);
	swap32(&g);
	swap32(&b);

	color[0] = r;
	color[1] = g;
	color[2] = b;

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::IntColorChunkProcessor(long length, long findex, rr::RRVec3& color)
{
	unsigned char r;
	unsigned char g;
	unsigned char b;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	fread(&r,sizeof(r),1,bin3ds);
	fread(&g,sizeof(g),1,bin3ds);
	fread(&b,sizeof(b),1,bin3ds);

	color[0] = r/255.0f;
	color[1] = g/255.0f;
	color[2] = b/255.0f;

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

char* Model_3DS::TextureMapChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty)
{
	char* name = NULL;

	ChunkHeader h;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0xA300:
				// Read the name of texture in the Diffuse Color map
				name = MapNameChunkProcessor(h.len, ftell(bin3ds), materialProperty);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);

	return name;
}

char* Model_3DS::MapNameChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty)
{
	char name[580];

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	// Read the name of the texture
	for (int i = 0; i < 580; i++)
	{
		name[i] = fgetc(bin3ds);
		if (name[i] == 0)
			break;
	}

	// Load the texture
	materialProperty.texture = rr::RRBuffer::load(name,NULL,textureLocator);
	materialProperty.texcoord = 0;
	if (materialProperty.texture)
	{
		rr::RRColorSpace* colorSpace = rr::RRColorSpace::create_sRGB();
		materialProperty.updateColorFromTexture(colorSpace,false,rr::RRMaterial::UTA_DELETE,true);
		delete colorSpace;
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);

	return _strdup(name);
}

void Model_3DS::ObjectChunkProcessor(long length, long findex, int& objindex)
{
	ChunkHeader h;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	// Load the object's name
	for (int i = 0; i < 580; i++)
	{
		Objects[objindex].name[i] = fgetc(bin3ds);
		if (Objects[objindex].name[i] == 0)
			break;
	}

	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0x4100:
				// Process the triangles of the object
				TriangularMeshChunkProcessor(h.len, ftell(bin3ds), objindex++);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::TriangularMeshChunkProcessor(long length, long findex, int objindex)
{
	ChunkHeader h;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0x4110:
				// Load the vertices of the onject
				VertexListChunkProcessor(h.len, ftell(bin3ds), objindex);
				break;
			case 0x4160:
				//LocalCoordinatesChunkProcessor(h.len, ftell(bin3ds));
				break;
			case 0x4140:
				// Load the texture coordinates for the vertices
				TexCoordsChunkProcessor(h.len, ftell(bin3ds), objindex);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// After we have loaded the vertices we can load the faces
	fseek(bin3ds, findex, SEEK_SET);

	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0x4120:
				// Load the faces of the object
				FacesDescriptionChunkProcessor(h.len, ftell(bin3ds), objindex);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::VertexListChunkProcessor(long length, long findex, int objindex)
{
	unsigned short numVerts;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	// Read the number of vertices of the object
	fread(&numVerts,sizeof(numVerts),1,bin3ds);
	swap16(&numVerts);

	// Allocate arrays for the vertices and normals
	Objects[objindex].Vertexes = new rr::RRVec3[numVerts];
	Objects[objindex].Normals = new rr::RRVec3[numVerts];

	// Assign the number of vertices for future use
	Objects[objindex].numVerts = numVerts;

	// Zero out the normals array
	for (int j = 0; j < numVerts; j++)
		Objects[objindex].Normals[j] = rr::RRVec3(0);

	// Read the vertices, switching the y and z coordinates and changing the sign of the z coordinate
	
	float v0, v1, v2;

	for (int i = 0; i < numVerts; i++)
	{
		fread(&v0, sizeof(float), 1, bin3ds);
		fread(&v1, sizeof(float), 1, bin3ds);
		fread(&v2, sizeof(float), 1, bin3ds);
		swap32(&v0);
		swap32(&v1);
		swap32(&v2);

		Objects[objindex].Vertexes[i] = rr::RRVec3(v0,v2,-v1) * scale;
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::TexCoordsChunkProcessor(long length, long findex, int objindex)
{
	// The number of texture coordinates
	unsigned short numCoords;

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	// Read the number of coordinates
	fread(&numCoords,sizeof(numCoords),1,bin3ds);
	swap16(&numCoords);

	// Allocate an array to hold the texture coordinates
	Objects[objindex].TexCoords = new rr::RRVec2[numCoords];

	// Set the number of texture coords
	Objects[objindex].numTexCoords = numCoords;

	// Read the texture coordiantes into the array
	float t0, t1;

	for (int i = 0; i < numCoords; i++)
	{
		fread(&t0, sizeof(float), 1, bin3ds);
		fread(&t1, sizeof(float), 1, bin3ds);
		swap32(&t0);
		swap32(&t1);

		Objects[objindex].TexCoords[i] = rr::RRVec2(t0,t1);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::FacesDescriptionChunkProcessor(long length, long findex, int objindex)
{
	ChunkHeader h;
	unsigned short numFaces;	// The number of faces in the object
	unsigned short vertA;		// The first vertex of the face
	unsigned short vertB;		// The second vertex of the face
	unsigned short vertC;		// The third vertex of the face
	unsigned short flags;		// The winding order flags
	long subs;					// Holds our place in the file
	int numMatFaces = 0;		// The number of different materials

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	// Read the number of faces
	fread(&numFaces,sizeof(numFaces),1,bin3ds);
	swap16(&numFaces);

	// Allocate an array to hold the faces
	Objects[objindex].Faces = new unsigned short[numFaces * 3];
	// Store the number of faces
	Objects[objindex].numFaces = numFaces * 3;

	// Read the faces into the array
	for (int i = 0; i < numFaces * 3; i+=3)
	{
		// Read the vertices of the face
		fread(&vertA,sizeof(vertA),1,bin3ds);
		fread(&vertB,sizeof(vertB),1,bin3ds);
		fread(&vertC,sizeof(vertC),1,bin3ds);
		fread(&flags,sizeof(flags),1,bin3ds);
		swap16(&vertA);
		swap16(&vertB);
		swap16(&vertC);
		swap16(&flags);

		// Place them in the array
		Objects[objindex].Faces[i]   = vertA;
		Objects[objindex].Faces[i+1] = vertB;
		Objects[objindex].Faces[i+2] = vertC;

		// Calculate the face's normal
		rr::RRVec3 v1 = Objects[objindex].Vertexes[vertA];
		rr::RRVec3 v2 = Objects[objindex].Vertexes[vertB];
		rr::RRVec3 v3 = Objects[objindex].Vertexes[vertC];

		// calculate the normal
		rr::RRVec3 u = v2 - v3;
		rr::RRVec3 v = v2 - v1;
		rr::RRVec3 n;
		n.x = (u[1]*v[2] - u[2]*v[1]);
		n.y = (u[2]*v[0] - u[0]*v[2]);
		n.z = (u[0]*v[1] - u[1]*v[0]);

		if (!smoothAll)
		{
			// Add this normal to its verts' normals
			Objects[objindex].Normals[vertA] += n;
			Objects[objindex].Normals[vertB] += n;
			Objects[objindex].Normals[vertC] += n;
		}
		else
		{
			// smooth all vertices in the same position
			// warning: slow for big scenes
			for (unsigned k=0;k<3;k++)
			{
				unsigned v = k?((k>1)?vertC:vertB):vertA;
				for (int j=0;j<Objects[objindex].numVerts;j++)
					if (!memcmp(&Objects[objindex].Vertexes[j],&Objects[objindex].Vertexes[v],sizeof(rr::RRVec3)))
					{
						Objects[objindex].Normals[j] += n;
					}
			}
		}
	}

	// Store our current file position
	subs = ftell(bin3ds);

	// Check to see how many materials the faces are split into
	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0x4130:
				numMatFaces++;
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// Split the faces up according to their materials
	if (numMatFaces > 0)
	{
		// Allocate an array to hold the lists of faces divided by material
		Objects[objindex].MatFaces = new MaterialFaces[numMatFaces];
		// Store the number of material faces
		Objects[objindex].numMatFaces = numMatFaces;

		fseek(bin3ds, subs, SEEK_SET);

		int j = 0;

		// Split the faces up
		while (ftell(bin3ds) < (findex + length - 6))
		{
			fread(&h.id,sizeof(h.id),1,bin3ds);
			fread(&h.len,sizeof(h.len),1,bin3ds);
			swap16(&h.id);
			swap32(&h.len);

			switch (h.id)
			{
				case 0x4130:
					// Process the faces and split them up
					FacesMaterialsListChunkProcessor(h.len, ftell(bin3ds), objindex, j);
					j++;
					break;
			}

			fseek(bin3ds, (h.len - 6), SEEK_CUR);
		}
	}
	else
	{
		// Prehistorical file without materials encountered, panic (proceed without material)
		Objects[objindex].MatFaces = new MaterialFaces[1];
		Objects[objindex].numMatFaces = 1;
		Objects[objindex].MatFaces[0].MatIndex = -1; // invalid index = no material
		Objects[objindex].MatFaces[0].subFaces = new unsigned short[numFaces * 3];
		Objects[objindex].MatFaces[0].numSubFaces = numFaces * 3;
		for (int i = 0; i < numFaces * 3; i++)
		{
			Objects[objindex].MatFaces[0].subFaces[i] = Objects[objindex].Faces[i];
		}
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::FacesMaterialsListChunkProcessor(long length, long findex, int objindex, int subfacesindex)
{
	char name[580];				// The material's name
	unsigned short numEntries;	// The number of faces associated with this material
	unsigned short Face;		// Holds the faces as they are read
	int material;				// An index to the Materials array for this material

	// move the file pointer to the beginning of the main
	// chunk's data findex + the size of the header
	fseek(bin3ds, findex, SEEK_SET);

	// Read the material's name
	for (int i = 0; i < 580; i++)
	{
		name[i] = fgetc(bin3ds);
		if (name[i] == 0)
			break;
	}

	// Find the material's index in the Materials array
	for (material = 0; material < numMaterials; material++)
	{
		if (Materials[material].name==name)
			break;
	}

	// Store this value for later so that we can find the material
	Objects[objindex].MatFaces[subfacesindex].MatIndex = material;

	// Read the number of faces associated with this material
	fread(&numEntries,sizeof(numEntries),1,bin3ds);
	swap16(&numEntries);

	// Allocate an array to hold the list of faces associated with this material
	Objects[objindex].MatFaces[subfacesindex].subFaces = new unsigned short[numEntries * 3];
	// Store this number for later use
	Objects[objindex].MatFaces[subfacesindex].numSubFaces = numEntries * 3;

	// Read the faces into the array
	for (int i = 0; i < numEntries * 3; i+=3)
	{
		// read the face
		fread(&Face,sizeof(Face),1,bin3ds);
		swap16(&Face);

		// Add the face's vertices to the list
		Objects[objindex].MatFaces[subfacesindex].subFaces[i] = Objects[objindex].Faces[Face * 3];
		Objects[objindex].MatFaces[subfacesindex].subFaces[i+1] = Objects[objindex].Faces[Face * 3 + 1];
		Objects[objindex].MatFaces[subfacesindex].subFaces[i+2] = Objects[objindex].Faces[Face * 3 + 2];
	}
	
	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}
