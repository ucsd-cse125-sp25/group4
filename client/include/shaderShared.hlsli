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

// define flags here
#define FLAG_NOCTURNAL_RUNNER 0x00000001
#define FLAG_NOCTURNAL_HUNTER 0x00000002


const float PI = 3.14159265359;

struct PSInput
{
	// divided by w in the by the rasterizer between the vertex and fragment shaders
    float4 positionNDC    SEMANTIC(SV_POSITION);
	
	float4 positionGlobal SEMANTIC(POSITION);
    float3 normal         SEMANTIC(NORMAL0);
    float2 texcoord       SEMANTIC(TEXCOORD0);
    float2 lightmap_texcoord SEMANTIC(TEXCOORD1);
    uint   triangle_id      SEMANTIC(TEXCOORD2);
};
struct PerDrawConstants 
{
    matrix   viewProject;           
	matrix   modelMatrix;           // positions model to global
    // runner positions
    uint     vpos_idx;              // vertex positions in model space
    uint     vshade_idx;            // normals and texcoords
    uint     material_ids_idx;
    uint     materials_idx;
    uint     first_texture_idx;
    uint     lightmap_texcoord_idx;
    uint     lightmap_texture_idx;
    uint     cubemap_idx;
    uint     flags;
    // placed as individual floats because of packing rules
    float camx;
    float camy;
    float camz;
    float p1x;
    float p1y;
    float p1z;
    float p2x;
    float p2y;
    float p2z;
    float p3x;
    float p3y;
    float p3z;
	// 40 DWORDS
};
struct PlayerDrawConstants
{
    matrix   viewProject;           
	matrix   modelMatrix;           // positions model to global
	matrix   modelInverseTranspose; // normals model to global
    uint     vpos_idx;              // vertex positions in model space
    uint     vshade_idx;            // normals and texcoords
    uint     material_ids_idx;
    uint     materials_idx;
    uint     first_texture_idx;
    uint     vbone_idx;
    uint     vweight_idx;
    uint     bone_transforms_idx;
    uint     bone_adj_transforms_idx;
    uint     frame_number;
    uint     num_bones;
    uint     flags;
	// 57 DWORDS
};
struct VertexPosition
{
	float3 position;
};
struct UIVertexPosition
{
    float3 position;
    float2 uv;
};
struct VertexShadingData {
	float3 normal;
	float2 texcoord;
};
struct VertexLightmapTexcoord {
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
