#include "Light.hlsl"
cbuffer cbMaterial : register(b1)
{
    float4 ambientAlbedo;
    float4 diffuseAlbedo;
    float4 specularAlbedo;
	// Used in texture mapping.
    float4x4 matTransform;
    float refraction;
    float roughness;
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
cbuffer cbLight : register(b3)
{
    float4 ambientLight;
    Light directionalLights[MAX_DIRECT_LIGHT_SOURCE_NUM];
    Light pointLights[MAX_POINT_LIGHT_SOURCE_NUM];
    Light spotLights[MAX_SPOT_LIGHT_SOURCE_NUM];
}
SamplerState defaultSampler : register(s0);
Texture2D diffuseMap : register(t0);

float4 ComputeLighting(Material m, float3 pos, float3 normal, float3 toEye)
{
    float4 result = 0.0f;
    int i = 0;
    //Diferences between lights: lightVec and strength calc ways
    
    for (i = 0; i < MAX_DIRECT_LIGHT_SOURCE_NUM; ++i)
    {
        //toEye->Point to eye
        //lightVec -> point to light source
        float3 lightVec = -directionalLights[i].direction;
        result += BlinnPhong(m, directionalLights[i].strength, lightVec, normal, toEye);;
    }
    for (i = 0; i < MAX_POINT_LIGHT_SOURCE_NUM; ++i)
    {
        float3 lightVec = pointLights[i].position - pos;
        float distance = length(lightVec);
        if (distance > pointLights[i].falloffEnd)
            continue;
        lightVec = normalize(lightVec);
        //Attenuation
        float3 lightStrength = pointLights[i].strength;
        lightStrength *= Attenuation(pointLights[i].falloffStart, pointLights[i].falloffEnd, distance);
        result += BlinnPhong(m, lightStrength, lightVec, normal, toEye);
    }
    for (i = 0; i < MAX_SPOT_LIGHT_SOURCE_NUM; ++i)
    {
        float3 lightVec = spotLights[i].position - pos;
        float distance = length(lightVec);
        if (distance > spotLights[i].falloffEnd)
            continue;
        lightVec = normalize(lightVec);
        //Attenuation
        float3 lightStrength = spotLights[i].strength * Attenuation(spotLights[i].falloffStart, spotLights[i].falloffEnd, distance);
        //Spot
        lightStrength *= pow(max(dot(-lightVec, spotLights[i].direction), 0.0f), spotLights[i].spotPower);
        
        result += BlinnPhong(m, lightStrength, lightVec, normal, toEye);
    }
    return result;
}

float4 PS(float4 posH : SV_POSITION, float4 posW : POSITION, float3 normalW : NORMAL, float2 texCoord : TEXC) : SV_TARGET
{
    //Interpolated normal may not be normalized
    normalW = normalize(normalW);
    float3 toEyeW = normalize(eyePosWorld - posW);
    
    float4 ka = ambientAlbedo, kd = 0.0f, ks = specularAlbedo;
    if (length(diffuseAlbedo) != 0.0f)
        kd = diffuseAlbedo;
    else
        kd = diffuseMap.Sample(defaultSampler, texCoord);
    
    ks *= kd;
    ka *= kd;
    
    //direct lighting
    Material mat = { kd, ks, roughness };
    float4 diffuseSpec = ComputeLighting(mat, posW, normalW, toEyeW);
    
    //indirect lighting
    float4 ambient = ambientLight * ka;
    
    //tone mapping to [0,1].
    float4 result = ambient + diffuseSpec;
    //result = result / (result + 1.0f);
    return result;
}