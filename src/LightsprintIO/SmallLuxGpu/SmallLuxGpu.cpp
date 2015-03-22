// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Export to SmallLuxGpu .scn/.ply formats.
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

// swaps axes, so that we don't have to rotate environment
RRVec3 convertPos(const RRVec3& a)
{
	return RRVec3(-a.x,a.z,a.y);
}

// swaps axes, so that we don't have to rotate environment
RRVec3 convertDir(const RRVec3& a)
{
	return RRVec3(-a.x,a.z,a.y);
}

// invert v in uv
RRVec2 convertUv(const RRVec2& a)
{
	return RRVec2(a.x,1-a.y);
}

// converts #inf/#nan to 0, they would break .ply
float fix(float a)
{
	return _finite(a) ? a : 0;
}

// converts shininess to rougness as described in http://www.luxrender.net/wiki/LuxRender_Materials_Glossy
float getRoughness(const RRMaterial* m)
{
	return sqrt(2/(m->specularShininess+2));
}

//////////////////////////////////////////////////////////////////////////////
//
// stream helpers

std::ostream & operator<<(std::ostream &os, const RRVec2& a)
{
	return os << fix(a.x) << " " << fix(a.y);
}

std::ostream & operator<<(std::ostream &os, const RRVec3& a)
{
	return os << fix(a.x) << " " << fix(a.y) << " " << fix(a.z);
}

std::set<RRBuffer*> textures; // global, so that << can deduce textureIndex
unsigned numSamplers = 0;

// RR data necessary to generate slg texture (=filename+channel+inversion+gamma)
struct SlgTexture
{
	const RRMaterial* material;
	const RRMaterial::Property* property;
	//const RRLight* light;
	const RRBuffer* buffer;
	SlgTexture(const RRMaterial* _material, const RRMaterial::Property* _property)
	{
		material = _material;
		property = _property;
	//	light = nullptr;
		buffer = (material && property) ? property->texture : nullptr;
	}
	// doesn't work, slg4 light wants filename, not texture
	//SlgTexture(const RRLight* _light)
	//{
	//	material = nullptr;
	//	property = nullptr;
	//	light = _light;
	//	buffer = light ? light->projectedTexture : nullptr;
	//}
};

