// --------------------------------------------------------------------------
// Building unwrap.
// Copyright (c) 2010-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef _WIN32

// with settings below, Lightsprint SDK builds and runs without DirectX SDK or DirectX runtime, we only attempt to locate DirectX runtime at runtime
// we can even unwrap without DirectX runtime installed, it is sufficient to copy d3dx9_43.dll files to bin/win32 and bin/x64 dirs
// (they are not included in SDK to simplify testing, nearly everyone has DirectX runtime installed, better not add redundant dll)
#define DYNAMIC_LOAD // tries to load d3d[x]9.dll only when building unwrap, fails gracefully
//#define ERROR_STRINGS // makes code depend on dx lib, don't use

#include <cstdio>
#include <string>
#include <vector>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "Windows/d3dx9/D3DX9Mesh.h" // here we include our copy of Direct3DX9 headers, so that installing legacy packages (DirectX SDK) is not necessary
#include "Lightsprint/RRObject.h"

#ifndef DYNAMIC_LOAD
	#pragma comment(lib,"D3d9.lib")
	#pragma comment(lib,"D3dx9.lib")
#endif

#ifdef ERROR_STRINGS
	#pragma comment(lib,"DXErr9.lib")
	#include "DXErr9.h"
#else
	const char* DXGetErrorDescription9(HRESULT hr)
	{
		static char buf[16];
		sprintf(buf,"%x",(int)hr);
		return buf;
	}
#endif

namespace rr
{

typedef boost::unordered_set<unsigned> UvChannels;

class Unwrapper
{
public:
	Unwrapper();
	bool buildUnwrap(RRMeshArrays* rrMesh, unsigned unwrapChannel, const UvChannels& keepChannels, unsigned mapSize, float gutter, float pixelsPerWorldUnit, bool& aborting);
	~Unwrapper();

	// stats updated by buildUnwrap()
	unsigned sumMeshesFailed;
	unsigned sumMeshesUnwrapped;
	unsigned sumCharts; // in sumMeshesUnwrapped
	unsigned sumVerticesOld; // in sumMeshesUnwrapped
	unsigned sumVerticesNew; // in sumMeshesUnwrapped
	float getVertexOverhead() const {return float(sumVerticesNew-sumVerticesOld)/sumVerticesOld;}

private:
	IDirect3DVertexBuffer9* createVBuffer(void* data, unsigned numBytes) const;
	ID3DXMesh* createID3DXMesh(const RRMeshArrays* mesh, const UvChannels& keepChannels) const;
	void copyToRRMesh(RRMeshArrays* rrMesh, ID3DXMesh* dxMesh, const UvChannels& keepChannels, unsigned unwrapChannel) const;
	static HRESULT CALLBACK callback(FLOAT percentDone,LPVOID context)
	{
		return *(bool*)context ? S_FALSE : S_OK;
	}

#ifdef DYNAMIC_LOAD
	typedef IDirect3D9* (WINAPI * LPDIRECT3DCREATE9)      (UINT SDKVersion);
	typedef HRESULT     (WINAPI * LPD3DXCREATEMESH)       (DWORD NumFaces, DWORD NumVertices, DWORD Options, CONST D3DVERTEXELEMENT9 *pDeclaration, LPDIRECT3DDEVICE9 pD3DDevice, LPD3DXMESH* ppMesh);
	typedef HRESULT     (WINAPI * LPD3DXCLEANMESH)        (D3DXCLEANTYPE CleanType, LPD3DXMESH pMeshIn, CONST DWORD* pAdjacencyIn, LPD3DXMESH* ppMeshOut, DWORD* pAdjacencyOut, LPD3DXBUFFER* ppErrorsAndWarnings);
	typedef HRESULT     (WINAPI * LPD3DXVALIDMESH)        (LPD3DXMESH pMeshIn, CONST DWORD* pAdjacency, LPD3DXBUFFER* ppErrorsAndWarnings);
	typedef HRESULT     (WINAPI * LPD3DXUVATLASCREATE)    (LPD3DXMESH pMesh, UINT uMaxChartNumber, FLOAT fMaxStretch, UINT uWidth, UINT uHeight, FLOAT fGutter, DWORD dwTextureIndex, CONST DWORD *pdwAdjacency, CONST DWORD *pdwFalseEdgeAdjacency, CONST FLOAT *pfIMTArray, LPD3DXUVATLASCB pStatusCallback, FLOAT fCallbackFrequency, LPVOID pUserContext, DWORD dwOptions, LPD3DXMESH *ppMeshOut, LPD3DXBUFFER *ppFacePartitioning, LPD3DXBUFFER *ppVertexRemapArray, FLOAT *pfMaxStretchOut, UINT *puNumChartsOut);
	typedef HRESULT     (WINAPI * LPD3DXUVATLASPARTITION) (LPD3DXMESH pMesh, UINT uMaxChartNumber, FLOAT fMaxStretch, DWORD dwTextureIndex, CONST DWORD *pdwAdjacency, CONST DWORD *pdwFalseEdgeAdjacency, CONST FLOAT *pfIMTArray, LPD3DXUVATLASCB pStatusCallback, FLOAT fCallbackFrequency, LPVOID pUserContext, DWORD dwOptions, LPD3DXMESH *ppMeshOut, LPD3DXBUFFER *ppFacePartitioning, LPD3DXBUFFER *ppVertexRemapArray, LPD3DXBUFFER *ppPartitionResultAdjacency, FLOAT *pfMaxStretchOut, UINT *puNumChartsOut);
	typedef HRESULT     (WINAPI * LPD3DXUVATLASPACK)      (ID3DXMesh *pMesh, UINT uWidth, UINT uHeight, FLOAT fGutter, DWORD dwTextureIndex, CONST DWORD *pdwPartitionResultAdjacency, LPD3DXUVATLASCB pStatusCallback, FLOAT fCallbackFrequency, LPVOID pUserContext, DWORD dwOptions, LPD3DXBUFFER pFacePartitioning);

