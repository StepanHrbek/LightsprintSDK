// EMERGENT GAME TECHNOLOGIES PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Emergent Game Technologies and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.
//
//      Copyright (c) 1996-2008 Emergent Game Technologies.
//      All Rights Reserved.
//
// Emergent Game Technologies, Chapel Hill, North Carolina 27517
// http://www.emergent.net

#ifndef NIMATERIALDETECTOR_H
#define NIMATERIALDETECTOR_H

#include "../supported_formats.h"
#ifdef SUPPORT_GAMEBRYO

#include <NiVersion.h>

#if GAMEBRYO_MAJOR_VERSION==3
	#ifdef RR_IO_BUILD
		// in case of LightsprintIO library, Gamebryo is configured here (static linking)
		#define EE_ECR_NO_IMPORT
		#define EE_EFD_NO_IMPORT
		#define EE_EGF_NO_IMPORT
		#define EE_EGMGI_NO_IMPORT
		#define NIAPPLICATION_NO_IMPORT
		#define NIDX9RENDERER_NO_IMPORT
		#define NIFLOODGATE_NO_IMPORT
		#define NIINPUT_NO_IMPORT
		#define NILIGHTMAPMATERIAL_NO_IMPORT
		#define NIMAIN_NO_IMPORT
		#define NIMESH_NO_IMPORT
		#define NITERRAIN_NO_IMPORT
		#ifdef _DEBUG
			#define EE_CONFIG_DEBUG
			#define EE_EFD_CONFIG_DEBUG
			#define EE_USE_MEMORY_MANAGEMENT
		#else
			#define EE_CONFIG_SHIPPING
			#define EE_EFD_CONFIG_SHIPPING
			#define EE_DISABLE_LOGGING
		#endif
		// Fix gamebryo linking
		// _WINDLL (defined by Microsoft) switches some Gamebryo libraries to dll (see e.g. ecrLibType.h) even if we explicitly say we want static lib
		#undef _WINDLL
	#else
		// in case of Toolbench plugin, Gamebryo is configured in project (dynamic linking)
	#endif
#endif

#include <NiAnimation.h>
#include <NiD3D10Renderer.h>
#include <NiD3D10RenderedTextureData.h>
#include <NiD3DRendererHeaders.h>
#include <NiDebug.h>
#include <NiDX9RenderedTextureData.h>
#include <NiMain.h>
#include <NiMeshLib.h>
#include <NiMeshCullingProcess.h>
#include <windows.h>

#ifndef NIMD_RENDER_TEXTURE_SIZE
#define NIMD_RENDER_TEXTURE_SIZE 64 // in South2, 64 takes 130MB of memory, 128 takes 460MB
#endif

class NiMaterialPointValues : public NiRefObject 
{
public:
    unsigned int m_uiTextureResolution;
    NiPixelDataPtr m_spDiffuseTexture;
    NiPixelDataPtr m_spSpecularTexture;
    NiPixelDataPtr m_spTransmittedTexture;
    NiPixelDataPtr m_spEmittanceTexture;
};

NiSmartPointer(NiMaterialPointValues);

class NiMaterialDetector : public NiRefObject 
{
public:
    static NiMaterialDetector* GetInstance();
    static void Init();
    static void Shutdown();

    NiMaterialPointValuesPtr CreatePointMaterialTextures(NiMesh* pkMesh);

protected:
    NiMaterialDetector();
    static NiPointer<NiMaterialDetector> ms_spInstance;
    
    NiNodePtr m_spNode;
    NiMeshPtr m_spSquarePolygon;
    NiCameraPtr m_spOrthoCamera;
    NiPointLightPtr m_spPointLight;
    NiDirectionalLightPtr m_spDirLight;
    NiRenderedTexturePtr m_spTexture;
    NiRenderTargetGroupPtr m_spRenderTargetGroup;
    LPDIRECT3DTEXTURE9 m_pkD3DReadTexture;

    NiMeshPtr CreateSquarePolygon(float fScale = 1.0);
    NiPixelDataPtr NiMaterialDetector::GetPixelsFromTexture();
    NiPixelDataPtr GetScenePixels(NiColor kClearColor);
    NiPoint3 GetSceneColor(NiColor kClearColor);
    //void SaveTexture(NiPixelData* pkPixelData, char* pcFilename);
};

NiSmartPointer(NiMaterialDetector);

#endif // SUPPORT_GAMEBRYO

#endif // NIMATERIALDETECTOR_H
