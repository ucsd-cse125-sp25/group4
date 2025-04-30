// TODO: create a shared hlsl header
struct PSInput
{
    float4 position : SV_POSITION;
};

float4 PSMain(PSInput input, uint id : SV_PrimitiveID) : SV_TARGET
{
    return float4(1, 0, id/12.0, 1);
}
