#pragma once
/*
Contains shared data types between C++ and HLSL.
Contains shared data types between multiple shader file.
*/


#ifdef __cplusplus

#define SEMANTIC(sem) 
#include <DirectXMath.h>
#include <stdint.h>
using namespace DirectX;

using float2   = XMFLOAT2;
using float3   = XMFLOAT3;
using float4   = XMFLOAT4;

using float4x4 = XMFLOAT4X4;

using vector   = XMVECTOR;
using matrix   = XMMATRIX;

using uint     = uint32_t;

#else
#define SEMANTIC(sem) : sem
#endif

struct PSInput
{
	// divided by w in the by the rasterizer between the vertex and fragment shaders
    float4 positionNDC    SEMANTIC(SV_POSITION);
	
	float4 positionGlobal SEMANTIC(POSITION);
    float3 normal         SEMANTIC(NORMAL0);
};
struct PerDrawConstants 
{
    matrix   viewProject;           
	matrix   modelMatrix;           // positions model to global
	matrix   modelInverseTranspose; // normals model to global
    uint     vpos_idx;              // vertex positions in model space
    uint     vshade_idx;            // normals and texcoords
	// 50 DWORDS
};
struct VertexPosition
{
	float3 position;
};
struct VertexShadingData {
	float3 normal;
	float2 texcoord;
};