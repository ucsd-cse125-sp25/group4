#include "shaderShared.hlsli"
struct PSInput_UI
{
    float4 position : SV_POSITION;
};

float4 PSMain(PSInput_UI input) : SV_TARGET
{
    return float4(1, 0, 0, 1);
}