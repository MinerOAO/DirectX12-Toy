#include "Light.hlsl"
cbuffer cbPerObject : register(b0)
{
    float4x4 world;
}
cbuffer cbMaterial : register(b1)
{
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float shininess;
	// Used in texture mapping.
    float4x4 matTransform;
}
cbuffer cbPassObject : register(b2)
{
    float4x4 view;
    float4x4 inverseView;
    float4x4 proj;
    float4x4 inverseProj;
    float4x4 viewProj;
    float4x4 inverseViewProj;
		
    float3 eyePosWorld;
    float totalTime;
    
    float2 RTVSize;
    float2 invRTVSize;
    float nearZ;
    float farZ;
}
cbuffer cbLight : register(b3)
{
    float4 ambientLight;
    Light directionalLights[MAX_DIRECT_LIGHT_SOURCE_NUM];
    Light pointLights[MAX_POINT_LIGHT_SOURCE_NUM];
    Light spotLights[MAX_SPOT_LIGHT_SOURCE_NUM];
}

float4 ComputeLighting(Material m, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 result = (float3)0.0f;
    int i = 0;
    //Diferences between lights: lightVec and strength calc ways
    
    for (i = 0; i < MAX_DIRECT_LIGHT_SOURCE_NUM; ++i)
    {
        //toEye->Point to eye
        //lightVec -> point to light source
        float3 lightVec = -directionalLights[i].direction;
        //Lambert cosine law
        float lightStrength = max(dot(lightVec, normal), 0.0f);

        result += BlinnPhong(directionalLights[i].strength * lightStrength, lightVec, normal, toEye, m);;
    }
    for (i = 0; i < MAX_POINT_LIGHT_SOURCE_NUM; ++i)
    {
        float3 lightVec = pointLights[i].position - pos;
        float distance = length(lightVec);
        if (distance > pointLights[i].falloffEnd)
            continue;
        lightVec = normalize(lightVec);
        //Lambert cosine law
        float lightStrength = max(dot(lightVec, normal), 0.0f);
        //Attenuation
        lightStrength *= Attenuation(pointLights[i].falloffStart, pointLights[i].falloffEnd, distance);

        result += BlinnPhong(pointLights[i].strength * lightStrength, lightVec, normal, toEye, m);
    }
    for (i = 0; i < MAX_SPOT_LIGHT_SOURCE_NUM; ++i)
    {
        float3 lightVec = spotLights[i].position - pos;
        float distance = length(lightVec);
        if (distance > spotLights[i].falloffEnd)
            continue;
        lightVec = normalize(lightVec);
        //Lambert cosine law
        float lightStrength = max(dot(lightVec, normal), 0.0f);
        //Attenuation
        lightStrength *= Attenuation(spotLights[i].falloffStart, spotLights[i].falloffEnd, distance);
        //Spot
        lightStrength *= pow(max(dot(-lightVec, spotLights[i].direction), 0.0f), spotLights[i].spotPower);
        
        result += BlinnPhong(spotLights[i].strength * lightStrength, lightVec, normal, toEye, m);
    }
    
    float l = length(result);
    if (l < 0.1f)
        result *= 0.0f;
    if (l > 0.9f)
        result *= 1.1f;
    if (l < 0.5f)
        result *= 0.3f;
    else
        result *= 0.7f;
    
    return float4(result, 0.0f);
}

void VS(float3 posL : POSITION, float3 normalL : NORMAL, out float4 posH : SV_POSITION, out float3 posW : POSITION, out float3 normalW : NORMAL)
{
    //Transform to world space
    float4 posWorld = mul(float4(posL, 1.0f), world);
    posW = posWorld.xyz;
    //Transform to homogeneous clip space
    posH = mul(posWorld, viewProj);
    // nonuniform scaling need to use inverse-transpose of world matrix (A^-1)T
    normalW = mul(normalL, (float3x3) world);
}
float4 PS(float4 posH : SV_POSITION, float3 posW : POSITION, float3 normalW : NORMAL) : SV_TARGET
{
    //Interpolated normal may not be normalized
    normalW = normalize(normalW);
    float3 toEyeW = normalize(eyePosWorld - posW);
    
    //indirect lighting
    float4 ambient = ambientLight * diffuseAlbedo;
    //direct lighting
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float4 diffuseSpec = ComputeLighting(mat, posW, normalW, toEyeW, 1.0f);
    
    return ambient + diffuseSpec;
}