std::ostream & operator<<(std::ostream &os, const SlgTexture& slgTexture)
{
	if (slgTexture.buffer)
	{
		unsigned textureIndex = 0;
		for (std::set<RRBuffer*>::const_iterator i=textures.begin();i!=textures.end();++i)
			if (*i)
			{
				if (*i==slgTexture.buffer)
				{
					if (slgTexture.property==&slgTexture.material->bumpMap && slgTexture.material->bumpMapTypeHeight)
					{
						os << "sampler" << numSamplers << "\n";
						os << "	scene.textures.sampler" << numSamplers << ".type = scale\n";
						os << "	scene.textures.sampler" << numSamplers << ".texture1 = tex" << textureIndex << "\n";
						os << "	scene.textures.sampler" << numSamplers << ".texture2 = " << 0.01f*slgTexture.material->bumpMap.color.y;
						RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu material: bump works differently, can't be exported accurately.\n"));
						numSamplers++;
					}
					else
					if (slgTexture.property==&slgTexture.material->specularTransmittance)
					{
						os << "sampler" << numSamplers << "\n";
						os << "	scene.textures.sampler" << numSamplers << ".type = add\n";
						os << "	scene.textures.sampler" << numSamplers << ".texture1 = tex" << textureIndex << "\n";
						os << "	scene.textures.sampler" << numSamplers << ".texture2 = 0\n";
						if (slgTexture.material->specularTransmittanceInAlpha)
							os << "	scene.textures.sampler" << numSamplers << ".channel = alpha\n";
						if (!slgTexture.material->specularTransmittanceInAlpha)
							RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu material: current slg4 ignores specular transmittance texture color, works in grayscale.\n"));
						os << "	scene.textures.sampler" << numSamplers << ".gamma = 0.45"; // does not work
						RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu material: specular transmittance has wrong gamma.\n"));
						numSamplers++;
					}
					else
					{
/*
[#39] TODO: kdyz je specTrans=textura
	-zmenit difRefl na difRefl*(1-specTrans)
	-zmenit specRefl na specRefl*(1-specTrans)
*/
						os << "tex" << textureIndex;
					}
					return os;
				}
				textureIndex++;
			}
	}
	// texture not found in textures? fall back to flat color
	if (slgTexture.property)
		return os << slgTexture.property->colorLinear;
	// we should not get here
	RR_ASSERT(0);
	return os;
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

		const RRMatrix3x4Ex& worldMatrix = object->getWorldMatrixRef();
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
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu object: .ply doesn't support multiple uvs, mapping might be broken.\n"));
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
				ofs << convertPos(worldMatrix.getTransformedPosition(mesh->position[i])) << " " << convertDir(worldMatrix.getTransformedNormal(mesh->normal[i])) << "\n";
			else
				ofs << convertPos(worldMatrix.getTransformedPosition(mesh->position[i])) << " " << convertDir(worldMatrix.getTransformedNormal(mesh->normal[i])) << " " << convertUv(mesh->texcoord[uvChannel][i]) << "\n";
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
			if (eye.isOrthogonal())
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu camera: does not support ortho.\n"));
			ofs << "scene.camera.lookat.orig = " << convertPos(eye.getPosition()) << "\n";
			ofs << "scene.camera.lookat.target = " << convertPos(eye.getPosition()+eye.getDirection()) << "\n";
			ofs << "scene.camera.up = " << convertDir(eye.getUp()) << "\n";
			ofs << "scene.camera.fieldofview = " << eye.getFieldOfViewHorizontalDeg() << "\n";
			if (eye.getScreenCenter()!=RRVec2(0))
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu camera: does not support non-zero screenCenter.\n"));
			// dof
			ofs << "scene.camera.focaldistance = " << (eye.dofNear+eye.dofFar)/2 << "\n";
			ofs << "scene.camera.lensradius = " << eye.apertureDiameter << "\n";
			// stereo
			if (eye.stereoMode!=RRCamera::SM_MONO)
			{
				ofs << "scene.camera.horizontalstereo.enable = 1\n";
				ofs << "scene.camera.horizontalstereo.eyesdistance = " << eye.eyeSeparation << "\n";
				//ofs << "scene.camera.horizontalstereo.lensdistance = " << << "\n";
				switch (eye.stereoMode)
				{
					case RRCamera::SM_SIDE_BY_SIDE:
						break;
					case RRCamera::SM_OCULUS_RIFT:
						ofs << "scene.camera.horizontalstereo.oculusrift.barrelpostpro.enable = 1\n";
						break;
					default:
						RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu camera: does not support this stereo mode (only side-by-side and Oculus Rift).\n"));
						break;
				}
			}
			// panorama
			if (eye.panoramaMode!=RRCamera::PM_OFF)
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu camera: does not support panorama modes.\n"));
			ofs << "################################################################################\n";
		}

		// textures
		RRMaterials materials;
		scene->objects.getAllMaterials(materials);
		textures.clear();
		//for (unsigned i=0;i<scene->lights.size();i++)
		//	textures.insert(scene->lights[i]->projectedTexture);
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
		for (std::set<RRBuffer*>::const_iterator i=textures.begin();i!=textures.end();++i)
			if (*i)
			{
				// prepare ascii filename, just in case we need it later
				char asciiFilename[20];
				sprintf(asciiFilename,"texture%d.%s",textrueIndex,"png");

				bool originalIsAscii = true;
				const wchar_t* p = (*i)->filename.w_str();
				while (*p)
					if (*(p++)>127)
						originalIsAscii = false;
				if ((*i)->filename.empty())
				{
					// embedded texture, save it to disk
					RRBuffer* tmp = (*i)->createCopy(); // we don't want to modify (*i) so we save its copy (alternatively we can backup and restore (*i)'s filename and version)
					tmp->save(asciiFilename);
					delete tmp;
					ofs << "scene.textures.tex" << textrueIndex << ".file = " << asciiFilename << "\n";
				}
				else
				if (!originalIsAscii)
				{
					// non-ascii filename, copy file to ascii filename
					bf::copy_file(RR_RR2PATH((*i)->filename),asciiFilename);
					ofs << "scene.textures.tex" << textrueIndex << ".file = " << asciiFilename << "\n";
				}
				else
				{
					// original filename is ascii, keep it
					ofs << "scene.textures.tex" << textrueIndex << ".file = " << (*i)->filename.c_str() << "\n";
				}
				//scene.textures.tex1.channel = alpha
				//ofs << "scene.textures.tex" << textrueIndex << ".gamma = 1.0
				//ofs << "scene.textures.tex" << textrueIndex << ".uvscale = 2 2
				//ofs << "scene.textures.tex" << textrueIndex << ".gain = 7.0
				textrueIndex++;
			}
		if (!textures.empty())
			ofs << "################################################################################\n";

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
				ofs << "scene.lights.env.type = infinite\n";
				ofs << "scene.lights.env.file = " << env2->filename.c_str() << "\n"; //!!! no unicode
				ofs << "scene.lights.env.transformation = 1.0 0.0 0.0 0.0  0.0 -1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0\n";
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
			if (light && light->enabled)
			{
				switch (light->type)
				{
					case RRLight::DIRECTIONAL:
						ofs << "scene.lights.light" << i << ".type = sharpdistant\n";
						ofs << "scene.lights.light" << i << ".direction = " << convertDir(light->direction.normalizedSafe()) << "\n";
						break;
					case RRLight::SPOT:
						if (light->projectedTexture)
						{
							ofs << "scene.lights.light" << i << ".type = projection\n";
							//ofs << "scene.lights.light" << i << ".mapfile = " << SlgTexture(light) << "\n"; //!!! no unicode
							ofs << "scene.lights.light" << i << ".mapfile = " << light->projectedTexture->filename.c_str() << "\n"; //!!! no unicode
							ofs << "scene.lights.light" << i << ".fov = " << RR_RAD2DEG(light->outerAngleRad) << "\n";
							//!!! texture is 90 deg rotated. it seems that transformation depends on position+target
							//ofs << "scene.lights.light" << i << ".transformation = 1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0\n";
							RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu light: projected texture is 90deg rotated.\n"));
						}
						else
						{
							ofs << "scene.lights.light" << i << ".type = spot\n";
							ofs	<< "scene.lights.light" << i << ".coneangle = " << RR_RAD2DEG(light->outerAngleRad) << "\n";
							ofs << "scene.lights.light" << i << ".conedeltaangle = " << RR_RAD2DEG(light->fallOffAngleRad) << "\n";
							ofs << "scene.lights.light" << i << ".power = " << light->spotExponent << "\n"; // it seems to have no effect
							if (light->spotExponent!=1)
								RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu light: does not support spot exponent.\n"));
						}
						ofs << "scene.lights.light" << i << ".target = " << convertPos(light->position+light->direction) << "\n";
						break;
					case RRLight::POINT:
						ofs << "scene.lights.light" << i << ".type = point\n";
						break;
				}
				// slg4_dir and slg4_point use color*gain, but slg4_spot uses only gain
				//ofs << "scene.lights.light" << i << ".color = " << light->color << "\n";
				//ofs << "scene.lights.light" << i << ".gain = 3 3 3\n";
				ofs << "scene.lights.light" << i << ".gain = " << light->color*3 << "\n";
				
				if (light->type!=RRLight::DIRECTIONAL)
				{
					ofs << "scene.lights.light" << i << ".position = " << convertPos(light->position) << "\n";
					if (light->distanceAttenuationType!=RRLight::REALISTIC)
						RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu light: does not support non-REALISTIC distance attenuation.\n"));
				}
				if (!light->castShadows)
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu light: does not support disabled shadows.\n"));
				if (light->directLambertScaled)
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu light: does not support sRGB incorrect adding.\n"));
				if (i+1<scene->lights.size())
					ofs << "\n";
			}
		}
		if (scene->lights.size())
			ofs << "################################################################################\n";

		// materials
		ofs << "scene.materials.null.type = null\n";
		ofs << "#\n";
		for (unsigned i=0;i<materials.size();i++)
		{
			const RRMaterial* m = materials[i];
			if (m)
			{
				if (m->specularReflectance.color==RRVec3(0) && m->specularTransmittance.color==RRVec3(0))
				{
					// [dif] = matte
					ofs << "scene.materials.mat" << i << ".type = matte\n";
					ofs << "scene.materials.mat" << i << ".kd = " << SlgTexture(m,&m->diffuseReflectance) << "\n";
				}
				else
				if (m->specularReflectance.color!=RRVec3(0) && m->specularTransmittance.color==RRVec3(0))
				{
					// [dif+]spec = glossy
					ofs << "scene.materials.mat" << i << ".type = glossy2\n";
					ofs << "scene.materials.mat" << i << ".kd = " << SlgTexture(m,&m->diffuseReflectance) << "\n";
					ofs << "scene.materials.mat" << i << ".ks = " << SlgTexture(m,&m->specularReflectance) << "\n";
					ofs << "scene.materials.mat" << i << ".uroughness = " << getRoughness(m) << "\n";
					ofs << "scene.materials.mat" << i << ".vroughness = " << getRoughness(m) << "\n";
				}
				else
				if (m->diffuseReflectance.color==RRVec3(0) && m->specularTransmittance.color!=RRVec3(0))
				{
					// [spec+]trans = glass
					ofs << "scene.materials.mat" << i << ".type = roughglass\n";
					ofs << "scene.materials.mat" << i << ".kr = " << SlgTexture(m,&m->specularReflectance) << "\n"; // has no effect (2015-03)
					ofs << "scene.materials.mat" << i << ".uroughness = " << getRoughness(m) << "\n";
					ofs << "scene.materials.mat" << i << ".vroughness = " << getRoughness(m) << "\n";
					ofs << "scene.materials.mat" << i << ".kt = " << SlgTexture(m,&m->specularTransmittance) << "\n";
					ofs << "scene.materials.mat" << i << ".interiorior = " << m->refractionIndex << "\n";
					ofs << "scene.materials.mat" << i << ".exteriorior = 1\n";
				}
				else
				{
					if (m->specularReflectance.color==RRVec3(0))
					{
						// dif+transmap      = mix amount=trans mat1=null mat2=matte[dif]
						// dif+transcol      = mix amount=trans mat1=null mat2=matte[dif*(1-transcol)]
						ofs << "scene.materials.mat" << i << "_.type = matte\n";
						ofs << "scene.materials.mat" << i << "_.kd = " << SlgTexture(m,&m->diffuseReflectance) << "\n"; //!!! *(1-transcol)
					}
					else
					{
						// dif+spec+transmap = mix amount=trans mat1=null mat2=glossy[dif,spec]
						// dif+spec+transcol = mix amount=trans mat1=null mat2=glossy[dif*(1-transcol),spec*(1-transcol)]
						ofs << "scene.materials.mat" << i << "_.type = glossy2\n";
						ofs << "scene.materials.mat" << i << "_.kd = " << SlgTexture(m,&m->diffuseReflectance) << "\n"; //!!! *(1-transcol)
						ofs << "scene.materials.mat" << i << "_.ks = " << SlgTexture(m,&m->specularReflectance) << "\n"; //!!! *(1-transcol)
						ofs << "scene.materials.mat" << i << "_.uroughness = " << getRoughness(m) << "\n";
						ofs << "scene.materials.mat" << i << "_.vroughness = " << getRoughness(m) << "\n";
					}
					ofs << "scene.materials.mat" << i << ".type = mix\n";
					ofs << "scene.materials.mat" << i << ".amount = " << SlgTexture(m,&m->specularTransmittance) << "\n";
					ofs << "scene.materials.mat" << i << ".material" << ((m->specularTransmittance.texture && m->specularTransmittanceMapInverted==m->specularTransmittanceInAlpha)?"2":"1") << " = null\n";
					ofs << "scene.materials.mat" << i << ".material" << ((m->specularTransmittance.texture && m->specularTransmittanceMapInverted==m->specularTransmittanceInAlpha)?"1":"2") << " = mat" << i << "_\n";
				}
				if (m->diffuseEmittance.color!=RRVec3(0))
				{
					ofs << "scene.materials.mat" << i << ".emission = " << SlgTexture(m,&m->diffuseEmittance) << "\n";
				}
				if (m->bumpMap.texture)
				{
					ofs << "scene.materials.mat" << i << (m->bumpMapTypeHeight?".bumptex = ":".normaltex = ") << SlgTexture(m,&m->bumpMap) << "\n";
				}
				if (i+1<materials.size())
					ofs << "#\n";
				if (m->specularTransmittanceKeyed)
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu material: does not support 1bit specular transmittance.\n"));
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
					RR_LIMITED_TIMES(1,RRReporter::report(WARN,"SmallLuxGpu object: doesn't support multiple materials per mesh, split objects by materials before exporting.\n"));
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
