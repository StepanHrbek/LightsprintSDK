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

#include "../supported_formats.h"
#ifdef SUPPORT_GAMEBRYO

#include "NiMaterialDetector.h"
//#include "NiTGAWriter.h"
#include <NiPoint2.h>

#pragma comment(lib, "NiDX9Renderer.lib")
#pragma comment(lib, "dxguid.lib")

NiPointer<NiMaterialDetector> NiMaterialDetector::ms_spInstance = NULL;
//---------------------------------------------------------------------------
NiMaterialDetector::NiMaterialDetector(HWND hWnd)
{
    m_spRenderer = NiDX9Renderer::Create(0, 0, 0, hWnd, NULL);

    // Create the square polygon
    m_spNode = NiNew NiNode;
    m_spSquarePolygon = CreateSquarePolygon(1.0);
    m_spNode->AttachChild(m_spSquarePolygon);

    // Create an orthographic camera
    m_spOrthoCamera = NiNew NiCamera();
    NiFrustum kFrustum(-0.5, 0.5, 0.5, -0.5, 0, 100, true);
    m_spOrthoCamera->SetViewFrustum(kFrustum);

    // Move the polygon in front of the camera
    NiPoint3 kTrn = m_spOrthoCamera->GetWorldLocation() +
        (1.0f * m_spOrthoCamera->GetWorldDirection());
    m_spSquarePolygon->SetTranslate(kTrn);
    m_spSquarePolygon->Update(0);

    // Create the light we will need for diffuse and specular tests
    m_spPointLight = NiNew NiPointLight();
    m_spPointLight->SetLinearAttenuation(0);
    m_spPointLight->SetAmbientColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spPointLight->SetDiffuseColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spPointLight->SetSpecularColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spPointLight->SetTranslate(m_spOrthoCamera->GetTranslate());

    // Create the light we will need for diffuse and specular tests
    m_spDirLight = NiNew NiDirectionalLight();
    m_spDirLight->SetAmbientColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spDirLight->SetDiffuseColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spDirLight->SetSpecularColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spDirLight->SetRotate(NI_PI*0.5f, 0.0f, 0.0f, 1.0f);;

    // Create a small render texture to render all of this to, and a
    // render target group to pass to the renderer
    m_spTexture = NiRenderedTexture::Create(NIMD_RENDER_TEXTURE_SIZE, 
        NIMD_RENDER_TEXTURE_SIZE, NiRenderer::GetRenderer());
    NIASSERT(m_spTexture);
    m_spRenderTargetGroup = NiRenderTargetGroup::Create(
        m_spTexture->GetBuffer(), NiRenderer::GetRenderer(), true);
    NIASSERT(m_spRenderTargetGroup);

    // Create a lockable Direct3D texture that we can use for reading
    // the pixel data.
    NiDX9Renderer *pDX9Renderer = NiDynamicCast(NiDX9Renderer, 
        NiRenderer::GetRenderer());
	NIASSERT(pDX9Renderer);
    LPDIRECT3DDEVICE9 pkDevice = pDX9Renderer->GetD3DDevice();
    NIASSERT(pkDevice);

    NiDX9RenderedTextureData* pkRenderData = 
        (NiDX9RenderedTextureData*)m_spTexture->GetRendererData();
    NIASSERT(pkRenderData);
    LPDIRECT3DTEXTURE9 pkD3DTexture = 
        (LPDIRECT3DTEXTURE9)(pkRenderData->GetD3DTexture());
    NIASSERT(pkD3DTexture);

    D3DSURFACE_DESC kSurfDesc;
    pkD3DTexture->GetLevelDesc(0, &kSurfDesc);

    pkDevice->CreateTexture(kSurfDesc.Width, kSurfDesc.Height,
        1, 0, kSurfDesc.Format, D3DPOOL_SYSTEMMEM, &m_pkD3DReadTexture, 0);
    NIASSERT(m_pkD3DReadTexture);
}
//---------------------------------------------------------------------------
NiMaterialDetector* NiMaterialDetector::GetInstance()
{
    NIASSERT(ms_spInstance);
    return ms_spInstance;
}
//---------------------------------------------------------------------------
void NiMaterialDetector::Init(HWND hWnd)
{
    NIASSERT(!ms_spInstance);
    ms_spInstance = NiNew NiMaterialDetector(hWnd);
}
//---------------------------------------------------------------------------
NiMeshPtr NiMaterialDetector::CreateSquarePolygon(
    float fScale /* = 1.0 */)
{
    fScale *= 0.5;

    NiPoint3 akVertex[4];
    akVertex[0] = NiPoint3(0.0f, -fScale, -fScale);
    akVertex[1] = NiPoint3(0.0f, +fScale, -fScale);
    akVertex[2] = NiPoint3(0.0f, +fScale, +fScale);
    akVertex[3] = NiPoint3(0.0f, -fScale, +fScale);

    NiPoint2 akTexture[4];
    akTexture[1] = NiPoint2(0.0f, 1.0f);
    akTexture[2] = NiPoint2(1.0f, 1.0f);
    akTexture[3] = NiPoint2(1.0f, 0.0f);
    akTexture[0] = NiPoint2(0.0f, 0.0f);

    NiPoint3 akNormal[4];
    akNormal[0] = NiPoint3(-1.0f, 0.0f, 0.0f);
    akNormal[1] = NiPoint3(-1.0f, 0.0f, 0.0f);
    akNormal[2] = NiPoint3(-1.0f, 0.0f, 0.0f);
    akNormal[3] = NiPoint3(-1.0f, 0.0f, 0.0f);

    NiPoint3 akBinormal[4];
    akBinormal[0] = NiPoint3(0.0f, 0.0f, -1.0f);
    akBinormal[1] = NiPoint3(0.0f, 0.0f, -1.0f);
    akBinormal[2] = NiPoint3(0.0f, 0.0f, -1.0f);
    akBinormal[3] = NiPoint3(0.0f, 0.0f, -1.0f);

    NiPoint3 akTangent[4];
    akTangent[0] = NiPoint3(0.0f, -1.0f, 0.0f);
    akTangent[1] = NiPoint3(0.0f, -1.0f, 0.0f);
    akTangent[2] = NiPoint3(0.0f, -1.0f, 0.0f);
    akTangent[3] = NiPoint3(0.0f, -1.0f, 0.0f);

    // Create 4 triangles from 4 vertices, for a "one-sided polygon".
    unsigned short ausTriList[6];
    ausTriList[0] = 1;  ausTriList[1] = 0;  ausTriList[2] = 3;
    ausTriList[3] = 1;  ausTriList[4] = 3;  ausTriList[5] = 2;

    NiMesh* spMesh = NiNew NiMesh();
    
    spMesh->AddStream(NiCommonSemantics::POSITION(), 0, 
        NiDataStreamElement::F_FLOAT32_3, 4, NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC, NiDataStream::USAGE_VERTEX,
        akVertex); 

    spMesh->AddStream(NiCommonSemantics::TEXCOORD(), 0, 
        NiDataStreamElement::F_FLOAT32_2, 4,  NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC, NiDataStream::USAGE_VERTEX, 
        akTexture);

    spMesh->AddStream(NiCommonSemantics::TEXCOORD(), 1, 
        NiDataStreamElement::F_FLOAT32_2, 4,  NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC, NiDataStream::USAGE_VERTEX, 
        akTexture);

    spMesh->AddStream(NiCommonSemantics::TEXCOORD(), 2, 
        NiDataStreamElement::F_FLOAT32_2, 4,  NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC, NiDataStream::USAGE_VERTEX, 
        akTexture);

    spMesh->AddStream(NiCommonSemantics::TEXCOORD(), 3, 
        NiDataStreamElement::F_FLOAT32_2, 4,  NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC, NiDataStream::USAGE_VERTEX, 
        akTexture);

    spMesh->AddStream(NiCommonSemantics::TEXCOORD(), 4, 
        NiDataStreamElement::F_FLOAT32_2, 4,  NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC, NiDataStream::USAGE_VERTEX, 
        akTexture);

    spMesh->AddStream(NiCommonSemantics::INDEX(), 0, 
        NiDataStreamElement::F_UINT16_1, 6, NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC,
        NiDataStream::USAGE_VERTEX_INDEX, ausTriList);

    spMesh->AddStream(NiCommonSemantics::NORMAL(), 0, 
        NiDataStreamElement::F_FLOAT32_3, 4, NiDataStream::ACCESS_GPU_READ |
        NiDataStream::ACCESS_CPU_WRITE_STATIC, NiDataStream::USAGE_VERTEX,
        akNormal);

    spMesh->UpdateProperties();

    NiBound kBound;
    kBound.SetCenter(NiPoint3::ZERO);
    kBound.SetRadius(fScale);
    spMesh->SetModelBound(kBound);

    return spMesh;
}
//---------------------------------------------------------------------------
NiPixelDataPtr NiMaterialDetector::GetPixelsFromTexture()
{
    D3DLOCKED_RECT kLockRect;
    RECT kCheckRect;

    kCheckRect.left = 0;
    kCheckRect.top = 0;
    kCheckRect.right = NIMD_RENDER_TEXTURE_SIZE;
    kCheckRect.bottom = NIMD_RENDER_TEXTURE_SIZE;

    // Get the DirectX Divice
    NiDX9Renderer *pDX9Renderer = NiDynamicCast(NiDX9Renderer, 
        NiRenderer::GetRenderer());
	NIASSERT(pDX9Renderer);
    LPDIRECT3DDEVICE9 pkDevice = pDX9Renderer->GetD3DDevice();
    NIASSERT(pkDevice);

    // Get the Direct3D Texture
    NiDX9RenderedTextureData* pkRenderData = 
        (NiDX9RenderedTextureData*)m_spTexture->GetRendererData();
    LPDIRECT3DTEXTURE9 pkD3DTexture = 
        (LPDIRECT3DTEXTURE9)(pkRenderData->GetD3DTexture());

    // We have to copy the rendered texture to the 'read' texture
    D3DSurfacePtr pkSrc, pkDst;

    pkD3DTexture->GetSurfaceLevel(0, &pkSrc);
    m_pkD3DReadTexture->GetSurfaceLevel(0, &pkDst);
    pkDevice->GetRenderTargetData(pkSrc, pkDst);

    pkSrc->Release();
    pkDst->Release();

    // Now, check the color
    unsigned int uiRed = 0;
    unsigned int uiGreen = 0;
    unsigned int uiBlue = 0;

    NiPixelDataPtr spData = NiNew NiPixelData(
        NIMD_RENDER_TEXTURE_SIZE, NIMD_RENDER_TEXTURE_SIZE, NiPixelFormat::RGBA32);
    unsigned char* pcData = spData->GetPixels();

    if (SUCCEEDED(m_pkD3DReadTexture->LockRect(0, &kLockRect, &kCheckRect, 
        D3DLOCK_READONLY)))
    {
        // Add up all the color values in the texture
        unsigned char* ucImageData = (unsigned char*)kLockRect.pBits;
        for (int i = 0; i < NIMD_RENDER_TEXTURE_SIZE*NIMD_RENDER_TEXTURE_SIZE*4; i+=4)
        {
            pcData[i] = ucImageData[i+2];
            pcData[i+1] = ucImageData[i+1];
            pcData[i+2] = ucImageData[i];
            pcData[i+3] = ucImageData[i+3];
        }

        m_pkD3DReadTexture->UnlockRect(0);
    }

    return spData;
}
//---------------------------------------------------------------------------
//void NiMaterialDetector::SaveTexture(NiPixelData* pkPixelData, char* pcFilename)
//{
//    NiFile kFile(pcFilename, NiFile::WRITE_ONLY);
//    NiTGAWriter::Dump(pkPixelData, kFile);
//}
//---------------------------------------------------------------------------
NiPixelDataPtr NiMaterialDetector::GetScenePixels(NiColor kClearColor)
{
    NiRenderer* pkRenderer = NiRenderer::GetRenderer();

    pkRenderer->BeginOffScreenFrame();
    
    pkRenderer->SetBackgroundColor(kClearColor);
    pkRenderer->BeginUsingRenderTargetGroup(m_spRenderTargetGroup,
        NiRenderer::CLEAR_ALL);
    pkRenderer->SetCameraData(m_spOrthoCamera);
    m_spSquarePolygon->RenderImmediate(pkRenderer);
    pkRenderer->EndUsingRenderTargetGroup();

    NiPixelDataPtr spData = GetPixelsFromTexture();

    pkRenderer->EndOffScreenFrame();

    return spData;
}
//---------------------------------------------------------------------------
NiMaterialPointValuesPtr NiMaterialDetector::CreatePointMaterialTextures(NiMesh* pkMesh)
{
    // Create material textures
    NiMaterialPointValuesPtr spColorValues = NiNew NiMaterialPointValues;

    pkMesh->UpdateProperties();
    // Apply the Material and Properties of the Mesh to our Square Polygon
    NiPropertyStatePtr pkPropertyState = pkMesh->GetPropertyState();
    m_spSquarePolygon->SetPropertyState(pkPropertyState);
    NiMaterial* pkMaterial = (NiMaterial*)pkMesh->GetActiveMaterial();
    NiStandardMaterial* pkStandardMaterial = NiDynamicCast(NiStandardMaterial, pkMaterial);
    if (pkStandardMaterial)
        pkStandardMaterial->SetForcePerPixelLighting(true);
    m_spSquarePolygon->ApplyAndSetActiveMaterial(pkMaterial);

    // Get Diffuse Emittance Color
    spColorValues->m_spEmittanceTexture = GetScenePixels(NiColor(0,0,0));

    // Get Transmitted Color
    spColorValues->m_spTransmittedTexture = GetScenePixels(NiColor(1,1,1));

    // Get Diffuse Color
    m_spDirLight->SetDiffuseColor(NiColor(1.0f, 1.0f, 1.0f));
    m_spDirLight->SetSpecularColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spDirLight->AttachAffectedNode(m_spNode);
    m_spNode->UpdateEffects();

    spColorValues->m_spDiffuseTexture = GetScenePixels(NiColor(0,0,0));

    // Get Specular Color
    m_spDirLight->SetDiffuseColor(NiColor(0.0f, 0.0f, 0.0f));
    m_spDirLight->SetSpecularColor(NiColor(1.0f, 1.0f, 1.0f));
    m_spNode->UpdateEffects();
    
    spColorValues->m_spSpecularTexture = GetScenePixels(NiColor(0,0,0));

    // Clear lighting
    m_spDirLight->DetachAffectedNode(m_spNode);
    m_spNode->UpdateEffects();

    return spColorValues;
}
//---------------------------------------------------------------------------
void NiMaterialDetector::Shutdown()
{
    ms_spInstance = NULL;
}
//---------------------------------------------------------------------------

#endif // SUPPORT_GAMEBRYO
