#include "shaderShared.hlsli"
cbuffer SceneConstantBuffer : register(b0) // b0 is the "virtual register" where the constant buffer is stored
{
    float4x4 viewProject;
};

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);

struct PSInput_UI
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput_UI VSMain(uint vid : SV_VertexID)
{
    StructuredBuffer<UIVertexPosition> vbuffer = ResourceDescriptorHeap[drawConstants.vpos_idx];
    float4 rawPos = float4(vbuffer[vid].position, 1);
    float4 worldPos = mul(rawPos, drawConstants.modelMatrix);
    float4 clipPos = mul(worldPos, drawConstants.viewProject);

    PSInput_UI result;
    result.pos = clipPos;
    result.uv = vbuffer[vid].uv;
    return result;
}
