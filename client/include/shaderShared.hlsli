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
    float4 position SEMANTIC(SV_POSITION);
    float3 normal   SEMANTIC(NORMAL0);
};
struct PerDrawConstants 
{
    matrix   modelViewProject;
    uint     vpos_idx;
    uint     vshade_idx;
};
struct VertexPosition
{
	float3 position;
};
struct VertexShadingData {
	float3 normal;
	float2 texcoord;
};