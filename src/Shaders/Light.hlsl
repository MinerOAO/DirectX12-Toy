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
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float shininess;
};
float Attenuation(float start, float end, float distance)
{
    //start < end
    return saturate((end - distance) / (end - start)); //clamp to 0-1
}
float3 SchlickFresnel(float3 r0, float3 normal, float3 lightVec)
{
    //all in unit length
    float cosAngle = saturate(dot(normal, lightVec));
    //Bigger r0 gives bigger base, which means small angle can result in great specular.
    return r0 + (1.0f - r0) * pow(1.0f - cosAngle, 5.0f);
}
float3 BlinnPhong(float3 strength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    float3 halfVec = normalize(lightVec + toEye);

    //halfvector gives similarity between eyeVec and inverse lightVec
    float3 shininessFactor = (1.0f + (mat.shininess / 8.0f)) * pow(max(dot(halfVec, normal), 0.0f), mat.shininess);
    float3 fresnelFactor = SchlickFresnel(mat.fresnelR0, halfVec, lightVec);
    
    float3 specAlbedo = shininessFactor * fresnelFactor;
    //tone mapping to [0,1].
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);
    return strength * (mat.diffuseAlbedo.rgb + specAlbedo); //(lightDire*normal)*(diffuse + spec)
}