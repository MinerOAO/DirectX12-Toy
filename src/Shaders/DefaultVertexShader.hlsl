cbuffer cbPerObject : register(b0)
{
    float4x4 world;
}
cbuffer cbPassObject : register(b2)
{
    float4x4 view;
    float4x4 inverseView;
    float4x4 proj;
    float4x4 inverseProj;
    float4x4 viewProj;
    float4x4 inverseViewProj;
		
    float4 eyePosWorld;
    
    float2 RTVSize;
    float2 invRTVSize;
    float nearZ;
    float farZ;
    float totalTime;
}

void VS(float3 posL : POSITION, float3 normalL : NORMAL,
    out float4 posH : SV_POSITION, out float4 posW : POSITION, out float3 normalW : NORMAL, inout float2 texC : TEXC)//Sequence order matters
{
    //Transform to world space
    posW = mul(float4(posL, 1.0f), world);
    //Transform to homogeneous clip space
    posH = mul(posW, viewProj);
    // nonuniform scaling need to use inverse-transpose of world matrix (A^-1)T
    normalW = mul(normalL, (float3x3) world);
}