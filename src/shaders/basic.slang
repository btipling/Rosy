import data;
import pcf;

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
    if (sd.reverseCrossBitangent == 1) {
        bitangent = cross(tangent, normal) * new_tangent.w;
    }

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
    output.basicVertex.textureCoordinates = v.textureCoordinates;
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

    if (sd.fragmentOutput == 5 || BasicPushConstants.md == nullptr)
    {

        if (sd.lightEnabled > 0)
        {
            float3 V = normalize(basicVertex.viewDir);
            float3 L = normalize(basicVertex.lightDir);
            float3 N = normalize(basicVertex.normal);
            float3 H = L + V;

            float cosTheta = dot(L, N);
            float cosPhi = dot(H, N);

            float3 ambientLight = sd.ambientColor.xyz;
            float3 sunLight = sd.sunlightColor.xyz * max(cosTheta, 0.0);

            float4 outFragColor = basicVertex.color * float4(max(ambientLight, sunLight), 1.f);

            Fragment output;
            output.color = outFragColor;
            return output;
        }

        Fragment output;
        output.color = basicVertex.color;
        return output;

    }

    BasicMaterialData md = *BasicPushConstants.md;

    uint nti = md.normal_texture_index;
    uint nsi = md.normal_sampler_index;

    float3 lightNormal = normalize(basicVertex.normal);
    float3 lightDir = normalize(basicVertex.lightDir);
    float3 bitangent = basicVertex.bitangent;

    if (sd.normalMapsEnabled == 1 && validDescriptorIndex(nti))
    {
        texture2D normalMap = imageTexture[NonUniformResourceIndex(nti)];
        SamplerState normalSampler = imageSampler[NonUniformResourceIndex(nsi)];

        float3 m = normalMap.Sample(normalSampler, basicVertex.textureCoordinates).xyz * 2 - 1;
        float3 n = normalize(basicVertex.normal);
        float3 t = normalize(basicVertex.tangent - n * dot(basicVertex.tangent, n));
        float3 b = normalize(cross(basicVertex.normal, basicVertex.tangent.xyz) * basicVertex.sigma);
        lightNormal = normalize(t * m.x + b * m.y + n * m.z);
    }

    float3 V = normalize(basicVertex.viewDir);
    float3 L = lightDir;
    float3 N = lightNormal;
    float3 H = L + V;

    float cosTheta = dot(L, N);
    float cosPhi = dot(H, N);

    float4 outFragColor = md.color;
    switch (sd.fragmentOutput)
    {
    case 0:
        uint cti = md.color_texture_index;
        uint csi = md.color_sampler_index;

        if (validDescriptorIndex(cti))
        {
            texture2D matImageTexture = imageTexture[NonUniformResourceIndex(cti)];
            SamplerState matSampler = imageSampler[NonUniformResourceIndex(csi)];
            float4 imageTextureColor = matImageTexture.Sample(matSampler, basicVertex.textureCoordinates);
            outFragColor = imageTextureColor;
        }
        break;
    case 1:
        outFragColor = float4(normalize(basicVertex.worldNormal) * 0.5 + 0.5, 1.f);
        break;
    case 2:
        outFragColor = float4(normalize(basicVertex.tangent.xyz) * 0.5 + 0.5, 1.f);
        break;
    case 3:
        outFragColor = float4(normalize(basicVertex.lightDir) * 0.5 + 0.5, 1.f);
        break;
    case 4:
        outFragColor = float4(normalize(basicVertex.viewDir) * 0.5 + 0.5, 1.f);
        break;
    case 6:
        outFragColor = float4(lightNormal * 0.5 + 0.5, 1.f);
        break;
    case 7:
        outFragColor = float4(bitangent * 0.5 + 0.5, 1.f);
        break;
    case 8:
        outFragColor = float4(float3(cosTheta, cosTheta, cosTheta) * 0.5 + 0.5, 1.f);
        break;
    case 9:
        outFragColor = float4(float3(cosPhi, cosPhi, cosPhi) * 0.5 + 0.5, 1.f);
        break;
    default:
        break;
    }

    if (sd.lightEnabled > 0)
    {
        float3 ambientLight = sd.ambientColor.xyz;
        float3 sunLight = sd.sunlightColor.xyz * max(cosTheta, 0.0);

        float shadowFactor = 1.f;
        if (sd.shadowsEnabled == 1)
        {
            uint csmSampler = sd.csmIndexSampler;
            SamplerComparisonState shadowSampler = shadowSampler[NonUniformResourceIndex(csmSampler)];
            uint csmNear = sd.csmIndexNear;
            texture2D shadowNearImage = imageTexture[NonUniformResourceIndex(csmNear)];
            shadowFactor = pcf(fragCoord, csm.near, shadowNearImage, shadowSampler);
        }

        outFragColor = outFragColor * float4(max(ambientLight, sunLight * shadowFactor), 1.0);
    }

    Fragment output;
    output.color = outFragColor;
    return output;
}