	LPDIRECT3DCREATE9      Direct3DCreate9;
	LPD3DXCREATEMESH       D3DXCreateMesh;
	LPD3DXCLEANMESH        D3DXCleanMesh;
	LPD3DXVALIDMESH        D3DXValidMesh;
	LPD3DXUVATLASCREATE    D3DXUVAtlasCreate;
	LPD3DXUVATLASPARTITION D3DXUVAtlasPartition;
	LPD3DXUVATLASPACK      D3DXUVAtlasPack;
#endif

	HWND                hwnd;
	IDirect3D9*         d3d;
	IDirect3DDevice9*   d3dDevice;
};

Unwrapper::Unwrapper()
{
	// clear all
	sumMeshesFailed = 0;
	sumMeshesUnwrapped = 0;
	sumCharts = 0;
	sumVerticesOld = 0;
	sumVerticesNew = 0;
	hwnd = NULL;
	d3d = NULL;
	d3dDevice = NULL;

	// create window to make d3d happy
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = DefWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = NULL;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "a";
	RegisterClass(&wc);
	RECT rect = {0, 0, 100, 100};
	DWORD dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX;
	AdjustWindowRect(&rect, dwStyle, false);
	hwnd = CreateWindow("a", "a", dwStyle, 0, 0, rect.right-rect.left, rect.bottom-rect.top, NULL, NULL, NULL, NULL);

	// create d3d to make d3ddevice happy
#ifdef DYNAMIC_LOAD
	static HMODULE s_hModD3D9 = LoadLibrary("d3d9.dll");
	static HMODULE s_hModD3DX9 = NULL; //LoadLibrary("d3dx9_32.dll");
	static bool inited = false;
	if (!inited)
	{
		inited = true;
		for (unsigned version=43;!s_hModD3DX9 && version>=31;version--)
		{
			char d3dxFilename[] = "d3dx9_??.dll";
			d3dxFilename[6] = '0'+version/10;
			d3dxFilename[7] = '0'+version%10;
			s_hModD3DX9 = LoadLibrary(d3dxFilename);
		}
	}

	Direct3DCreate9      = s_hModD3D9  ? (LPDIRECT3DCREATE9)     GetProcAddress(s_hModD3D9,  "Direct3DCreate9")      : NULL;
	D3DXCreateMesh       = s_hModD3DX9 ? (LPD3DXCREATEMESH)      GetProcAddress(s_hModD3DX9, "D3DXCreateMesh")       : NULL;
	D3DXCleanMesh        = s_hModD3DX9 ? (LPD3DXCLEANMESH)       GetProcAddress(s_hModD3DX9, "D3DXCleanMesh")        : NULL;
	D3DXValidMesh        = s_hModD3DX9 ? (LPD3DXVALIDMESH)       GetProcAddress(s_hModD3DX9, "D3DXValidMesh")        : NULL;
	D3DXUVAtlasCreate    = s_hModD3DX9 ? (LPD3DXUVATLASCREATE)   GetProcAddress(s_hModD3DX9, "D3DXUVAtlasCreate")    : NULL;
	D3DXUVAtlasPartition = s_hModD3DX9 ? (LPD3DXUVATLASPARTITION)GetProcAddress(s_hModD3DX9, "D3DXUVAtlasPartition") : NULL;
	D3DXUVAtlasPack      = s_hModD3DX9 ? (LPD3DXUVATLASPACK)     GetProcAddress(s_hModD3DX9, "D3DXUVAtlasPack")      : NULL;

	d3d = Direct3DCreate9 ? Direct3DCreate9(D3D_SDK_VERSION) : NULL;
	if (!d3d || !D3DXCreateMesh || !D3DXUVAtlasPartition || !D3DXUVAtlasPack)
		RRReporter::report(WARN,"Unwrap not built, please install DirectX runtime%s.\n",
			(!s_hModD3D9)?" (d3d9.dll not found)":((!s_hModD3DX9)?" (d3dx9_nn.dll not found, 44>nn>30)":""));
#else
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
#endif

	// create d3ddevice to make d3dx happy
	if (d3d)
	{
		D3DPRESENT_PARAMETERS present;
		memset(&present,0,sizeof(present));
		present.BackBufferWidth = 1;
		present.BackBufferHeight = 1;
		present.BackBufferFormat = D3DFMT_UNKNOWN;
		present.BackBufferCount = 1;
		present.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		present.MultiSampleType = D3DMULTISAMPLE_NONE;
		present.SwapEffect = D3DSWAPEFFECT_DISCARD;
		present.hDeviceWindow = hwnd;
		present.Windowed = TRUE;
		present.EnableAutoDepthStencil = FALSE;
		d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_NULLREF,hwnd,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE|D3DCREATE_MULTITHREADED,&present,&d3dDevice);
	}

