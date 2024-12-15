#version 460
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 fragmentColor;
layout (location = 1) out vec3 fragmentNormal;
layout (location = 2) out vec3 fragmentVertex;
layout (location = 3) out vec2 textureCoordinates;

layout(set = 0, binding = 0) uniform  SceneData{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 shadowproj;
	vec4 cameraPosition;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
} sceneData;

struct Vertex {
	vec3 position;
	float textureCoordinates_x;
	vec3 normal;
	float textureCoordinates_y;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout( push_constant ) uniform constants
{
	mat4 worldMatrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() {
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = sceneData.viewproj * PushConstants.worldMatrix * vec4(v.position, 1.0);
    fragmentColor = v.color.xyz;
	mat3 normalTransform = transpose(inverse(mat3(PushConstants.worldMatrix)));
	fragmentNormal = normalize(normalTransform * v.normal);
	fragmentVertex = v.position;
	textureCoordinates = vec2(v.textureCoordinates_x, v.textureCoordinates_y);
}
