#include "shaderShared.hlsli"

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);



PSInput VSMain(uint vid : SV_VertexID)
{
     
    StructuredBuffer<VertexPosition> vbuffer = ResourceDescriptorHeap[drawConstants.vpos_idx];
    float4 position_homogeneous = float4(vbuffer[vid].position, 1);
    
    
    StructuredBuffer<VertexShadingData> shadebuffer = ResourceDescriptorHeap[drawConstants.vshade_idx];
    float2 texcoord = float2(shadebuffer[vid].texcoord);
    
    StructuredBuffer<VertexLightmapTexcoord> lightmapTexcoordBuffer = ResourceDescriptorHeap[drawConstants.lightmap_texcoord_idx];
    float2 lightmap_texcoord = lightmapTexcoordBuffer[vid].texcoord;
     
    PSInput result;
    result.positionGlobal = mul(position_homogeneous , drawConstants.modelMatrix);
    result.normal = shadebuffer[vid].normal;
    result.positionNDC    = mul(result.positionGlobal, drawConstants.viewProject);
    result.texcoord = texcoord;
    result.lightmap_texcoord = lightmap_texcoord;
    result.triangle_id = vid / 3;

    return result;
    
}
