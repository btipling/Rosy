module lighting;

public struct BRDFMaterialInfo
{
    public float roughness;
    public bool isMetal;
    public float3 color;
}

public float3 schlickFresnel(float lDotH, BRDFMaterialInfo matInfo)
{

    float3 f0 = float3(0.04f);
    if (matInfo.isMetal)
    {
        f0 = matInfo.color;
    }
    return f0 + (1 - f0) * pow(1.f - lDotH, 5.f);
}

public float geoSmith(float dotProd, BRDFMaterialInfo matInfo)
{
    float k = (matInfo.roughness + 1.f) * (matInfo.roughness + 1.f) / 8.f;
    float denom = dotProd * (1 - k) + k;
    return 1.f / denom;
}

public float ggxDistribution(float cosPhi, BRDFMaterialInfo matInfo)
{
    float alpha2 = matInfo.roughness * matInfo.roughness * matInfo.roughness * matInfo.roughness;
    float d = (cosPhi * cosPhi) * (alpha2 - 1) + 1;
    return alpha2 / 3.14159265f * d * d;
}

