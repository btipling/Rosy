#version 460
#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform  SceneData{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
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
}