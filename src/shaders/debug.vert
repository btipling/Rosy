#version 460
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 fragmentColor;

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


layout( push_constant ) uniform constants
{
	mat4 worldMatrix;
	vec4 color;
} PushConstants;

void main() {
	vec4 p1 = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 p2 = vec4(1.0, 0.0, 0.0, 1.0);

	vec4 v = p1;
	if (gl_VertexIndex != 0) {
		v = p2;
	}

    gl_Position = sceneData.viewproj * PushConstants.worldMatrix * v;
    fragmentColor = PushConstants.color.xyz;
	mat3 normalTransform = transpose(inverse(mat3(PushConstants.worldMatrix)));
}
