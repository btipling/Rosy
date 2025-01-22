import data;

[[vk::binding(0, 0)]] texture2D imageTexture[];
[[vk::binding(1, 0)]] SamplerState imageSampler[];

[[vk::push_constant]] BasicPushConstant BasicPushConstants;

[shader("vertex")]
BasicVertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    SceneData sd = *BasicPushConstants.sd;
    BasicInputVertex v = *(BasicPushConstants.v + uiVertexId);
    BasicGraphicsData gd = *BasicPushConstants.gd;
    float4x4 worldMat = gd.transform;

    float4 posWorld = float4(v.position, 1.0);

    BasicVertexStageOutput output;
    output.basicVertex.vertex = float4(v.position, 1.f);
    output.basicVertex.normal = v.normal;
    output.basicVertex.color = v.color;

    output.sv_position = mul(mul(sd.viewproj, worldMat), posWorld);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(BasicVertex basicVertex: BasicVertex) : SV_Target
{
    SceneData sd = *BasicPushConstants.sd;
    BasicMaterialData md = *BasicPushConstants.md;
    float3 sunlight = float3(sd.sunlight[0], sd.sunlight[1], sd.sunlight[2]);
    float3 V = normalize(sd.cameraPosition.xyz - basicVertex.vertex.xyz);
    float3 L = sunlight;
    float3 N = normalize(basicVertex.normal);
    float3 H = normalize(L + V);

    float cosTheta = dot(L, N);
    float cosPhi = dot(H, N);

    float3 ambientLight = sd.ambientColor.xyz;
    float3 sunLight = sd.sunlightColor.xyz * max(cosTheta, 0.0);

    // uint cti = md.color_texture_index;
    // uint csi = md.color_sampler_index;
    // texture2D matImageTexture = imageTexture[NonUniformResourceIndex(cti)];
    // SamplerState matSampler = imageSampler[NonUniformResourceIndex(csi)];
    // float4 imageTextureColor = matImageTexture.Sample(matSampler, coarseVertex.textureCoordinates);

    float4 outFragColor = md.color * 0.25 + float4(ambientLight * sunLight, 1.0) * 0.5;

    Fragment output;
    output.color = outFragColor;
    return output;
}
