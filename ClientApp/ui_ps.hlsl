struct PSInput
{
    float4 position : SV_POSITION;
};

float4 PSMain(PSInput in) : SV_TARGET
{
    return float4(1, 0, 0, 1);
}