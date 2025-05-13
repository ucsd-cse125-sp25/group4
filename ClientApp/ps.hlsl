#include "shaderShared.hlsli"
float4 PSMain(PSInput input, uint id : SV_PrimitiveID) : SV_TARGET
{
    float4 color = float4(input.normal, 1);
    return color;
}
