#include "shaderShared.hlsli"

SamplerState g_sampler : register(s0);

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);

float3 agxDefaultContrastApprox(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
  
    return +15.5 * x4 * x2
         - 40.14 * x4 * x
         + 31.96 * x4
         - 6.868 * x2 * x
         + 0.4298 * x2
         + 0.1191 * x
         - 0.00232;
}



float3 agx(float3 val)
{
    const float3x3 agx_mat = float3x3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    
    const float min_ev = -12.47393f;
    const float max_ev = 4.026069f;

    // Input transform
    val = mul(val, agx_mat);
  
    // Log2 space encoding
    val = clamp(log2(val), min_ev, max_ev);
    val = (val - min_ev) / (max_ev - min_ev);
  
    // Apply sigmoid function approximation
    val = agxDefaultContrastApprox(val);

    return val;
}
float3 agxEotf(float3 val) {
    const float3x3 agx_mat_inv = float3x3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    
  // Undo input transform
    val = mul(val, agx_mat_inv);
  
  // sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
  // val = pow(val, float3(2.2, 2.2, 2.2));

  return val;
}

#define AGX_LOOK 0
float3 agxLook(float3 val)
{
  // Default
    float3 offset = float3(0.0, 0.0, 0.0);
    float3 slope = float3(1.0, 1.0, 1.0);
    float3 power = float3(1.0, 1.0, 1.0);
    float sat = 1.0;
 
#if AGX_LOOK == 1
  // Golden
  slope = vec3(1.0, 0.9, 0.5);
  power = vec3(0.8);
  sat = 0.8;
#elif AGX_LOOK == 2
  // Punchy
  slope = vec3(1.0);
  power = vec3(1.35, 1.35, 1.35);
  sat = 1.4;
#endif
  
  // ASC CDL
    val = pow(val * slope + offset, power);
  
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float luma = dot(val, lw);
  
    return luma + sat * (val - luma);
}

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
    const float3 lightpos = float3(-2, -2, 2);
    // const float3 diffuseColor = float3(0.7, 0.1, 0.1);
    
    float3 toLight = lightpos - input.positionGlobal.xyz;
    float3 lightDir = normalize(toLight);

    float diffuseStrength = clamp(dot(lightDir, normalize(input.normal)), 0.3, 1);
    
    Texture2D lightmap = ResourceDescriptorHeap[drawConstants.lightmap_texture_idx];
    float3 lightmapColor = lightmap.Sample(g_sampler, input.lightmap_texcoord).rgb;
    // return float4(lightmapColor, 1);
    float3 col = lightmapColor * diffuseColor;
    
    col = agx(col);
    col = agxLook(col);
    col = agxEotf(col);
    
    
    return float4(col, 1);
    
    // return float4(input.normal, 1);
}
