import data;
import pcf;
import lighting;

[[vk::binding(0, 0)]]
texture2D imageTexture[];
[[vk::binding(1, 0)]]
SamplerState imageSampler[];
[[vk::binding(1, 0)]]
SamplerComparisonState shadowSampler[];

[[vk::push_constant]]
BasicPushConstant BasicPushConstants;

[shader("vertex")]
BasicVertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    SceneData sd = *BasicPushConstants.sd;
    BasicInputVertex v = *(BasicPushConstants.v + uiVertexId);
    BasicGraphicsData gd = *BasicPushConstants.gd;
    float4x4 worldMat = gd.transform;
    float3x3 normalMat = gd.normalTransform;

    float4x4 ndc_to_tc = { 0.5, 0, 0, 0.5, 0, 0.5, 0, 0.5, 0, 0, 1, 0, 0, 0, 0, 1 };

    float4 new_tangent = v.tangent;

    // ignore asset tangent sign
    if (sd.ignoreAssetTangentSign == 1) {
        new_tangent.w = 1.f;
    }
    // flip tangents
    if (sd.flipTangents[0] == 1)
    {
        new_tangent = new_tangent * float4(-1, 1, 1, 1);
    }
    if (sd.flipTangents[1] == 1)
    {
        new_tangent = new_tangent * float4(1, -1, 1, 1);
    }
    if (sd.flipTangents[2] == 1)
    {
        new_tangent = new_tangent * float4(1, 1, -1, 1);
    }
    if (sd.flipTangents[3] == 1)
    {
        new_tangent = new_tangent * float4(1, 1, 1, -1);
    }

    bool tangentSpaceEnabled = sd.tangentSpaceEnabled == 1;
    if (BasicPushConstants.md != nullptr)
    {
        BasicMaterialData md = *BasicPushConstants.md;
        uint nti = md.normal_texture_index;
        if (!validDescriptorIndex(nti))
        {
            tangentSpaceEnabled = false;
        }
    }

    float3 normal = v.normal;
    float3 tangent = new_tangent.xyz;

    float3 bitangent = cross(normal, tangent) * new_tangent.w;
    if (sd.ensureOrthogonalBitangent == 1) {
        float3 normal_projection = dot(normal, bitangent) * bitangent;
        float3 ortho_n = normalize(normal - normal_projection);
        float3 tangent_projection = normal_projection - dot(tangent, bitangent) * bitangent;
        float3 ortho_t = normalize(tangent - tangent_projection);
        bitangent = cross(ortho_n, ortho_t) * new_tangent.w;
    }

    float3 viewDir = sd.cameraPosition.xyz - mul(worldMat, float4(v.position, 1.f)).xyz;
    float3 lightDir = sd.sun_position.xyz - mul(worldMat, float4(v.position, 1.f)).xyz;
    if (tangentSpaceEnabled) {
        viewDir = mul(gd.toObjectSpaceTransform, sd.cameraPosition).xyz - v.position;
        lightDir = mul(gd.toObjectSpaceTransform, sd.sun_position).xyz - v.position;
    }
    else
    {
        normal = normalize(mul(normalMat, v.normal));
        tangent = normalize(mul(normalMat, new_tangent.xyz));
        tangent = normalize(tangent - normal * dot(normal, tangent));
    }

    // flip lights
    if (sd.flipLights[0] == 1)
    {
        lightDir = lightDir * float3(-1, 1, 1);
    }
    if (sd.flipLights[1] == 1)
    {
        lightDir = lightDir * float3(1, -1, 1);
    }
    if (sd.flipLights[2] == 1)
    {
        lightDir = lightDir * float3(1, 1, -1);
    }

    float4 vertPos = float4(v.position, 1.0);
    float4 outVertex = mul(worldMat, vertPos);

    BasicVertexStageOutput output;
    output.basicVertex.vertex = outVertex.xyz / outVertex.w;
    output.basicVertex.normal = normal;
    output.basicVertex.worldNormal = mul(gd.normalTransform, v.normal);
    output.basicVertex.color = v.color;
    output.basicVertex.tangent = new_tangent.xyz;
    output.basicVertex.bitangent = bitangent;
    output.basicVertex.viewDir = viewDir;
    output.basicVertex.lightDir = lightDir;
    output.basicVertex.sigma = new_tangent.w;
    output.basicVertex.uvs = v.uvs;
    output.csm.near = mul(mul(mul(ndc_to_tc, sd.shadowProjNear), worldMat), vertPos);

    output.sv_position = mul(mul(sd.viewproj, worldMat), vertPos);

    return output;
}

