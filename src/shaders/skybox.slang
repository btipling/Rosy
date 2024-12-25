import data.data;

StructuredBuffer<MaterialData, ScalarDataLayout> MaterialDataBuffer;
[[vk::binding(0, 1)]] texture2D imageTexture[];
[[vk::binding(1, 1)]] SamplerState imageSampler[];

[[vk::push_constant]] PushConstant PushConstants;

[shader("fragment")]
Fragment fragmentMain(CoarseVertex coarseVertex: CoarseVertex) : SV_Target
{
    MaterialData md = *PushConstants.md;
    uint cti = md.color_texture_index;
    uint csi = md.color_sampler_index;

    texture2D matImageTexture = imageTexture[NonUniformResourceIndex(cti)];
    SamplerState matSampler = imageSampler[NonUniformResourceIndex(csi)];
    float4 outFragColor = matImageTexture.Sample(matSampler, coarseVertex.textureCoordinates);

    Fragment output;
    output.color = outFragColor;
    return output;
}