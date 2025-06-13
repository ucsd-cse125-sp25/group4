#include "shaderShared.hlsli"

SamplerState g_sampler : register(s0);

float4 PSMain(PSInput input, uint id : SV_PrimitiveID) : SV_TARGET
{
    return float4(1, 0, 0, 1);
}
