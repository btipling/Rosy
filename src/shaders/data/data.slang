module data;

public struct VertexStageOutput
{
    public CoarseVertex coarseVertex : CoarseVertex;
    public float4 sv_position : SV_Position;
};

public struct InputVertex {
    public float3 position;
    public float textureCoordinates_x;
    public float3 normal;
    public float textureCoordinates_y;
    public float4 color;
};

public struct RenderData {
    public float4x4 transform;
    public float3x3 normalTransform;
    public uint32_t4 material_data;
};

public struct MaterialData {
    public uint32_t color_texture_index;
    public uint32_t color_sampler_index;
};

public struct CoarseVertex
{
    public float3 color;
    public float3 normal;
    public float3 vertex;
    public float2 textureCoordinates;
};

public struct SceneData
{
    public float4x4 view;
    public float4x4 proj;
    public float4x4 viewproj;
    public float4x4 shadowproj_near;
    public float4x4 shadowproj_middle;
    public float4x4 shadowproj_far;
    public float4 cameraPosition;
    public float4 ambientColor;
    public float4 sunlightDirection;
    public float4 sunlightColor;
    uint csm_index_sampler;
    uint csm_index_near;
    uint csm_index_middle;
    uint csm_index_far;
};

public struct Fragment
{
    public float4 color;
};

public struct PushConstant {
    public SceneData *sd;
    public InputVertex *v;
    public RenderData *rd;
    public MaterialData *md;
}

public struct ShadowConstant {
    public SceneData *sd;
    public InputVertex *v;
    public RenderData *rd;
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

public struct DebugConstant {
    public float4x4 worldMatrix;
    public float4 p1;
    public float4 p2;
    public float4 color;
}