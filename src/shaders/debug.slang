#version 460
#extension GL_EXT_buffer_reference : require

struct DebugVertex
{
    float3 color;
};

struct DebugVertexStageOutput
{
    DebugVertex debugVertex : DebugVertex;
    float4 sv_position : SV_Position;
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
[[vk::binding(0, 0)]] ConstantBuffer<SceneData> sceneData;

struct DebugConstant {
    mat4 worldMatrix;
    float4 p1;
    float4 p2;
    float4 color;
}
[[vk::push_constant]] DebugConstant DebugConstants;

[shader("vertex")]
DebugVertexStageOutput debugVertMain(uint uiVertexId: SV_VertexID)
{
    float4 v = DebugConstants.p1;
    if (uiVertexId != 0) {
        v = DebugConstants.p2;
    }
    DebugVertexStageOutput output;
    output.debugVertex.color = DebugConstants.color.xyz;
    output.sv_position = mul(mul(sceneData.viewproj, DebugConstants.worldMatrix), v);

    return output;
}

struct Fragment
{
    float4 color;
};

[shader("fragment")]
Fragment debugFragmentMain(DebugVertex debugVertex: DebugVertex) : SV_Target
{
    Fragment output;
    output.color = float4(debugVertex.color, 1.0);
    return output;
}