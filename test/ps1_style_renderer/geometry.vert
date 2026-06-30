#version 450

#include "ps1.utility"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexcoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texcoord;

layout (set = 0, binding = 0)
uniform Info
{
	mat4 view;
	mat4 proj;
	mat4 model;
} info;

void main() {
	const vec2 resolution = vec2(320.0, 240.0);
    const mat4 transform = info.proj * info.view * info.model;
	const vec4 position = transform * vec4(inPosition, 1.0f);
	gl_Position = ps1_low_precision(position, resolution);
    fragColor = inColor;
    texcoord = inTexcoord;
}
