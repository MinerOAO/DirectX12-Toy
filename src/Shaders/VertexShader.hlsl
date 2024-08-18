cbuffer cbPerObject : register(b0)
{
    float4x4 worldViewProj;
}
void VS(float3 iPos : POSITION, float4 iColor: COLOR, out float4 oPos : SV_POSITION, out float4 oColor: COLOR)
{
    oPos = mul(float4(iPos, 1.0f), worldViewProj);
    oColor = iColor;
}