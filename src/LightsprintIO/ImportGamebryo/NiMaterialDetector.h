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

#include <NiAnimation.h>
#include <NiDebug.h>
#include <NiMain.h>
#include <NiMeshLib.h>
#include <NiMeshCullingProcess.h>
#include <windows.h>

#include <NiD3DRendererHeaders.h>
#include <NiDX9RenderedTextureData.h>

#include <NiD3D10Renderer.h>
#include <NiD3D10RenderedTextureData.h>

#ifndef NIMD_RENDER_TEXTURE_SIZE
#define NIMD_RENDER_TEXTURE_SIZE 128
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
    static void Init(HWND hWnd);
    static void Shutdown();

    NiMaterialPointValuesPtr CreatePointMaterialTextures(NiMesh* pkMesh);

protected:
    NiMaterialDetector(HWND hWnd);
    static NiPointer<NiMaterialDetector> ms_spInstance;
    
    NiRendererPtr m_spRenderer;
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
