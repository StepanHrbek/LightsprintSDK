//////////////////////////////////////////////////////////////////////
//
// 3D Studio Model Class
// by: Matthew Fairfax
// modified by: Stepan Hrbek
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

#include <cassert>
#include <cmath>
#include <cstring>
#ifndef RR_IO_BUILD
#include <GL/glew.h>
#endif
#include "Model_3DS.h"
#ifndef RR_IO_BUILD
#include "Lightsprint/GL/UberProgramSetup.h"
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

//////////////////////////////////////////////////////////////////////
// utility functions for handling little endian on a big endian system
//////////////////////////////////////////////////////////////////////

#ifdef __PPC__
#define RR_BIG_ENDIAN
#endif

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
	// Initialization
	memset(this,0,sizeof(*this));

	// Set up the path
	path = new char[580];
	path[0] = 0;//sprintf(path, "");

	// Set the scale to one
	scale = 1.0f;
}

Model_3DS::~Model_3DS()
{
	delete[] path;
	delete[] Materials;
	delete[] Objects;
}

bool Model_3DS::Load(const char *filename, float ascale)
{
	char buf[500];
	strncpy(buf,filename,499);
	buf[499]=0;
	char* name = buf;

	scale = ascale;
	// holds the main chunk header
	ChunkHeader main;

	// strip "'s
	if (strstr(name, "\""))
		name = strtok(name, "\"");

	// Find the path
	if (strstr(name, "/") || strstr(name, "\\"))
	{
		// Holds the name of the model minus the path
		char *temp;

		// Find the name without the path
		if (strstr(name, "/"))
			temp = strrchr(name, '/');
		else
			temp = strrchr(name, '\\');

		// Allocate space for the path
		delete[] path;
		path = new char[strlen(name)-strlen(temp)+10];
		// Get a pointer to the end of the path and name
		char *src = name + strlen(name) - 1;

		// Back up until a \ or the start
		while (src != path && !((*(src-1)) == '\\' || (*(src-1)) == '/'))
			src--;

		// Copy the path into path
		memcpy (path, name, src-name);
		path[src-name] = 0;
	}

	// Load the file
	bin3ds = fopen(name,"rb");

	if (!bin3ds)
	{
		printf("file not found: %s\n",name);
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
			Objects[i].Normals[g].normalize();
		}
	}

	// For future reference
	modelname = name;

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

	// Let's build simple colored textures for the materials w/o a texture
	for (int j = 0; j < numMaterials; j++)
	{
		if (!Materials[j].diffuseReflectance.texture)
		{
			Materials[j].diffuseReflectance.texture = rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGBF,true,(unsigned char*)&Materials[j].diffuseReflectance.color[0]);
		}
	}
	UpdateCenter();

	// check that transformations are identity
	bool identity = true;
	/*if (pos.x||pos.y||pos.z||rot.x||rot.y||rot.z||scale!=1)
	{
		identity=false;
		assert(0);
	}*/
	for (int i = 0; i < numObjects; i++)
	{
		if (Objects[i].pos.x||Objects[i].pos.y||Objects[i].pos.z||Objects[i].rot.x||Objects[i].rot.y||Objects[i].rot.z)
		{
			identity=false;
			assert(0);
		}
	}
	if (!identity)
		rr::RRReporter::report(rr::WARN,"Object transformations in .3ds are not supported.\n");

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
			localMinY = MIN(localMinY,Objects[o].Vertexes[v].y);
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
				glEnableClientState(GL_COLOR_ARRAY);
				glColorPointer(3, GL_FLOAT, 0, vertexColors);
				if (releaseVertexColors) releaseVertexColors(model,i);
			}
		}
		else
		{
			glColor3f(0,0,0);
		}

		// Enable texture coordiantes, normals, and vertices arrays
		if (texturedDiffuse)
		{
			glClientActiveTexture(GL_TEXTURE0+rr_gl::MULTITEXCOORD_MATERIAL_DIFFUSE);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		if (texturedEmissive)
		{
			glClientActiveTexture(GL_TEXTURE0+rr_gl::MULTITEXCOORD_MATERIAL_EMISSIVE);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		if (lit)
			glEnableClientState(GL_NORMAL_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);

		// Point them to the objects arrays
		if (texturedDiffuse || texturedEmissive)
			glTexCoordPointer(2, GL_FLOAT, 0, Objects[i].TexCoords);
		if (lit)
			glNormalPointer(GL_FLOAT, 0, Objects[i].Normals);
		glVertexPointer(3, GL_FLOAT, 0, Objects[i].Vertexes);

		// Loop through the faces as sorted by material and draw them
		for (int j = 0; j < Objects[i].numMatFaces; j ++)
		{
			// Use the material's texture
			if (texturedDiffuse || texturedEmissive)
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

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

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

	// First count the number of Objects and Materials
	while (ftell(bin3ds) < (findex + length - 6))
	{
		fread(&h.id,sizeof(h.id),1,bin3ds);
		fread(&h.len,sizeof(h.len),1,bin3ds);
		swap16(&h.id);
		swap32(&h.len);

		switch (h.id)
		{
			case 0x4000:
				numObjects++;
				break;
			case 0xAFFF:
				numMaterials++;
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// Now load the materials
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
			}

			fseek(bin3ds, (h.len - 6), SEEK_CUR);
		}
	}

	// Load the Objects (individual meshes in the whole model)
	if (numObjects > 0)
	{
		Objects = new Object[numObjects];

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
					ObjectChunkProcessor(h.len, ftell(bin3ds), j);
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

void Model_3DS::MaterialChunkProcessor(long length, long findex, int matindex)
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
			case 0xA000:
				// Material name
				MaterialNameChunkProcessor(h.len, ftell(bin3ds), matindex);
				break;
			case 0xA020:
				// Diffuse color
				ColorChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].diffuseReflectance);
				break;
			case 0xA030:
				// Specular color
				//ColorChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].specularReflectance);
				break;
			case 0xA084:
				// Self illum... what type is it?
				//ColorChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].diffuseEmittance);
				break;
			case 0xa050:
				// Transparency percent
				Materials[matindex].specularTransmittance.color = rr::RRVec3(ReadPercentage()*0.01f);
				break;
			case 0xa041:
				// Shininess strength percent
				Materials[matindex].specularReflectance.color = rr::RRVec3(ReadPercentage()*0.01f);
				break;

			case 0xA200:
				// Texture map 1
				TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].diffuseReflectance);
				break;
			case 0xA210:
				// Opacity map
				TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].specularTransmittance);
				Materials[matindex].specularTransmittance.texture->invert();
				break;
			case 0xA204:
				// Specular map
				TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].specularReflectance);
				break;
			case 0xA33D:
				// Self illum map
				TextureMapChunkProcessor(h.len, ftell(bin3ds), Materials[matindex].diffuseEmittance);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