	// if we are lucky and everything initialized, we can finally use two d3dx function
}

IDirect3DVertexBuffer9* Unwrapper::createVBuffer(void* data, unsigned numBytes) const
{
	IDirect3DVertexBuffer9* vbuf = NULL;
	HRESULT hr = d3dDevice->CreateVertexBuffer(numBytes,0,0,D3DPOOL_SYSTEMMEM,&vbuf,NULL);
	void* lock = NULL;
	hr = vbuf->Lock(0,numBytes,&lock,D3DLOCK_DISCARD);
	memcpy(lock,data,numBytes);
	hr = vbuf->Unlock();
	return vbuf;
}

enum {MAX_CHANNELS=5};

struct Vertex
{
	RRVec3 position;
	RRVec3 normal;
	RRVec3 tangent;
	RRVec3 bitangent;
	RRVec2 texcoord[MAX_CHANNELS];
};

ID3DXMesh* Unwrapper::createID3DXMesh(const RRMeshArrays* rrMesh, const UvChannels& keepChannels) const
{
	if (!d3dDevice || !D3DXCreateMesh)
	{
		return NULL;
	}
	if (keepChannels.size()+1>MAX_CHANNELS)
	{
		RRReporter::report(WARN,"Unwrap not built, too many uv channels.\n");
		return NULL; // keepChannels must not contain too many channels
	}
	unsigned numTriangles = rrMesh->getNumTriangles();
	unsigned numVertices = rrMesh->getNumVertices();
	if (!numTriangles || !numVertices)
		return NULL; // fail now or D3DXCreateMesh fails few lines below
	// allocate mesh
	Vertex* vertexData = NULL;
	D3DVERTEXELEMENT9 dxMeshDeclaration[MAX_FVF_DECL_SIZE] = {
		{0,(char*)&vertexData->position   -(char*)vertexData,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,(char*)&vertexData->normal     -(char*)vertexData,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},
		{0,(char*)&vertexData->tangent    -(char*)vertexData,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TANGENT,0},
		{0,(char*)&vertexData->bitangent  -(char*)vertexData,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_BINORMAL,0},
		{0,(char*)&vertexData->texcoord[0]-(char*)vertexData,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0,(char*)&vertexData->texcoord[1]-(char*)vertexData,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,(char*)&vertexData->texcoord[2]-(char*)vertexData,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		{0,(char*)&vertexData->texcoord[3]-(char*)vertexData,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,3},
		{0,(char*)&vertexData->texcoord[4]-(char*)vertexData,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,4},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}//D3DDECL_END
	};
	ID3DXMesh* dxMesh = NULL;
	HRESULT hr = D3DXCreateMesh(numTriangles,numVertices,D3DXMESH_32BIT|D3DXMESH_SYSTEMMEM,dxMeshDeclaration,d3dDevice,&dxMesh);
	// copy indices
	RRMesh::Triangle* indexData = NULL;
	hr = dxMesh->LockIndexBuffer(0/*D3DLOCK_DISCARD*/,(LPVOID*)&indexData);
	memcpy(indexData,rrMesh->triangle,sizeof(RRMesh::Triangle)*numTriangles);
	hr = dxMesh->UnlockIndexBuffer();
	// copy vertices
	hr = dxMesh->LockVertexBuffer(0/*D3DLOCK_DISCARD*/,(LPVOID*)&vertexData);
	memset(vertexData,0,sizeof(Vertex)*numVertices);
	for (unsigned v=0;v<numVertices;v++)
	{
		vertexData[v].position = rrMesh->position[v];
		vertexData[v].normal = rrMesh->normal[v];
		if (rrMesh->tangent)
		{
			vertexData[v].tangent = rrMesh->tangent[v];
			vertexData[v].bitangent = rrMesh->bitangent[v];
		}
		unsigned numUvs = 0;
		for (UvChannels::const_iterator i=keepChannels.begin();i!=keepChannels.end();++i)
		{
			RR_ASSERT(rrMesh->texcoord[*i]); // keepChannels must contain only existing channels
			vertexData[v].texcoord[numUvs++] = rrMesh->texcoord[*i][v];
		}
	}
	hr = dxMesh->UnlockVertexBuffer();
	return dxMesh;
}

