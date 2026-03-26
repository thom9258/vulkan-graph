#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout (set = 0, binding = 0)
uniform Info
{
	mat4 view;
	mat4 proj;
	mat4 model;
} info;


void main() {
    mat4 transform = info.proj * info.view * info.model;
    gl_Position = transform * vec4(inPosition, 1.0);
    fragColor = inColor;
}
