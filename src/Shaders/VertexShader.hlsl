cbuffer cbPerObject : register(b0)
{
    float4x4 world;
}
cbuffer cbPassObject : register(b1)
{
    float4x4 view;
    float4x4 inverseView;
    float4x4 proj;
    float4x4 inverseProj;
    float4x4 viewProj;
    float4x4 inverseViewProj;
		
    float2 RTVSize;
    float2 invRTVSize;
    float nearZ;
    float farZ;
    float totalTime;
}
void VS(float3 iPos : POSITION, float4 iColor: COLOR, out float4 oPos : SV_POSITION, out float4 oColor: COLOR)
{
    float4 posWorld = mul(float4(iPos, 1.0f), world);
    oPos = mul(posWorld, viewProj);
    oColor = iColor;
}