void Unwrapper::copyToRRMesh(RRMeshArrays* rrMesh, ID3DXMesh* dxMesh, const UvChannels& keepChannels, unsigned unwrapChannel) const
{
	unsigned numTriangles = dxMesh->GetNumFaces();
	unsigned numVertices = dxMesh->GetNumVertices();
	RRVector<unsigned> texcoords;
	texcoords.push_back(unwrapChannel);
	for (UvChannels::const_iterator i=keepChannels.begin();i!=keepChannels.end();++i)
		texcoords.push_back(*i);
	rrMesh->resizeMesh(numTriangles,numVertices,&texcoords,rrMesh->tangent?true:false);
	// copy indices
	DWORD* indexData = NULL;
	HRESULT hr = dxMesh->LockIndexBuffer(D3DLOCK_READONLY,(LPVOID*)&indexData);
	memcpy(rrMesh->triangle,indexData,sizeof(RRMesh::Triangle)*numTriangles);
	dxMesh->UnlockIndexBuffer();
	// copy vertices
	Vertex* vertexData = NULL;
	hr = dxMesh->LockVertexBuffer(0,(LPVOID*)&vertexData);
	for (unsigned v=0;v<numVertices;v++)
	{
		rrMesh->position[v] = vertexData[v].position;
		rrMesh->normal[v] = vertexData[v].normal;
		if (rrMesh->tangent)
		{
			rrMesh->tangent[v] = vertexData[v].tangent;
			rrMesh->bitangent[v] = vertexData[v].bitangent;
		}
		unsigned numUvs = 0;
		for (UvChannels::const_iterator i=keepChannels.begin();i!=keepChannels.end();++i)
		{
			rrMesh->texcoord[*i][v] = vertexData[v].texcoord[numUvs++];
		}
		rrMesh->texcoord[unwrapChannel][v] = vertexData[v].texcoord[MAX_CHANNELS-1];
	}
	hr = dxMesh->UnlockVertexBuffer();
}

