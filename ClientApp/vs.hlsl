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

struct Mat4 
{
    float4x4 mat;
};

ConstantBuffer<Mat4> modelMatrix : register(b1);
ConstantBuffer<Mat4> viewProjectMatrix : register(b2);


PSInput VSMain(uint vid : SV_VertexID)
{
    StructuredBuffer<Vertex> vbuffer = ResourceDescriptorHeap[1];
    float4 position_homogeneous = float4(vbuffer[vid].position, 1);
    PSInput result;
    result.position = mul(mul(position_homogeneous, modelMatrix.mat), viewProjectMatrix.mat); // offset is visible outside of the struct

    return result;
    
}
