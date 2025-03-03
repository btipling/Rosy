module data;

public struct BasicVertexStageOutput
{
    public BasicVertex basicVertex : BasicVertex;
    public CSM csm : CSM;
    public float4 sv_position : SV_Position;
};

public struct BasicVertex
{
    public float4 vertex;
    public float3 normal;
    public float3 worldNormal;
    public float3 viewDir;
    public float3 lightDir;
    public float4 tangent;
    public float3 bitangent;
    public float4 color;
    public float2 textureCoordinates;
};

public struct BasicInputVertex {
    public float3 position;
    public float3 normal;
    public float4 tangent;
    public float4 color;
    public float2 textureCoordinates;
};

public struct BasicGraphicsData {
    public float4x4 transform;
    public float4x4 toObjectSpaceTransform;
    public float3x3 normalTransform;
};

public struct CSM
{
    public float4 near;
    //public float4 middle;
    //public float4 far;
};

public struct SceneData
{
    public float4x4 view;
    public float4x4 proj;
    public float4x4 viewproj;
    public float4x4 shadowProjNear;
    public float4 sunlight;
    public float4 cameraPosition;
    public float4 ambientColor;
    public float4 sunlightColor;
    public uint4 flipLights;
    public uint4 flipTangents;
    public float2 drawExtent;
    public uint csmIndexSampler;
    public uint csmIndexNear;
    public uint fragmentOutput;
    public uint lightEnabled;
    public uint tangentSpaceEnabled;
    public uint shadowsEnabled;
    public uint normalMapsEnabled;
    public uint inverseBNT;
    public uint ignoreAssetTangentSign;
    public uint ensureOrthogonalBitangent;
    public uint reverseCrossBitangent;
};

public struct Fragment
{
    public float4 color;
};

public struct BasicMaterialData {
    public float4 color;
    public float metallicFactor;
    public float roughnessFactor;
    public float alphaCutoff;
    public uint32_t alphaMode;
    public uint32_t color_texture_index;
    public uint32_t color_sampler_index;
    public uint32_t normal_texture_index;
    public uint32_t normal_sampler_index;
    public uint32_t metallic_texture_index;
    public uint32_t metallic_sampler_index;
};

public struct BasicPushConstant {
    public SceneData *sd;
    public BasicInputVertex *v;
    public BasicGraphicsData *gd;
    public BasicMaterialData *md;
}

public struct ShadowConstant {
    public SceneData *sd;
    public BasicInputVertex *v;
    public BasicGraphicsData *gd;
    public uint pass_number;
}

public struct ShadowOutput
{
    public float4 sv_position : SV_Position;
};

public struct DebugVertex
{
   public float3 color;
};

public struct DebugVertexStageOutput
{
    public DebugVertex debugVertex : DebugVertex;
    public float4 sv_position : SV_Position;
};

public struct DebugInputVertex {
    public float4 position;
};

public struct DebugConstant {
    public float4x4 transform;
    public float4 color;
    public uint flags;
    public SceneData *sd;
    public DebugInputVertex *v;
}

public bool validDescriptorIndex(uint32_t index) {
    return index < 4294967295;
}