bool Unwrapper::buildUnwrap(RRMeshArrays* rrMesh, unsigned unwrapChannel, const UvChannels& keepChannels, unsigned mapSize, float gutter, float pixelsPerWorldUnit, bool& aborting)
{
	bool result = false;

	// keepChannels must contain only existing channels
	bool wrongInputs = false;
	for (UvChannels::const_iterator i=keepChannels.begin();i!=keepChannels.end();++i)
	{
		if (!rrMesh || *i>=rrMesh->texcoord.size() || !rrMesh->texcoord[*i])
		{
			RR_ASSERT(0);
			wrongInputs = true;
		}
	}

	if (!wrongInputs && D3DXUVAtlasCreate)
	{
		unsigned numTriangles = rrMesh->getNumTriangles();
		unsigned numVertices = rrMesh->getNumVertices();
		// technically, unwrap creation has failed, uv channel was not added, but let's not warn about it
		//  because it would confuse users, main problem is not in unwrapper but in caller who works with empty mesh
		// no, keep warning about it, we would like to get rid of empty meshes
		//if (!numTriangles || !numVertices)
		//	return true;
		ID3DXMesh* dxMeshIn = createID3DXMesh(rrMesh,keepChannels);
		if (dxMeshIn)
		{
			DWORD* adjacency = new DWORD[3*numTriangles];
			if (adjacency)
			{
				if (dxMeshIn->GenerateAdjacency(0.0f,adjacency)==D3D_OK)
				{
					// generated adjacency would make all edges smooth,
					// let's break adjacency where we want hard edge
					const RRMesh* rrMeshStitched = rrMesh->createOptimizedVertices(0.001f,RR_DEG2RAD(3),0,NULL);
					for (unsigned t=0;t<numTriangles;t++)
					{
						RRMesh::Triangle triangleIndices;
						rrMeshStitched->getTriangle(t,triangleIndices);
						for (unsigned s=0;s<3;s++)
						{
							unsigned neighbor = adjacency[3*t+s];
							if (neighbor<numTriangles)
							{
								RRMesh::Triangle neighborIndices;
								rrMeshStitched->getTriangle(neighbor,neighborIndices);
								unsigned numSharedVertices =
									((triangleIndices[0]==neighborIndices[0] || triangleIndices[0]==neighborIndices[1] || triangleIndices[0]==neighborIndices[2])?1:0) +
									((triangleIndices[1]==neighborIndices[0] || triangleIndices[1]==neighborIndices[1] || triangleIndices[1]==neighborIndices[2])?1:0) +
									((triangleIndices[2]==neighborIndices[0] || triangleIndices[2]==neighborIndices[1] || triangleIndices[2]==neighborIndices[2])?1:0);
								if (numSharedVertices<2)
									adjacency[3*t+s] = -1;
							}
						}
					}
					if (rrMeshStitched!=rrMesh)
						delete rrMeshStitched;

					// fix bowties, unwrapper may fail because of them
					if (D3DXCleanMesh)
					{
						ID3DXMesh* dxMeshOut = NULL;
						DWORD* adjacencyOut = new DWORD[3*numTriangles];
						ID3DXBuffer* errorsAndWarnings = NULL;
						HRESULT err = D3DXCleanMesh(D3DXCLEAN_SIMPLIFICATION,dxMeshIn,adjacency,&dxMeshOut,adjacencyOut,&errorsAndWarnings);
						//RRReporter::report(WARN,"cleanup=%s\n",DXGetErrorDescription9(err));
						if (err==D3D_OK)
						{
							if (dxMeshOut!=dxMeshIn)
							{
								dxMeshIn->Release();
								dxMeshIn = dxMeshOut;
								dxMeshOut = NULL;
							}
							if (adjacencyOut!=adjacency)
							{
								delete[] adjacency;
								adjacency = adjacencyOut;
								adjacencyOut = NULL;
							}
							if (errorsAndWarnings)
							{
								RRReporter::report(WARN,"%s\n",errorsAndWarnings->GetBufferPointer());
								errorsAndWarnings->Release();
							}
						}
					}

					// report remaining errors
					if (D3DXValidMesh)
					{
						ID3DXBuffer* errorsAndWarnings2 = NULL;
						D3DXValidMesh(dxMeshIn,adjacency,&errorsAndWarnings2);
						if (errorsAndWarnings2)
						{
							RRReporter::report(WARN,"%s\n",errorsAndWarnings2->GetBufferPointer());
							errorsAndWarnings2->Release();
						}
					}

#if 0
					ID3DXMesh* dxMeshOut = NULL;
					FLOAT maxStretch = 0;
					UINT numCharts = 0;
					HRESULT err = D3DXUVAtlasCreate(
						dxMeshIn,
						0, // maxCharts
						0.8f, // maxStretch
						mapSize,
						mapSize,
						gutter,
						MAX_CHANNELS-1, // textureIndex
						adjacency,
						NULL, // falseEdges,
						NULL, // metric tensor array
						&callback,0.0001f,&aborting,
						D3DXUVATLAS_DEFAULT,
						&dxMeshOut,
						NULL, // face partitioning
						NULL, // vertex remap array
						&maxStretch, // max stretch
						&numCharts // num charts
						);
					if (err==D3D_OK)
					{
						// update stats
						sumMeshesUnwrapped++;
						sumCharts += numCharts;
						sumVerticesOld += numVertices;
						sumVerticesNew += dxMeshOut->GetNumVertices();

						copyToRRMesh(rrMesh,dxMeshOut,keepChannels,unwrapChannel);
						result = true;
					}
					else
					{
						RRReporter::report(WARN,"Build failed for mesh %x: %s.\n",(int)(size_t)rrMesh,DXGetErrorDescription9(err));
					}
#else
					ID3DXMesh* dxMeshOut = NULL;
					FLOAT maxStretch = 0;
					UINT numCharts = 0;
					LPD3DXBUFFER partitionResultAdjacency = NULL;
					LPD3DXBUFFER facePartitioning = NULL;
					HRESULT err = 19191919;
#if _MSC_VER>=1500 // this __try fails to compile in VS2005
					__try
					{
#endif
						err = D3DXUVAtlasPartition(
							dxMeshIn,
							0, // maxCharts
							0.8f, // maxStretch
							MAX_CHANNELS-1, // textureIndex
							adjacency,
							NULL, // falseEdges,
							NULL, // metric tensor array
							&callback,0.0001f,&aborting,
							D3DXUVATLAS_DEFAULT,
							&dxMeshOut,
							&facePartitioning,
							NULL, // vertex remap array
							&partitionResultAdjacency,
							&maxStretch, // max stretch
							&numCharts // num charts
							);
#if _MSC_VER>=1500
					}
					__except(EXCEPTION_EXECUTE_HANDLER)
					{
					}
#endif
					if (err==19191919)
					{
						RRReporter::report(ERRO,"D3DXUVAtlasPartition() crashed, better save your work and restart.\n");
					}
					else
					if (err==D3D_OK)
					{
						//unsigned minimalMapSize = (unsigned)(gutter*sqrtf(numCharts)); // probably never succeeds
						unsigned minimalMapSize = 1;
						while (minimalMapSize*2.0f<gutter*sqrtf((float)numCharts)) minimalMapSize *= 2;
						unsigned trySize = RR_MAX(mapSize,minimalMapSize);
						while (1)
						{
							err = D3DXUVAtlasPack(
								dxMeshOut,
								trySize,
								trySize,
								gutter,
								MAX_CHANNELS-1, // textureIndex
								(DWORD*)partitionResultAdjacency->GetBufferPointer(),
								&callback,0.0001f,&aborting,
								0,
								facePartitioning
								);
							if (err==D3D_OK)
							{
								// update stats
								sumMeshesUnwrapped++;
								sumCharts += numCharts;
								sumVerticesOld += numVertices;
								sumVerticesNew += dxMeshOut->GetNumVertices();

								copyToRRMesh(rrMesh,dxMeshOut,keepChannels,unwrapChannel);
								result = true;
								if (trySize>mapSize)
									RRReporter::report(WARN,"Mesh %x needs resolution at least %d (%d charts).\n",(int)(size_t)rrMesh,trySize,numCharts);
								break;
							}
							else
							if (trySize>=8*1024)
							{
								RRReporter::report(WARN,"Packing failed (%s) for mesh %x, even resolution %d too low for %d charts?\n",DXGetErrorDescription9(err),(int)(size_t)rrMesh,trySize,numCharts);
								break;
							}
							else
							{
								trySize *= 2;
							}
						}
					}
					else
					{
						RRReporter::report(WARN,"Partitioning failed (%s) for mesh %x.\n",DXGetErrorDescription9(err),(int)(size_t)rrMesh);
					}
					RR_SAFE_RELEASE(facePartitioning);
					RR_SAFE_RELEASE(partitionResultAdjacency);
					RR_SAFE_RELEASE(dxMeshOut);
#endif
				}
				delete[] adjacency;
			}
			RR_SAFE_RELEASE(dxMeshIn);
		}
	}
	if (!result) sumMeshesFailed++;
	return result;
}

