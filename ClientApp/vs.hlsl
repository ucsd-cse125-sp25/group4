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
    float4x4 modelViewProject;
    uint vbuffer_idx;
};

ConstantBuffer<PerDrawConstants> drawConstants : register(b1);


PSInput VSMain(uint vid : SV_VertexID)
{
    StructuredBuffer<Vertex> vbuffer = ResourceDescriptorHeap[drawConstants.vbuffer_idx];
    float4 position_homogeneous = float4(vbuffer[vid].position, 1);
    PSInput result;
    result.position = mul(position_homogeneous, drawConstants.modelViewProject); // offset is visible outside of the struct

    return result;
    
}
