#include "shaderShared.hlsli"


float4 PSMain(PSInput input, uint id : SV_PrimitiveID) : SV_TARGET
{
    if (length(input.normal) < 0.5)
    {
        return float4(0, 1, 0, 1);
    }
    const float3 lightpos = float3(-2, -2, 2);
    const float3 diffuseColor = float3(0.7, 0.1, 0.1);
    
    float3 toLight = lightpos - input.positionGlobal.xyz;
    float3 lightDir = normalize(toLight);

    float diffuseStrength = clamp(dot(lightDir, normalize(input.normal)), 0, 1);
    
    float3 color = diffuseStrength * diffuseColor;
    return float4(color, 1);
    
    // return float4(input.normal, 1);
}
