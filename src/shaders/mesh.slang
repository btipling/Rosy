import data.data;

StructuredBuffer<InputVertex, Std430DataLayout> VertexBuffer;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;
[[vk::binding(0, 1)]] texture2D imageTexture[];
[[vk::binding(1, 1)]] SamplerState imageSampler[];

[[vk::push_constant]] PushConstant PushConstants;

[shader("vertex")]
VertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    InputVertex v = *(PushConstants.v + uiVertexId);

    float3x3 worldMat3 = (float3x3)PushConstants.worldMatrix;

    VertexStageOutput output;
    output.coarseVertex.vertex = v.position;
    output.coarseVertex.color = v.color.xyz;
    output.coarseVertex.normal = mul(worldMat3, v.normal);
    output.coarseVertex.textureCoordinates = float2(v.textureCoordinates_x, v.textureCoordinates_y);
    float4 posWorld = float4(v.position, 1.0);
    output.sv_position = mul(mul(sceneData.viewproj, PushConstants.worldMatrix), posWorld);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target
{
    float3 color = coarseVertex.color;
    float3 V = normalize(sceneData.cameraPosition.xyz - coarseVertex.vertex);
    float3 L = sceneData.sunlightDirection.xyz;
    float3 N = normalize(coarseVertex.normal);
    float3 H = normalize(L + V);

    float cosTheta = dot(L, N);
    float cosPhi = dot(H, N);

    float3 ambientLight = sceneData.ambientColor.xyz;
    float3 sunLight = sceneData.sunlightColor.xyz * max(cosTheta, 0.0);
    float4 outFragColor = imageTexture[NonUniformResourceIndex(0)].Sample(imageSampler[NonUniformResourceIndex(0)], coarseVertex.textureCoordinates) * float4(ambientLight + sunLight, 1.0);

    Fragment output;
    output.color = outFragColor;
    return output;
}