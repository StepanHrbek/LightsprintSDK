// --------------------------------------------------------------------------
// Lightsprint adapters for Lightsprint SmallLuxGpu .scn/.ply formats.
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_SMALLLUXGPU

#include <string>
#include <set>
#include "SmallLuxGpu.h"
#include "Lightsprint/RRScene.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// conversion helpers

RRVec3 convertPos(const RRVec3& a)
{
	return RRVec3(-a.x,a.z,a.y);
}

RRVec3 convertDir(const RRVec3& a)
{
	return RRVec3(-a.x,a.z,a.y);
}

//////////////////////////////////////////////////////////////////////////////
//
// stream helpers

std::ostream & operator<<(std::ostream &os, const RRVec2& a)
{
	return os << a.x << " " << a.y;
}

std::ostream & operator<<(std::ostream &os, const RRVec3& a)
{
	return os << a.x << " " << a.y << " " << a.z;
}

std::set<const RRBuffer*> textures; // global, so that << can deduce textureIndex

std::ostream & operator<<(std::ostream &os, const RRMaterial::Property& a)
{
	if (a.texture)
	{
		unsigned textureIndex = 0;
		for (std::set<const RRBuffer*>::const_iterator i=textures.begin();i!=textures.end();++i)
			if (*i)
			{
				if (*i==a.texture)
					return os << "tex" << textureIndex;
				textureIndex++;
			}
		// texture not found in textures? fall back to flat color
	}
	return os << a.color;
}


//////////////////////////////////////////////////////////////////////////////
//
// .ply

