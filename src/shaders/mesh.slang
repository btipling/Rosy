import data.data;

StructuredBuffer<InputVertex, ScalarDataLayout> VertexBuffer;
StructuredBuffer<RenderData, ScalarDataLayout> RenderDataBuffer;
StructuredBuffer<MaterialData, ScalarDataLayout> MaterialDataBuffer;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;
[[vk::binding(0, 1)]] texture2D imageTexture[];
[[vk::binding(1, 1)]] SamplerState imageSampler[];

[[vk::push_constant]] PushConstant PushConstants;

[shader("vertex")]
VertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    InputVertex v = *(PushConstants.v + uiVertexId);
    RenderData rd = *PushConstants.rd;

    float4x4 worldMat = rd.transform;

    VertexStageOutput output;
    output.coarseVertex.vertex = v.position;
    output.coarseVertex.color = v.color.xyz;
    output.coarseVertex.normal = mul(rd.normalTransform, v.normal);
    output.coarseVertex.textureCoordinates = float2(v.textureCoordinates_x, v.textureCoordinates_y);
    float4 posWorld = float4(v.position, 1.0);
    output.sv_position = mul(mul(sceneData.viewproj, worldMat), posWorld);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target
{

    MaterialData md = *PushConstants.md;

    float3 color = coarseVertex.color;
    float3 V = normalize(sceneData.cameraPosition.xyz - coarseVertex.vertex);
    float3 L = sceneData.sunlightDirection.xyz;
    float3 N = normalize(coarseVertex.normal);
    float3 H = normalize(L + V);

    float cosTheta = dot(L, N);
    float cosPhi = dot(H, N);

    float3 ambientLight = sceneData.ambientColor.xyz;
    float3 sunLight = sceneData.sunlightColor.xyz * max(cosTheta, 0.0);

    texture2D matImageTexture = imageTexture[NonUniformResourceIndex(md.color_texture_index)];
    SamplerState matSampler = imageSampler[NonUniformResourceIndex(md.color_sampler_index)];
    float4 imageTextureColor = matImageTexture.Sample(matSampler, coarseVertex.textureCoordinates);

    float4 outFragColor = imageTextureColor * float4(ambientLight + sunLight, 1.0);

    Fragment output;
    output.color = outFragColor;
    return output;
}