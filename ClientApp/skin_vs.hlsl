#include "shaderShared.hlsli"

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);


struct BoneIndices 
{
    uint4 indices;
};
struct BoneWeights 
{
    float4 weights;
};
struct BoneTransform
{
    float4x4 mat;
};

PSInput VSMain(uint vid : SV_VertexID)
{
    StructuredBuffer<BoneIndices> boneIdxBuffer = ResourceDescriptorHeap[drawConstants.vbone_idx];
     uint4 boneIndices = boneIdxBuffer[vid].indices;
     uint4 frameBoneIndices = drawConstants.frame_number * drawConstants.num_bones + boneIndices;
    
    
    StructuredBuffer<BoneWeights> boneWeightBuffer = ResourceDescriptorHeap[drawConstants.vweight_idx];
    float4 boneWeights = boneWeightBuffer[vid].weights;
    
    StructuredBuffer<BoneTransform> boneTransformBuffer = ResourceDescriptorHeap[drawConstants.bone_transforms_idx];
   
    StructuredBuffer<VertexPosition> vbuffer = ResourceDescriptorHeap[drawConstants.vpos_idx];
    float4 position_homogeneous = float4(vbuffer[vid].position, 1);
    
    float4 skinned =
          boneWeights.x * mul(position_homogeneous, boneTransformBuffer[frameBoneIndices.x].mat)
        + boneWeights.y * mul(position_homogeneous, boneTransformBuffer[frameBoneIndices.y].mat)
        + boneWeights.z * mul(position_homogeneous, boneTransformBuffer[frameBoneIndices.z].mat)
        + boneWeights.w * mul(position_homogeneous, boneTransformBuffer[frameBoneIndices.w].mat);
    
    
    StructuredBuffer<VertexShadingData> shadebuffer = ResourceDescriptorHeap[drawConstants.vshade_idx];
    float4 normal = float4(shadebuffer[vid].normal, 0);
    float2 texcoord = float2(shadebuffer[vid].texcoord);
    
    PSInput result;
    result.positionGlobal = mul(position_homogeneous , drawConstants.modelMatrix);
    result.normal = normalize(mul(normal, drawConstants.modelInverseTranspose).xyz);
    // result.normal         = shadebuffer[vid].normal;
    result.positionNDC    = mul(result.positionGlobal, drawConstants.viewProject);
    result.texcoord = texcoord;
    result.triangle_id = vid / 3;

    return result;
    
}
