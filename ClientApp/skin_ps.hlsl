#include "shaderShared.hlsli"

SamplerState g_sampler : register(s0);

ConstantBuffer<PlayerDrawConstants> drawConstants : register(b1);
float4 PSMain(PSInput input, uint id : SV_PrimitiveID) : SV_TARGET
{
    // return float4(normalize(input.normal), 1);
    StructuredBuffer<min16uint> material_indices = ResourceDescriptorHeap[drawConstants.material_ids_idx];
    min16uint material_idx = material_indices[input.triangle_id];
    StructuredBuffer<Material> materials = ResourceDescriptorHeap[drawConstants.materials_idx];
    Material material = materials[material_idx];
    float3 diffuseColor;
    if (material.base_color < 0)
    {
        int base_color = material.base_color;
        int r_quantized = ((base_color & 0x7FE00000) >> 21);
        int g_quantized = ((base_color & 0x001FFC00) >> 10);
        int b_quantized = ((base_color & 0x000003FF) >> 00);
        float r = r_quantized / pow(2, 10);
        float g = g_quantized / pow(2, 11);
        float b = b_quantized / pow(2, 10);
        
        diffuseColor = float3(r, g, b);
    }
    else
    {
        Texture2D texture = ResourceDescriptorHeap[drawConstants.first_texture_idx + material.base_color];
        diffuseColor = texture.Sample(g_sampler, input.texcoord).rgb;
    }
    // const float3 lightpos = float3(-2, -2, 2);
    // const float3 diffuseColor = float3(0.7, 0.1, 0.1);
    
    
    float3 toLight = float3(0, 8.59, 3.20);
    float3 lightDir = normalize(toLight);

    float diffuseStrength = clamp(dot(lightDir, normalize(input.normal)), 0.3, 1);
    
    float3 color = diffuseStrength * diffuseColor;
    return float4(color, 1);
    
    // return float4(input.normal, 1);
}
