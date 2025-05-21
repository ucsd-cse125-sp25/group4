ConstantBuffer<PerDrawConstants> drawConstants : register(b1);

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VSMain(uint vid : SV_VertexID) {
    StructuredBuffer<VertexPosition> vbuf = ResourceDescriptorHeap[drawConstants.vpos_idx];
    PSInput out;
    out.positionNDC = float4(vbuf[vid].position, 1);
    return out;
}