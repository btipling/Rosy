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
    float4x4 normalMat = gd.normalTransform;

    float4 object_space_camera = mul(gd.objectSpaceTransform, sd.cameraPosition);
    float4 object_light_dir = mul(gd.objectSpaceTransform, float4(sd.sunlight.xyz, 0.f));

    float4 posWorld = float4(v.position, 1.0);

    float4x4 ndc_to_tc = { 0.5, 0, 0, 0.5, 0, 0.5, 0, 0.5, 0, 0, 1, 0, 0, 0, 0, 1 };

    float3 light = object_light_dir.xyz;
    float4 new_tangent = v.tangent;

    // flip lights
    if (sd.flipLights[0] == 1)
    {
        light = light * float3(-1, 1, 1);
    }
    if (sd.flipLights[1] == 1)
    {
        light = light * float3(1, -1, 1);
    }
    if (sd.flipLights[2] == 1)
    {
        light = light * float3(1, 1, -1);
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

    float3 normal = v.normal;
    float3 tangent = new_tangent.xyz;
    float3 bitangent = cross(normal, tangent) * new_tangent.w;

    float3 view = object_space_camera.xyz - v.position.xyz;

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

    if (tangentSpaceEnabled)
    {
        view = float3(dot(tangent, view), dot(bitangent, view), dot(normal, view));
        light = float3(dot(tangent, light), dot(bitangent, light), dot(normal, light));
    }

    BasicVertexStageOutput output;
    output.basicVertex.vertex = float4(v.position, 1.f);
    output.basicVertex.normal = normal;
    output.basicVertex.worldNormal = mul(gd.normalTransform, float4(v.normal, 0.f)).xyz;
    output.basicVertex.color = v.color;
    output.basicVertex.tangent = tangent * v.tangent.w;
    output.basicVertex.viewDir = view;
    output.basicVertex.lightDir = light;
    output.basicVertex.textureCoordinates = v.textureCoordinates;
    output.csm.near = mul(mul(mul(ndc_to_tc, sd.shadowProjNear), worldMat), posWorld);

    output.sv_position = mul(mul(sd.viewproj, worldMat), posWorld);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(BasicVertex basicVertex: BasicVertex, CSM csm: CSM, float4 fragCoord: SV_Position) : SV_Target
{
    SceneData sd = *BasicPushConstants.sd;

    if (BasicPushConstants.md == nullptr)
    {

        float3 V = normalize(basicVertex.viewDir);
        float3 L = normalize(basicVertex.lightDir);
        float3 N = normalize(basicVertex.normal);
        float3 H = L + V;

        float cosTheta = dot(L, N);
        float cosPhi = dot(H, N);

        float3 ambientLight = sd.ambientColor.xyz;
        float3 sunLight = sd.sunlightColor.xyz * max(cosTheta, 0.0);

        float4 outFragColor = basicVertex.color * float4(ambientLight * sunLight, 1.0);

        Fragment output;
        output.color = outFragColor;
        return output;
    }

    BasicMaterialData md = *BasicPushConstants.md;

    uint nti = md.normal_texture_index;
    uint nsi = md.normal_sampler_index;

    float3 lightNormal = basicVertex.normal;

    if (validDescriptorIndex(nti))
    {
        texture2D normalMap = imageTexture[NonUniformResourceIndex(nti)];
        SamplerState normalSampler = imageSampler[NonUniformResourceIndex(nsi)];

        lightNormal = normalMap.Sample(normalSampler, basicVertex.textureCoordinates).xyz;
    }

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
        outFragColor = float4(normalize(basicVertex.tangent) * 0.5 + 0.5, 1.f);
        break;
    case 3:
        outFragColor = float4(normalize(basicVertex.lightDir) * 0.5 + 0.5, 1.f);
        break;
    case 4:
        outFragColor = float4(normalize(basicVertex.viewDir) * 0.5 + 0.5, 1.f);
        break;
    }

    if (sd.lightEnabled > 0)
    {
        float3 V = normalize(basicVertex.viewDir);
        float3 L = normalize(basicVertex.lightDir);
        float3 N = lightNormal;
        float3 H = L + V;

        float cosTheta = dot(L, N);
        float cosPhi = dot(H, N);

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

        outFragColor =
            max(outFragColor * float4(ambientLight, 1.f), outFragColor * float4(sunLight * shadowFactor, 1.0));
    }

    Fragment output;
    output.color = outFragColor;
    return output;
}