Unwrapper::~Unwrapper()
{
	if (d3dDevice)
		d3dDevice->Release();
	if (d3d)
		d3d->Release();
	DestroyWindow(hwnd);
}

unsigned RRObjects::buildUnwrap(unsigned resolution, bool& aborting) const
{
	RRReportInterval report(INF2,"Building unwrap...\n");

	// 1. gather unique meshes and used uv channels
	typedef boost::unordered_map<RRMeshArrays*,UvChannels> Meshes;
	Meshes meshes;
	const RRObjects& objects = *this;
	for (unsigned i=0;i<objects.size();i++)
	{
		const RRObject* object = objects[i];
		if (object)
		{
			const RRCollider* collider = object->getCollider();
			if (collider)
			{
				const RRMesh* mesh = collider->getMesh();
				if (mesh)
				{
					// editing mesh without rebuilding collider is safe because we won't touch triangles,
					// we will only add/remove uv channels and split vertices
					RRMeshArrays* arrays = dynamic_cast<RRMeshArrays*>(const_cast<RRMesh*>(mesh));
					if (arrays)
					{
						// inserts arrays. without this, meshes without texcoords would not be inserted and unwrap not built
						meshes[arrays].begin();

						for (unsigned channel=0;channel<arrays->texcoord.size();channel++)
						{
							if (arrays->texcoord[channel])
								meshes[arrays].insert(channel);
						}
					}
					else 
					{
						RRReporter::report(WARN,"Unwrap not built, scene contains non-RRMeshArrays meshes. This affects 3ds, gamebryo, mgf, try changing format.\n");
						return UINT_MAX;
					}
				}
			}
		}
	}

	// 2. find first free uv channel for unwrap
	unsigned unwrapChannel = 0;
try_next_channel:
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		if (i->second.find(unwrapChannel)!=i->second.end())
		{
			unwrapChannel++;
			goto try_next_channel;
		}
	}
	RRReporter::report(INF2,"%d meshes in %d objects, unwrapping to channel %d.\n",meshes.size(),size(),unwrapChannel);

	// 3. change all lightmapTexcoord to unwrapChannel
	for (unsigned i=0;i<objects.size();i++)
	{
		for (unsigned fg=0;fg<objects[i]->faceGroups.size();fg++)
		{
			RRMaterial* material = objects[i]->faceGroups[fg].material;
			if (material)
				material->lightmapTexcoord = unwrapChannel;
		}
	}

	// 4. generate unwraps
	Unwrapper unwrapper;
