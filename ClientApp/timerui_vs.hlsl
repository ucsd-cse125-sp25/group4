#include "shaderShared.hlsli"
cbuffer SceneConstantBuffer : register(b0) // b0 is the "virtual register" where the constant buffer is stored
{
    float4x4 viewProject;
};

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);

struct PSInput_UI
{
    float4 position : SV_POSITION;
};

PSInput_UI VSMain(uint vid : SV_VertexID)
{
    StructuredBuffer<VertexPosition> vbuffer = ResourceDescriptorHeap[drawConstants.vpos_idx];
    float4 position = float4(vbuffer[vid].position, 1);

    PSInput_UI result;
    result.position = position;
    return result;
}
