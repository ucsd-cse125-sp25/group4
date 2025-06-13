cbuffer SceneConstantBuffer : register(b0) // b0 is the "virtual register" where the constant buffer is stored
{
    float4x4 viewProject;
};

struct Vertex
{
	float3 position;
};


struct PSInput
{
    float4 position : SV_POSITION;
    // float4 color : COLOR;
};

struct PerDrawConstants 
{
    float4x4 viewProject;
    uint vbuffer_idx;
};

struct CubeTransforms
{
    uint index;
};

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);
ConstantBuffer<CubeTransforms> cubeTransforms : register(b2);


PSInput VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    // StructuredBuffer<Vertex> vbuffer = ResourceDescriptorHeap[drawConstants.vbuffer_idx];
    StructuredBuffer<Vertex> vbuffer = ResourceDescriptorHeap[drawConstants.vbuffer_idx];
    StructuredBuffer<float4x4> modelMatrices = ResourceDescriptorHeap[cubeTransforms.index];
    float4x4 modelMatrix = modelMatrices[iid];
    float4 position_homogeneous = float4(vbuffer[vid].position, 1);
    PSInput result;
    result.position = mul(mul(position_homogeneous, modelMatrix), drawConstants.viewProject); // offset is visible outside of the struct
    // result.position = mul(position_homogeneous, drawConstants.viewProject); // offset is visible outside of the struct
    return result;
    
}
