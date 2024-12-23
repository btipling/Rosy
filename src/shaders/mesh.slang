
struct InputVertex {
    float3 position;
    float textureCoordinates_x;
    float3 normal;
    float textureCoordinates_y;
    float4 color;
};

struct CoarseVertex
{
    float3 color;
    float3 normal;
    float3 vertex;
    float2 textureCoordinates;
};

struct SceneData
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewproj;
    float4x4 shadowproj;
    float4 cameraPosition;
    float4 ambientColor;
    float4 sunlightDirection;
    float4 sunlightColor;
};

struct VertexStageOutput
{
    CoarseVertex coarseVertex : CoarseVertex;
    float4 sv_position : SV_Position;
};

StructuredBuffer<InputVertex, Std430DataLayout> VertexBuffer;
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;

struct PushConstant {
    float4x4 worldMatrix;
    InputVertex* v;
}
[[vk::push_constant]] PushConstant PushConstants;

[shader("vertex")]
VertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    VertexStageOutput output;
    InputVertex v = *(PushConstants.v + uiVertexId);

    float3x3 worldMat3 = (float3x3)PushConstants.worldMatrix;


    output.coarseVertex.vertex = v.position;
    output.coarseVertex.color = v.color.xyz;
    output.coarseVertex.normal = mul(worldMat3, v.normal);
    output.coarseVertex.textureCoordinates = float2(v.textureCoordinates_x, v.textureCoordinates_y);
    float4 posWorld = float4(v.position, 1.0);
    output.sv_position = mul(mul(sceneData.viewproj, PushConstants.worldMatrix), posWorld);

    return output;
}

struct Fragment
{
    float4 color;
};

[[vk::binding(0, 1)]] texture2D imageTexture[];
[[vk::binding(1, 1)]] SamplerState imageSampler[];

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