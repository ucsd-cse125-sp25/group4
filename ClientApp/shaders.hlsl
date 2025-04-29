cbuffer SceneConstantBuffer : register(b0) // b0 is the "virtual register" where the constant buffer is stored
{
    float4x4 viewProject;
};


struct PSInput
{
    float4 position : SV_POSITION;
    // float4 color : COLOR;
};

PSInput VSMain(float3 position : POSITION)
{
    PSInput result;
    
    // TODO unpack position once we have a more compressed format 
    float4 position_homogeneous = float4(position, 1);
    result.position = mul(position_homogeneous, viewProject); // offset is visible outside of the struct

    return result;
}

float4 PSMain(PSInput input, uint id : SV_PrimitiveID) : SV_TARGET
{
    return float4(1, id/12.0, 0, 1);
}