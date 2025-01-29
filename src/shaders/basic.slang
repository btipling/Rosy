import data;
import pcf;

[[vk::binding(0, 0)]] texture2D imageTexture[];
[[vk::binding(1, 0)]] SamplerState imageSampler[];
[[vk::binding(1, 0)]] SamplerComparisonState shadowSampler[];

[[vk::push_constant]] BasicPushConstant BasicPushConstants;

[shader("vertex")]
BasicVertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    SceneData sd = *BasicPushConstants.sd;
    BasicInputVertex v = *(BasicPushConstants.v + uiVertexId);
    BasicGraphicsData gd = *BasicPushConstants.gd;
    float4x4 worldMat = gd.transform;
    float4x4 normalMat = gd.normalTransform;

    float4 posWorld = float4(v.position, 1.0);

    float4x4 ndc_to_tc = {
        0.5, 0, 0, 0.5,
        0, 0.5, 0, 0.5,
        0, 0, 1, 0,
        0, 0, 0, 1
    };


    float3 normal = normalize(mul(float3x3(normalMat), v.normal));
    float3 tangent = normalize(mul(float3x3(normalMat), v.tangent.xyz));
    float3 bitangent = normalize(cross(normal, tangent)) * v.tangent.w;

    float3x3 toObjectLocal = float3x3(
        float3(tangent.x, bitangent.x, normal.x),
        float3(tangent.y, bitangent.y, normal.y),
        float3(tangent.z, bitangent.z, normal.z),
    );

    float3 lightDir = normalize(mul(toObjectLocal, sd.sunlight.xyz));
    float3 viewDir = normalize(mul(toObjectLocal, sd.cameraPosition.xyz - v.position.xyz));

    BasicVertexStageOutput output;
    output.basicVertex.vertex = float4(v.position, 1.f);
    output.basicVertex.normal = normal;
    output.basicVertex.color = v.color;
    output.basicVertex.textureCoordinates = v.textureCoordinates;
    output.csm.near = mul(
        mul(
            mul(ndc_to_tc, sd.shadowProjNear), 
            worldMat),
        posWorld
    );

    output.sv_position = mul(mul(sd.viewproj, worldMat), posWorld);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(BasicVertex basicVertex: BasicVertex, CSM csm: CSM, float4 fragCoord: SV_Position) : SV_Target
{
    SceneData sd = *BasicPushConstants.sd;
    BasicMaterialData md = *BasicPushConstants.md;

    uint nti = md.normal_texture_index;
    uint nsi = md.normal_sampler_index;

    float3 lightNormal = basicVertex.normal;

    texture2D normalMap = imageTexture[NonUniformResourceIndex(nti)];
    SamplerState normalSampler = imageSampler[NonUniformResourceIndex(nsi)];

    lightNormal = normalMap.Sample(normalSampler, basicVertex.textureCoordinates).xyz;

    float3 sunlight = float3(sd.sunlight[0], sd.sunlight[1], sd.sunlight[2]);
    float3 V = normalize(sd.cameraPosition.xyz - basicVertex.vertex.xyz);
    float3 L = normalize(sunlight);
    float3 N = normalize(lightNormal);
    float3 H = normalize(L + V);

    float cosTheta = dot(L, N);
    float cosPhi = dot(H, N);

    float3 ambientLight = sd.ambientColor.xyz;
    float3 sunLight = sd.sunlightColor.xyz * max(cosTheta, 0.0);

    uint cti = md.color_texture_index;
    uint csi = md.color_sampler_index;

    texture2D matImageTexture = imageTexture[NonUniformResourceIndex(cti)];
    SamplerState matSampler = imageSampler[NonUniformResourceIndex(csi)];
    float4 imageTextureColor = matImageTexture.Sample(matSampler, basicVertex.textureCoordinates);

    float shadowFactor = 1.f;
    uint csmSampler = sd.csmIndexSampler;
    SamplerComparisonState shadowSampler = shadowSampler[NonUniformResourceIndex(csmSampler)];
    uint csmNear = sd.csmIndexNear;
    texture2D shadowNearImage = imageTexture[NonUniformResourceIndex(csmNear)];
    shadowFactor = pcf(fragCoord, csm.near, shadowNearImage, shadowSampler);

    float4 outFragColor = imageTextureColor * float4(ambientLight + shadowFactor * sunLight, 1.0);

    Fragment output;
    output.color = outFragColor;
    // output.color = float4(lightNormal.xyz * 0.5 + 0.5, 1.f);
    return output;
}