// returns value from percentage chunk
unsigned Model_3DS::ReadPercentage()
{
	unsigned char arr[8];
	fread(arr,8,1,bin3ds);
	fseek(bin3ds, -8, SEEK_CUR);
	assert(arr[0]==0x30 && arr[1]==0 && arr[2]==8 && arr[3]==0 && arr[4]==0 && arr[5]==0 && arr[7]==0);
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
	Materials[matindex].name = _strdup(buf);

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::ColorChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty)
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
				FloatColorChunkProcessor(h.len, ftell(bin3ds), materialProperty);
				break;
			case 0x0011:
				// A rgb int color chunk
				IntColorChunkProcessor(h.len, ftell(bin3ds), materialProperty);
				break;
			case 0x0013:
				// A rgb gamma corrected float color chunk
				FloatColorChunkProcessor(h.len, ftell(bin3ds), materialProperty);
				break;
			case 0x0012:
				// A rgb gamma corrected int color chunk
				IntColorChunkProcessor(h.len, ftell(bin3ds), materialProperty);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::FloatColorChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty)
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

	materialProperty.color[0] = r;
	materialProperty.color[1] = g;
	materialProperty.color[2] = b;

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::IntColorChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty)
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

	materialProperty.color[0] = r/255.0f;
	materialProperty.color[1] = g/255.0f;
	materialProperty.color[2] = b/255.0f;

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::TextureMapChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty)
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
			case 0xA300:
				// Read the name of texture in the Diffuse Color map
				MapNameChunkProcessor(h.len, ftell(bin3ds), materialProperty);
				break;
		}

		fseek(bin3ds, (h.len - 6), SEEK_CUR);
	}

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::MapNameChunkProcessor(long length, long findex, rr::RRMaterial::Property& materialProperty)
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
	char fullname[580];
	sprintf(fullname, "%s%s", path, name);
	materialProperty.texture = rr::RRBuffer::load(fullname,NULL);
	materialProperty.texcoord = Material::CH_DIFFUSE_SPECULAR_EMISSIVE_OPACITY;
	if (materialProperty.texture)
	{
		rr::RRScaler* scaler = rr::RRScaler::createFastRgbScaler();
		materialProperty.updateColorFromTexture(scaler,false,rr::RRMaterial::UTA_DELETE);
		delete scaler;
	}
	else
		rr::RRReporter::report(rr::WARN,"Texture %s not found.\n",fullname);

	// move the file pointer back to where we got it so
	// that the ProcessChunk() which we interrupted will read
	// from the right place
	fseek(bin3ds, findex, SEEK_SET);
}

void Model_3DS::ObjectChunkProcessor(long length, long findex, int objindex)
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
				TriangularMeshChunkProcessor(h.len, ftell(bin3ds), objindex);
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
		if (Materials[material].name && !strcmp(name,Materials[material].name))
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
