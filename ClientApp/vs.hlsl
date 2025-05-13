#include "shaderShared.hlsli"
cbuffer SceneConstantBuffer : register(b0) // b0 is the "virtual register" where the constant buffer is stored
{
    float4x4 viewProject;
};

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);


PSInput VSMain(uint vid : SV_VertexID)
{
    StructuredBuffer<VertexPosition> vbuffer = ResourceDescriptorHeap[drawConstants.vpos_idx];
    float4 position_homogeneous = float4(vbuffer[vid].position, 1);
    
    StructuredBuffer<VertexShadingData> shadebuffer = ResourceDescriptorHeap[drawConstants.vshade_idx];
    float3 normal = shadebuffer[vid].normal;
    
    PSInput result;
    result.position = mul(position_homogeneous, drawConstants.modelViewProject); // offset is visible outside of the struct
    result.normal = normal;

    return result;
    
}