bool savePly(const RRObject* object, const RRString& filename)
{
	if (!object)
	{
		return false;
	}
	try
	{
		bf::ofstream ofs(RR_RR2PATH(filename),std::ios::out|std::ios::binary|std::ios::trunc); // binary because valid .ply needs 0a on first line, 0d 0a is wrong
		if (!ofs || ofs.bad())
		{
			rr::RRReporter::report(rr::WARN,"File %ls can't be created, object not saved.\n",filename.w_str());
			return false;
		}

		const RRMatrix3x4& worldMatrix = object->getWorldMatrixRef();
		const RRMatrix3x4& worldMatrixInv = object->getInverseWorldMatrixRef();
		const RRMeshArrays* mesh = dynamic_cast<const RRMeshArrays*>(object->getCollider()->getMesh());
		unsigned numTriangles = mesh->getNumTriangles();
		unsigned numVertices = mesh->getNumVertices();

		int uvChannel = -1;
		for (unsigned i=0;i<mesh->texcoord.size();i++)
			if (mesh->texcoord[i])
			{
				if (uvChannel==-1)
					uvChannel = i;
				else
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu .ply doesn't support multiple uvs, mapping might be broken.\n"));
			}

		// header
		ofs << "ply\n";
		ofs << "format ascii 1.0\n";
		ofs << "comment Created by Lightsprint SDK\n";
		ofs << "element vertex " << mesh->getNumVertices() << "\n";
		ofs << "property float x\n";
		ofs << "property float y\n";
		ofs << "property float z\n";
		ofs << "property float nx\n";
		ofs << "property float ny\n";
		ofs << "property float nz\n";
		if (uvChannel!=-1)
		{
			ofs << "property float s\n";
			ofs << "property float t\n";
		}
		ofs << "element face " << numTriangles << "\n";
		ofs << "property list uchar uint vertex_indices\n";
		ofs << "end_header\n";

		// vertices
		for (unsigned i=0;i<numVertices;i++)
		{
			if (uvChannel==-1)
				ofs << convertPos(worldMatrix.getTransformedPosition(mesh->position[i])) << " " << convertDir(worldMatrixInv.getTransformedNormal(mesh->normal[i])) << "\n";
			else
				ofs << convertPos(worldMatrix.getTransformedPosition(mesh->position[i])) << " " << convertDir(worldMatrixInv.getTransformedNormal(mesh->normal[i])) << " " << mesh->texcoord[uvChannel][i] << "\n";
		}

		// triangles
		for (unsigned i=0;i<numTriangles;i++)
		{
			RRMesh::Triangle t;
			mesh->getTriangle(i,t);
			ofs << "3 " << t[0] << " " << t[1] << " " << t[2] << "\n";
		}

		return true;
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to save %ls.\n",filename.w_str());
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// .scn

bool saveSmallLuxGpu(const RRScene* scene, const RRString& filename)
{
	if (!scene)
	{
		return false;
	}
	try
	{
		bf::ofstream ofs(RR_RR2PATH(filename),std::ios::out|std::ios::trunc);
		if (!ofs || ofs.bad())
		{
			rr::RRReporter::report(rr::WARN,"File %ls can't be created, scene not saved.\n",filename.w_str());
			return false;
		}

		// camera
		if (scene->cameras.size())
		{
			const RRCamera& eye = scene->cameras[0];
			ofs << "scene.camera.lookat = " << convertPos(eye.getPosition()) << " " << convertPos(eye.getPosition()+eye.getDirection()) << "\n";
			ofs << "scene.camera.up = " << convertDir(eye.getUp()) << "\n";
			ofs << "scene.camera.focaldistance = " << eye.focalLength << "\n";
			ofs << "scene.camera.fieldofview = " << eye.getFieldOfViewHorizontalDeg() << "\n";
			ofs << "################################################################################\n";
		}

		// environment
		if (scene->environment)
		{
			RRReportInterval report(INF2,"Exporting environment...\n");
			RRBuffer* env2 = scene->environment->createEquirectangular();
			if (env2)
			{
				bf::path skyFilename = RR_RR2PATH(filename);
				bool hdr = env2->getElementBits()>32;
				skyFilename.replace_extension(hdr?".exr":".png");
				if (hdr)
					env2->brightnessGamma(RRVec4(1),RRVec4(0.45f));
				env2->save(RR_PATH2RR(skyFilename));
				if (hdr)
					env2->brightnessGamma(RRVec4(1),RRVec4(2.222222f));
				ofs << "scene.infinitelight.file = " << skyFilename.filename().string() << "\n"; //!!! breaks unicode
				//ofs << "scene.infinitelight.shift = 0.0 0.0\n;
				ofs << "################################################################################\n";
				delete env2;
			}
		}

		// lights
		unsigned numSunlights = 0;
		for (unsigned i=0;i<scene->lights.size();i++)
		{
			RRLight* light = scene->lights[i];
			if (light && light->enabled) switch (light->type)
			{
				case RRLight::DIRECTIONAL:
					ofs << "scene.sunlight.dir = " << convertDir(-light->direction) << "\n";
					ofs << "scene.sunlight.gain = " << light->color << "\n";
					if (numSunlights++)
						RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu doesn't support multiple directional lights.\n"));
					break;
				case RRLight::SPOT:
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu doesn't support spotlights, exported as pointlight.\n"));
					// intentionally no break
				case RRLight::POINT:
					{
					const float CUBE_SIZE = 0.01f;
					ofs << "scene.materials.light" << i << ".type = glass\n";
					ofs << "scene.materials.light" << i << ".kd = 0 0 0\n";
					ofs << "scene.materials.light" << i << ".kr = 0 0 0\n";
					ofs << "scene.materials.light" << i << ".kt = 1 1 1\n";
					ofs << "scene.materials.light" << i << ".emission = " << (light->color * (4*RR_PI/(CUBE_SIZE*CUBE_SIZE*6))) << "\n";
					if (light->distanceAttenuationType!=RRLight::PHYSICAL)
						RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu supports only lights with PHYSICAL distance attenuation.\n"));
					bf::path plyFilename = RR_RR2PATH(filename);
					char tmp[25];
					sprintf(tmp,".light%d.ply",i);
					plyFilename.replace_extension(tmp);
					ofs << "scene.objects.light" << i << ".ply = " << plyFilename.filename().string() << "\n"; //!!! breaks unicode
					ofs << "scene.objects.light" << i << ".material = light" << i << "\n";

					// create temporary box at light->position[j]
					enum {TRIANGLES=12,VERTICES=24};
					rr::RRMeshArrays* arrays = new rr::RRMeshArrays;
					arrays->resizeMesh(TRIANGLES,VERTICES,NULL,false,false);
					for (unsigned i=0;i<TRIANGLES;i++)
						for (unsigned j=0;j<3;j++)
							arrays->triangle[i][j] = i*2+((i%2)?1-(((i/2)%2)?2-j:j):(((i/2)%2)?2-j:j));
					for (unsigned i=0;i<VERTICES;i++)
						for (unsigned j=0;j<3;j++)
							arrays->position[i][j] = ( ((j==((i/8)%3)) ? ((i/4)%2) : ((j==((1+i/8)%3)) ? ((i/2)%2) : (i%2))) - 0.5f )*CUBE_SIZE + light->position[j];
					arrays->buildNormals();
					bool aborting = false;
					RRCollider* collider = RRCollider::create(arrays,NULL,RRCollider::IT_LINEAR,aborting);
					RRObject object;
					object.setCollider(collider);
					savePly(&object,RR_PATH2RR(plyFilename));
					delete collider;
					delete arrays;
					}
					break;
			}
			ofs << ((i+1<scene->lights.size()) ? "#\n" : "################################################################################\n");
		}

		// textures
		RRMaterials materials;
		scene->objects.getAllMaterials(materials);
		textures.clear();
		for (unsigned i=0;i<materials.size();i++)
			if (materials[i])
			{
				textures.insert(materials[i]->diffuseReflectance.texture);
				textures.insert(materials[i]->specularReflectance.texture);
				textures.insert(materials[i]->diffuseEmittance.texture);
				textures.insert(materials[i]->specularTransmittance.texture);
				textures.insert(materials[i]->bumpMap.texture);
			}
		unsigned textrueIndex = 0;
		for (std::set<const RRBuffer*>::const_iterator i=textures.begin();i!=textures.end();++i)
			if (*i)
			{
				ofs << "scene.textures.tex" << textrueIndex << ".file = " << (*i)->filename.c_str() << "\n"; //!!! breaks unicode
				//ofs << "scene.textures.tex" << textrueIndex << ".gamma = 1.0
				//ofs << "scene.textures.tex" << textrueIndex << ".uvscale = 2 2
				//ofs << "scene.textures.tex" << textrueIndex << ".gain = 7.0
				textrueIndex++;
			}
		ofs << "################################################################################\n";

		// materials
		for (unsigned i=0;i<materials.size();i++)
		{
			const RRMaterial* m = materials[i];
			if (m)
			{
				if (m->diffuseReflectance.color!=RRVec3(0) && (m->specularTransmittance.color!=RRVec3(0) || m->specularReflectance.color!=RRVec3(0)))
				{
					ofs << "scene.materials.mat" << i << "_1.type = matte\n";
					ofs << "scene.materials.mat" << i << "_1.kd = " << m->diffuseReflectance << "\n";
					ofs << "scene.materials.mat" << i << "_1.emission = " << m->diffuseEmittance << "\n";
					if (m->bumpMap.texture)
						ofs << "scene.materials.mat" << i << (m->bumpMapTypeHeight?"_1.bumptex = ":"_1.normaltex = ") << m->bumpMap << "\n";

					ofs << "scene.materials.mat" << i << "_2.type = " << ((m->specularTransmittance.color!=RRVec3(0)) ? "glass" : "metal") << "\n";
					ofs << "scene.materials.mat" << i << "_2.kr = " << m->specularReflectance << "\n";
					ofs << "scene.materials.mat" << i << "_2.exp = " << m->specularShininess << "\n";
					ofs << "scene.materials.mat" << i << "_2.kt = " << m->specularTransmittance << "\n";
					if (m->bumpMap.texture)
						ofs << "scene.materials.mat" << i << (m->bumpMapTypeHeight?"_2.bumptex = ":"_2.normaltex = ") << m->bumpMap << "\n";
					ofs << "scene.materials.mat" << i << "_2.iorinside = " << m->refractionIndex << "\n";
					ofs << "scene.materials.mat" << i << "_2.ioroutside = 1\n";

					ofs << "scene.materials.mat" << i << ".type = mix\n";
					ofs << "scene.materials.mat" << i << ".amount = 0.5\n";
					ofs << "scene.materials.mat" << i << ".material1 = mat" << i << "_1\n";
					ofs << "scene.materials.mat" << i << ".material2 = mat" << i << "_2\n";
				}
				else
				{
					ofs << "scene.materials.mat" << i << ".type = " << (m->specularTransmittance.color!=RRVec3(0)?"glass":(m->specularReflectance.color!=RRVec3(0)?"metal":"matte")) << "\n";
					ofs << "scene.materials.mat" << i << ".kd = " << m->diffuseReflectance << "\n";
					ofs << "scene.materials.mat" << i << ".kr = " << m->specularReflectance << "\n";
					ofs << "scene.materials.mat" << i << ".exp = " << m->specularShininess << "\n";
					ofs << "scene.materials.mat" << i << ".kt = " << m->specularTransmittance << "\n";
					ofs << "scene.materials.mat" << i << ".emission = " << m->diffuseEmittance << "\n";
					if (m->bumpMap.texture)
						ofs << "scene.materials.mat" << i << (m->bumpMapTypeHeight?".bumptex = ":".normaltex = ") << m->bumpMap << "\n";
					ofs << "scene.materials.mat" << i << ".iorinside = " << m->refractionIndex << "\n";
					ofs << "scene.materials.mat" << i << ".ioroutside = 1\n";
				}
				if (i+1<materials.size())
					ofs << "#\n";
			}
		}
		ofs << "################################################################################\n";

		// objects
		for (unsigned i=0;i<scene->objects.size();i++)
		{
			RRObject* object = scene->objects[i];
			if (object)
			{
				bf::path plyFilename = RR_RR2PATH(filename);
				char tmp[20];
				sprintf(tmp,".%d.ply",i);
				plyFilename.replace_extension(tmp);
				ofs << "scene.objects.obj" << i << ".ply = " << plyFilename.filename().string() << "\n"; //!!! breaks unicode
				savePly(object,RR_PATH2RR(plyFilename));
				if (object->faceGroups.size()!=1)
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu doesn't support multiple materials per mesh, split objects by materials before exporting.\n"));
				if (object->faceGroups.size())
				{
					const RRMaterial* m = object->faceGroups[0].material;
					unsigned materialIndex = 0;
					for (unsigned j=0;j<materials.size();j++)
						if (materials[j]==m)
							materialIndex = j;
					ofs << "scene.objects.obj" << i << ".material = mat" << materialIndex << "\n";
				}
			}
		}

		return true;
	}
	catch(...)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to save %ls.\n",filename.w_str());
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// main

void registerSmallLuxGpu()
{
	RRScene::registerSaver("*.scn",saveSmallLuxGpu);
}

#endif // SUPPORT_LIGHTSPRINT
