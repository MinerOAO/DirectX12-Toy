#define MAX_DIRECT_LIGHT_SOURCE_NUM 8
#define MAX_POINT_LIGHT_SOURCE_NUM 8
#define MAX_SPOT_LIGHT_SOURCE_NUM 4
struct Light
{
    float3 strength;
    float falloffStart; //point/spot
    float3 direction; //Directional/Spot
    float falloffEnd;
    float3 position; //point/spot
    float spotPower;
};
struct Material
{
    float4 kd;
    float4 ks;
    float roughness;
};
float Attenuation(float start, float end, float distance)
{
    //start < end
    return saturate((end - distance) / (end - start)); //clamp to 0-1
}
float4 BlinnPhong(Material mat, float3 strength, float3 lightVec, float3 normal, float3 toEye)
{
    float3 halfVec = normalize(lightVec + toEye);

    //viewport independent, only light * normal, Lambert cosine law
    float4 diffuse = mat.kd * max(dot(lightVec, normal), 0.0f);
    //halfvector gives similarity between eyeVec and inverse lightVec
    float shininess = pow(2, (1 - mat.roughness) * 11);
    float4 specular = mat.ks * pow(max(dot(halfVec, normal), 0.0f), shininess);

    return float4(strength, 1.0f) * (diffuse + specular);
}