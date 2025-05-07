// TODO: create a shared hlsl header
struct PSInput
{
    float4 position : SV_POSITION;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(0, 1, 0, 1);
}