float srgbToLinear(float srgbValue)
{
    float linearValue = srgbValue / 12.92;
    if (srgbValue > 0.04045) {
        linearValue = pow((srgbValue + 0.055) / 1.055, 2.4);
    }
    return linearValue;
}

[shader("fragment")]
Fragment fragmentMain(BasicVertex basicVertex: BasicVertex, CSM csm: CSM, float4 fragCoord: SV_Position) : SV_Target
{
    SceneData sd = *BasicPushConstants.sd;
    BasicGraphicsData gd = *BasicPushConstants.gd;

    float3 ambientLight = sd.ambientColor.xyz;
    float4 outFragColor = basicVertex.color;
    float4 imageTextureColor = outFragColor;
    float alpha = basicVertex.color.w;

    bool hasMetallic = false;
    bool hasMixmap = false;
    float sampledRoughness = 0.f;
    float sampledMetallic = 0.f;

    bool hasMaterial = BasicPushConstants.md != nullptr;

    BasicMaterialData md;

    float3 lightNormal = normalize(basicVertex.normal);
    float3 lightDir = normalize(basicVertex.lightDir);
    float3 bitangent = basicVertex.bitangent;
    float lightDistance = length(basicVertex.lightDir);

    if (hasMaterial) {
        md = *BasicPushConstants.md;
        outFragColor = md.color;
        imageTextureColor = outFragColor;
        alpha = min(md.color.w, alpha);

        uint nti = md.normal_texture_index;
        uint nsi = md.normal_sampler_index;
        if (sd.normalMapsEnabled == 1 && validDescriptorIndex(nti))
        {
            texture2D normalMap = imageTexture[NonUniformResourceIndex(nti)];
            SamplerState normalSampler = imageSampler[NonUniformResourceIndex(nsi)];

            float3 m = normalMap.Sample(normalSampler, basicVertex.uvs).xyz * 2 - 1;
            float3 n = normalize(basicVertex.normal);
            float3 t = normalize(basicVertex.tangent - n * dot(basicVertex.tangent, n));
            float3 b = normalize(cross(basicVertex.normal, basicVertex.tangent.xyz) * basicVertex.sigma);
            lightNormal = normalize(t * m.x + b * m.y + n * m.z);
        }

        uint cti = md.color_texture_index;
        uint csi = md.color_sampler_index;
        if (validDescriptorIndex(cti))
        {
            texture2D matImageTexture = imageTexture[NonUniformResourceIndex(cti)];
            SamplerState matSampler = imageSampler[NonUniformResourceIndex(csi)];
            imageTextureColor = matImageTexture.Sample(matSampler, basicVertex.uvs);
            alpha = min(imageTextureColor.w, alpha);
        }
        uint mixti = md.mixmap_texture_index;
        uint mixsi = md.mixmap_sampler_index;
        uint metti = md.metallic_texture_index;
        uint metsi = md.metallic_sampler_index;

        hasMetallic = validDescriptorIndex(metti);
        hasMixmap = validDescriptorIndex(mixti);
        if (hasMixmap) {
            texture2D mixTexture = imageTexture[NonUniformResourceIndex(mixti)];
            SamplerState mixSampler = imageSampler[NonUniformResourceIndex(mixsi)];
            float4 mixSample = mixTexture.Sample(mixSampler, basicVertex.uvs);
            // red is ambient occlusion - currently unused
            // green is roughness
            // blue is metalness
            // these values are 0 to 1
            sampledRoughness = mixSample.y;
            sampledMetallic = mixSample.z;
        } else {
            texture2D metTexture = imageTexture[NonUniformResourceIndex(metti)];
            SamplerState metSampler = imageSampler[NonUniformResourceIndex(metsi)];
            float4 metSample = metTexture.Sample(metSampler, basicVertex.uvs);
            // gltf metallicRoughness gltf spec
            // red reserved for optional values like occlusion
            // green is roughness
            // blue is metallic
            // these values are also 0 to 1
            sampledRoughness = metSample.y;
            sampledMetallic = metSample.z;
        }
    }

    float3 V = normalize(basicVertex.viewDir);
    float3 L = lightDir;
    float3 N = lightNormal;
    float3 H = normalize(L + V);

    float cosTheta = max(dot(L, N), 0);
    float cosPhi = max(dot(H, N), 0);
    float lDotH = max(dot(L, H), 0);
    float nDotV = max(dot(N, V), 0);

    switch (sd.fragmentOutput)
    {
    case 0:
        outFragColor = float4(imageTextureColor.xyz, alpha);
        break;
    case 1:
        outFragColor = float4(normalize(basicVertex.worldNormal) * 0.5 + 0.5, alpha);
        break;
    case 2:
        outFragColor = float4(normalize(basicVertex.tangent.xyz) * 0.5 + 0.5, alpha);
        break;
    case 3:
        outFragColor = float4(normalize(basicVertex.lightDir) * 0.5 + 0.5, alpha);
        break;
    case 4:
        outFragColor = float4(normalize(basicVertex.viewDir) * 0.5 + 0.5, alpha);
        break;
    case 5:
        outFragColor = float4(basicVertex.color.xyz, alpha);
        break;
    case 6:
        outFragColor = float4(lightNormal * 0.5 + 0.5, alpha);
        break;
    case 7:
        outFragColor = float4(bitangent * 0.5 + 0.5, alpha);
        break;
    case 8:
        outFragColor = float4(float3(cosTheta, cosTheta, cosTheta) * 0.5 + 0.5, alpha);
        break;
    case 9:
        outFragColor = float4(float3(cosPhi, cosPhi, cosPhi) * 0.5 + 0.5, alpha);
        break;
    case 10:
        outFragColor = float4(float3(lDotH, lDotH, lDotH) * 0.5 + 0.5, alpha);
        break;
    case 11:
        outFragColor = float4(float3(nDotV, nDotV, nDotV) * 0.5 + 0.5, alpha);
        break;
    case 12:
        outFragColor = float4(float3(basicVertex.uvs.x, basicVertex.uvs.y, 0.f), alpha);
        break;
    case 13:
        outFragColor = float4(float3(sampledRoughness, 0.05f, 0.05f), alpha);
        break;
    case 14:
        outFragColor = float4(float3(sampledMetallic, 0.05f, 0.05f), alpha);
        break;
    case 15:
        outFragColor = float4(float3(1 - alpha, 0.05f, 0.05f), alpha);
        break;
    default:
        break;
    }

    if (sd.lightEnabled > 0)
    {
        float shadowFactor = 1.f;
        if (sd.shadowsEnabled == 1)
        {
            uint csmSampler = sd.csmIndexSampler;
            SamplerComparisonState shadowSampler = shadowSampler[NonUniformResourceIndex(csmSampler)];
            uint csmNear = sd.csmIndexNear;
            texture2D shadowNearImage = imageTexture[NonUniformResourceIndex(csmNear)];
            shadowFactor = pcf(fragCoord, csm.near, shadowNearImage, shadowSampler);
        }
        if (sd.brdfLightingEnabled == 0) {
            float3 sunLight = sd.sunlightColor.xyz * max(cosTheta, 0.0);
            outFragColor = outFragColor * float4(max(ambientLight, sunLight * shadowFactor), alpha);
        } else {
            BRDFMaterialInfo matInfo;
            float3 diffuseColor = outFragColor.xyz;
            matInfo.color = float3(0.04f);
            matInfo.isMetal = hasMetallic || hasMixmap;
            matInfo.roughness = 0.f;
            if (matInfo.isMetal) {
                matInfo.color = outFragColor.xyz;

                float roughness = md.roughnessFactor;
                float metallic = md.metallicFactor;
                roughness = sampledRoughness * roughness;
                metallic = sampledMetallic * metallic;
                diffuseColor = diffuseColor *= 1.f - metallic;
                matInfo.roughness = roughness;
            }
            float3 lightIntensity = sd.sunlightColor.xyz;
            float3 specBRDF = 0.25f * ggxDistribution(cosPhi, matInfo) * schlickFresnel(lDotH, matInfo) * geoSmith(cosTheta, matInfo) * geoSmith(nDotV, matInfo);
            float3 colorBRDF = (diffuseColor + 3.14159265f * specBRDF) * lightIntensity * cosTheta;
            outFragColor = float4(colorBRDF * shadowFactor, alpha);
        }
    }

    if (hasMaterial) {
        if (md.alphaMode == 1 && alpha < md.alphaCutoff) {
            discard;
        }
        outFragColor.w = alpha;
    }

    Fragment output;
    output.color = outFragColor;
    return output;
}
