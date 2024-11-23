#version 450 core
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 fragColor;
layout (location = 1) out vec2 tcOut;

struct Vertex {
	vec4 position;
	vec4 normal;
	vec4 textureCoordinates;
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
    gl_Position = PushConstants.worldMatrix * v.position;
    fragColor = v.color.xyz;
}
