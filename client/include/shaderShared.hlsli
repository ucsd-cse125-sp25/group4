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
    float2 texcoord       SEMANTIC(TEXCOORD0);
    uint triangle_id      SEMANTIC(TEXCOORD1);
};
struct PerDrawConstants 
{
    matrix   viewProject;           
	matrix   modelMatrix;           // positions model to global
	matrix   modelInverseTranspose; // normals model to global
    uint     vpos_idx;              // vertex positions in model space
    uint     vshade_idx;            // normals and texcoords
    uint     material_ids_idx;
    uint     materials_idx;
    uint     first_texture_idx;
	// 51 DWORDS
};
struct VertexPosition
{
	float3 position;
};
struct VertexShadingData {
	float3 normal;
	float2 texcoord;
};

struct Material
{
	// if the material parameter is texture-determined, the leading bit is 0
	// the parameter points to the texture index in the texture array
	// the texture array is a contiguous set of descriptors offset in the descriptor heap

	// if the material parameter is not texture determined, it has a leading bit of 1
    int base_color; // T1R10G11B10
    int metallic; // negated float
    int roughness; // negated float
    int normal; // tagged as default
};