#if _MSC_VER<1400 // VS2003
	// serial
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		unwrapper.buildUnwrap(i->first,unwrapChannel,i->second,resolution,2.5f,1,aborting);
	}
#else // !VS2003
	// parallel
	struct MeshesIterator
	{
		Meshes::const_iterator iter;
		bool operator <(const MeshesIterator& a) const
		{
			// sort meshes by expected calculation time
			// 10% speedup in vc2010+arm_skeleton.obj
			return iter->first->numTriangles>a.iter->first->numTriangles;
		}
	};
	typedef std::vector<MeshesIterator> MeshesIterators;
	MeshesIterators meshesIterators;
	for (Meshes::const_iterator i=meshes.begin();i!=meshes.end();++i)
	{
		MeshesIterator iter;
		iter.iter = i;
		meshesIterators.push_back(iter);
	}
	std::sort(meshesIterators.begin(),meshesIterators.end());
	#pragma omp parallel for schedule(dynamic)
	for (int i=0;i<(int)meshesIterators.size();i++)
	{
		unwrapper.buildUnwrap(meshesIterators[i].iter->first,unwrapChannel,meshesIterators[i].iter->second,resolution,2.5f,1,aborting);
	}
#endif // !VS2003

	if (unwrapper.sumMeshesFailed)
		RRReporter::report(WARN,"Failed to unwrap %d of %d meshes.\n",unwrapper.sumMeshesFailed,meshes.size());
	if (unwrapper.sumMeshesUnwrapped)
		RRReporter::report(INF2,"%.1f charts/mesh, vertex count increased by %d%%.\n",unwrapper.sumCharts/(float)unwrapper.sumMeshesUnwrapped,(int)(100*unwrapper.getVertexOverhead()));
	return unwrapChannel;
}

} // namespace

#else // !_WIN32

#include "Lightsprint/RRObject.h"

namespace rr
{

unsigned RRObjects::buildUnwrap(unsigned resolution, bool& aborting) const
{
	RRReporter::report(WARN,"buildUnwrap() not supported on this platform.\n");
	return UINT_MAX;
}

} // namespace

#endif // !_WIN32
