
struct InputVertex {
    float3 position;
    float textureCoordinates_x;
    float3 normal;
    float textureCoordinates_y;
    float4 color;
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

struct ShadowOutput
{
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
ShadowOutput shadowVertexMain(uint uiVertexId: SV_VertexID)
{
    InputVertex v = *(PushConstants.v + uiVertexId);

    float3x3 worldMat3 = (float3x3)PushConstants.worldMatrix;

    ShadowOutput output;
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
void shadowFragmentMain() : SV_Target
{
}