import data.data;

[[vk::binding(0, 1)]] texture2D imageTexture[];
[[vk::binding(1, 1)]] SamplerState imageSampler[];

[shader("fragment")]
Fragment fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target
{
    float4 outFragColor = imageTexture[NonUniformResourceIndex(0)].Sample(imageSampler[NonUniformResourceIndex(0)], coarseVertex.textureCoordinates);

    Fragment output;
    output.color = outFragColor;
    return output;
}