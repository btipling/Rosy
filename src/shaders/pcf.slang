float pcf(float4 fragCoord, float4 vertex, texture2D shadowMap, SamplerComparisonState shadowSampler)
{
    float result = 0.0f;
    float shadowOffset = 0.00025;
    // Sample some more locations;
    result += shadowMap.SampleCmpLevelZero(shadowSampler, float2(vertex.x - shadowOffset, vertex.y - shadowOffset), vertex.z);
    result += shadowMap.SampleCmpLevelZero(shadowSampler, float2(vertex.x + shadowOffset * 2.f, vertex.y), vertex.z);
    result += shadowMap.SampleCmpLevelZero(shadowSampler, float2(vertex.x, vertex.y + shadowOffset * 2.f), vertex.z);
    result += shadowMap.SampleCmpLevelZero(shadowSampler, float2(vertex.x + shadowOffset * 2.f, vertex.y + shadowOffset * 2.f), vertex.z);
    result += shadowMap.SampleCmpLevelZero(shadowSampler, float2(vertex.x - shadowOffset * 2.f, vertex.y - shadowOffset * 2.f), vertex.z);
    return result * (1.0 / 5.0f);
}
