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
    float4 normal = float4(shadebuffer[vid].normal, 0);
    
    PSInput result;
    result.positionGlobal = mul(position_homogeneous , drawConstants.modelMatrix);
    result.normal = normalize(mul(normal, drawConstants.modelInverseTranspose).xyz);
    // result.normal         = shadebuffer[vid].normal;
    result.positionNDC    = mul(result.positionGlobal, drawConstants.viewProject);

    return result;
    
}
