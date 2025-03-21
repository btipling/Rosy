module lighting;

public static const float PI = 3.14159265359;

public float3 schlickFresnel(float lDotH, float3 f0)
{
    return f0 + (1 - f0) * pow(1.f - lDotH, 5.f);
}

public float geoSmith(float dotProd, float roughness)
{
    float k = (roughness + 1.f) * (roughness + 1.f) / 8.f;
    float denom = dotProd * (1 - k) + k;
    return dotProd / denom;
}

public float ggxDistribution(float cosPhi, float roughness)
{
    float alpha2 = roughness * roughness * roughness * roughness;
    float d = (cosPhi * cosPhi) * (alpha2 - 1) + 1;
    return alpha2 / (PI * d * d);
}
