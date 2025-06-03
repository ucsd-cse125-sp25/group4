#include "shaderShared.hlsli"
SamplerState g_sampler : register(s0);
ConstantBuffer<PerDrawConstants> drawConstants : register(b1);

struct PSInput_UI
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 PSMain(PSInput_UI input) : SV_TARGET
{
    // return float4(1.0, 1.0, 1.0, 1.0);
    Texture2D<float4> tex = ResourceDescriptorHeap[drawConstants.first_texture_idx];
    return tex.Sample(g_sampler, input.uv);
}