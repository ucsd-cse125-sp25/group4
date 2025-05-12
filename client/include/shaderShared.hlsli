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

using float4   = XMFLOAT4;
using float4x4 = XMFLOAT4X4;
using uint     = uint32_t;

#else
#define SEMANTIC(sem) : sem
#endif

struct PSInput
{
    float4 position SEMANTIC(SV_POSITION);
};
struct PerDrawConstants 
{
    float4x4 modelViewProject;
    uint vbuffer_idx;
};
struct VertexPosition
{
	float3 position;
};