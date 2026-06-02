#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexcoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) 
uniform sampler2D diffuse;

vec4 posterize(vec4 colour)
{
	float gamma = 0.6f;
	float num_colors = 32.0f;
	vec3 colour_out = colour.rgb;
	colour_out = pow(colour_out, vec3(gamma));
	colour_out = colour_out * num_colors;
	colour_out = floor(colour_out);
	colour_out = colour_out / num_colors;
	colour_out = pow(colour_out, vec3(1.0 / gamma));
	return vec4(colour_out,1.0);
}

void main() {
    vec3 diffuse = vec3(texture(diffuse, inTexcoord));
	vec3 finalColor = posterize(diffuse * inColor);

	outColor = texture(diffuse, inTexcoord);
}
