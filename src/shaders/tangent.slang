module tangent;

public float3 calcNewNormal(float3 normal, float4 tangent, float2 textureCoordinates, texture2D normalMap, SamplerState normalSampler)
{
    float3 bitangent = cross(tangent.xyz, normal) * tangent.w;
    float3x3 tbn = float3x3(tangent.xyz, bitangent, normal);
    float3 retrievedNormal = normalMap.Sample(normalSampler, textureCoordinates).xyz;
    retrievedNormal = retrievedNormal * 2.0 - 1.0;
    float3 newNormal = mul(tbn, retrievedNormal);
    newNormal = normalize(newNormal);
    return newNormal;
}