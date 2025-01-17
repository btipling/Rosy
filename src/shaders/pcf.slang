
float pcf(float4 fragCoord, float4 vertex, texture2D shadowMap, SamplerState shadowSampler)
{
    float4 mapValue = shadowMap.Sample(shadowSampler, float2(vertex.x, vertex.y));
    if (mapValue.x <= vertex.z) {
        // Not occluded and not in shadow. Do not attenuate light.
        return 1.f;
    }
    float result = 0.0f;
    float shadowOffset = 0.00025;
    // Sample some more locations;
    mapValue = shadowMap.Sample(shadowSampler, float2(vertex.x - shadowOffset, vertex.y - shadowOffset));
    if (mapValue.x <= vertex.z) result += 0.1f;
    mapValue = shadowMap.Sample(shadowSampler, float2(vertex.x + shadowOffset * 2.f, vertex.y));
    if (mapValue.x <= vertex.z) result += 0.1f;
    mapValue = shadowMap.Sample(shadowSampler, float2(vertex.x, vertex.y + shadowOffset * 2.f));
    if (mapValue.x <= vertex.z) result += 0.1f;
    mapValue = shadowMap.Sample(shadowSampler, float2(vertex.x + shadowOffset * 2.f, vertex.y + shadowOffset * 2.f));
    if (mapValue.x <= vertex.z) result += 0.1f;
    mapValue = shadowMap.Sample(shadowSampler, float2(vertex.x - shadowOffset * 2.f, vertex.y - shadowOffset * 2.f));
    if (mapValue.x <= vertex.z) result += 0.1f;
    return result;
}