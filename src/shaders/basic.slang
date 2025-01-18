import data;

[[vk::push_constant]] BasicPushConstant BasicPushConstants;

[shader("vertex")]
BasicVertexStageOutput vertexMain(uint uiVertexId: SV_VertexID)
{
    BasicInputVertex v = *(BasicPushConstants.v + uiVertexId);
    float4 posWorld = float4(v.position, 1.0);

    BasicVertexStageOutput output;

    output.basicVertex.normal = v.normal;

    output.sv_position = float4(v.position, 1.f);

    return output;
}

[shader("fragment")]
Fragment fragmentMain(BasicVertex basicVertex: BasicVertex) : SV_Target
{
    Fragment output;
    output.color = float4(basicVertex.normal * 0.5 + 0.5, 1.0);
    return output;
}
